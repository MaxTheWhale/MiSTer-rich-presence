#define _POSIX_C_SOURCE 200809L
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>

#define PORT 41212
#define CORE_BUFFER_SIZE 100
#define BUFFER_SIZE 500
#define MAX_SUBFOLDERS 2000
#define COREPATH "/tmp/CORENAME"
#define BASE_GAMEPATH "/media/fat/games/"

typedef struct inotify_event inotify_event;
typedef struct dirent dirent;

char current_core[CORE_BUFFER_SIZE];
char current_game[BUFFER_SIZE];
char event_buffer[BUFFER_SIZE];
int game_wds[MAX_SUBFOLDERS];
int socket_fd, notify_fd, core_wd;
bool running;

sem_t game_sem, core_sem;
pthread_t tcp_thread;

void int_handler(int signal) {
    running = false;
}

void check_error(int error, const char message[]) {
    if (error < 0) {
        if (!running && errno == EINTR) return;
        perror(message);
        exit(EXIT_FAILURE);
    }
}

void check_pthread_error(int error, const char message[]) {
    if (error > 0) {
        errno = error;
        perror(message);
        exit(EXIT_FAILURE);
    }
}

dirent *readdir_check(DIR *dir) {
    errno = 0;
    dirent *file = readdir(dir);
    if (file == NULL && errno > 0) {
        perror("readdir failed");
        exit(1);
    }
    return file;
}

DIR *open_game_dir(const char path[]) {
    DIR *game_dir = opendir(path);
    if (game_dir == NULL) {
        if (errno == ENOENT) {
            return NULL;
        } else {
            perror("opendir failed");
            exit(1);
        }
    }
    return game_dir;
}

bool is_hidden_file(const dirent *file) {
    return (file->d_name[0] == '.');
}

bool is_folder(const char path[]) {
    struct stat file_stat;
    int error = stat(path, &file_stat);
    check_error(error, "stat failed");
    return (file_stat.st_mode & S_IFDIR);
}

int process_folder(int notify_fd, char path[], int index);
int process_folder(int notify_fd, char path[], int index) {
    char *sub_path = path + strlen(path);

    DIR *game_dir = open_game_dir(path);
    if (game_dir == NULL) return -1;

    game_wds[index] = inotify_add_watch(notify_fd, path, IN_ACCESS | IN_ONLYDIR);
    check_error(game_wds[index], "inotify_add_watch failed");

    dirent *file = readdir_check(game_dir);
    while (file != NULL) {
        if (!is_hidden_file(file)) {
            sprintf(sub_path, "/%s", file->d_name);
            if (is_folder(path)) {
                index = process_folder(notify_fd, path, index + 1);
            }
        }
        file = readdir_check(game_dir);
    }

    int error = closedir(game_dir);
    check_error(error, "closedir failed");

    strcpy(sub_path, "");
    return index;
}

void add_game_watches(int notify_fd, const char current_core[]) {

    char game_path[BUFFER_SIZE];
    sprintf(game_path, "%s%s", BASE_GAMEPATH, current_core);

    for (int i = 0; i < MAX_SUBFOLDERS && game_wds[i] >= 0; i++) {
        int error = inotify_rm_watch(notify_fd, game_wds[i]);
        check_error(error, "inotify_rm_watch failed");
        game_wds[i] = -1;
    }

    process_folder(notify_fd, game_path, 0);
}

void get_core_name(const char path[], char *buffer) {
    int file_fd = open(path, O_RDONLY);
    check_error(file_fd, "open failed");

    ssize_t bytes_read = read(file_fd, buffer, BUFFER_SIZE);
    check_error((int)bytes_read, "read failed");
    buffer[bytes_read] = '\0';

    int error = close(file_fd);
    check_error(error, "close failed");
}

ssize_t read_events(char *buffer) {
    ssize_t bytes_read = read(notify_fd, buffer, BUFFER_SIZE);
    check_error((int)bytes_read, "event read failed");
    return bytes_read;
}

void switch_to_core(const char new_core[]) {
    check_error(sem_wait(&core_sem), "sem_wait failed");
    strcpy(current_core, new_core);
    check_error(sem_post(&core_sem), "sem_post failed");
    printf("Switched to %s\n", current_core);
    add_game_watches(notify_fd, current_core);
}

void process_event(const inotify_event *event) {
    if (event->wd == core_wd) {
        char new_core[CORE_BUFFER_SIZE];
        get_core_name(COREPATH, new_core);
        if (new_core[0] != '\0' && strcmp(current_core, new_core) != 0) {
            switch_to_core(new_core);
        }
    } else if (event->len > 1 && !(event->mask & IN_ISDIR)) {
        const char *new_game = event->name;
        if (strcmp(current_game, new_game) != 0) {
            check_error(sem_wait(&game_sem), "sem_wait failed");
            strcpy(current_game, new_game);
            check_error(sem_post(&game_sem), "sem_post failed");
            printf("Started playing %s\n", current_game);
        }
    }
}

void add_interrupt_handler() {
    running = true;

    struct sigaction action;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    action.sa_handler = int_handler;

    int error = sigaction(SIGINT, &action, NULL);
    check_error(error, "sigaction failed");
}

int init_tcp(uint16_t port) {
    int socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    check_error(socket_fd, "socket failed");
    int reuse_address = 1;
    int error = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(int));
    check_error(error, "setsockopt failed");
  
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
  
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
  
    error = bind(socket_fd, (struct sockaddr *)&sa, sizeof(sa));
    check_error(error, "bind failed");
  
    error = listen(socket_fd, 10);
    check_error(error, "listen failed");

    return socket_fd;
}

void *send_status(void *arg) {
    pthread_detach(pthread_self());

    int connection_fd = *(int*)arg;
    free(arg);

    char buffer[BUFFER_SIZE];

    while (running) {
        ssize_t bytes_received = recv(connection_fd, buffer, BUFFER_SIZE, 0);
        check_error((int)bytes_received, "recv failed");
        buffer[bytes_received] = '\0';

        if (bytes_received == 0) {
            int error = close(connection_fd);
            check_error(error, "close failed");
            return NULL;
        }

        ssize_t bytes_sent = send(connection_fd, buffer, bytes_received, 0);
        check_error((int)bytes_sent, "send failed");
    }

    int error = close(connection_fd);
    check_error(error, "close failed");

    return NULL;
}

void *accept_connections(void *arg) {
    
    pthread_detach(pthread_self());
    pthread_t thread_id;
  
    while (running) {
        int connect_fd = accept(socket_fd, NULL, NULL);
        check_error(connect_fd, "accept failed");

        int *fd_ptr = malloc(sizeof(int));
        if (fd_ptr == NULL) {
            perror("malloc failed");
            return NULL;
        }
        *fd_ptr = connect_fd;

        int error = pthread_create(&thread_id, NULL, send_status, fd_ptr);
        check_pthread_error(error, "pthread_create failed");
    }

    return NULL;
}

pthread_t start_tcp_thread(int socket_fd) {
    pthread_t thread_id;

    int error = pthread_create(&thread_id, NULL, accept_connections, NULL);
    check_pthread_error(error, "pthread_create failed");

    return thread_id;
}

void initialise() {

    add_interrupt_handler();

    strcpy(current_core, "");
    strcpy(current_game, "");

    notify_fd = inotify_init();
    check_error(notify_fd, "inotify_init failed");

    for (int i = 0; i < MAX_SUBFOLDERS; i++) {
        game_wds[i] = -1;
    }
    core_wd = inotify_add_watch(notify_fd, COREPATH, IN_MODIFY);
    check_error(core_wd, "inotify_add_watch failed");

    int error = sem_init(&game_sem, 0, 1);
    check_error(error, "sem_init failed");
    error = sem_init(&core_sem, 0, 1);
    check_error(error, "sem_init failed");

    socket_fd = init_tcp(PORT);
    tcp_thread = start_tcp_thread(socket_fd);
}

void finalise() {
    int error = close(notify_fd);
    check_error(error, "close failed");

    error = sem_destroy(&game_sem);
    check_error(error, "sem_destroy failed");
    error = sem_destroy(&core_sem);
    check_error(error, "sem_destroy failed");

    error = close(socket_fd);
    check_error(error, "close failed");

    error = pthread_cancel(tcp_thread);
    check_pthread_error(error, "pthread_cancel failed");
}

int main() {

    initialise();

    while (running) {   
        ssize_t read_size = read_events(event_buffer);
        ssize_t bytes_processed = 0;
        while (bytes_processed < read_size) {
            const inotify_event *event = (inotify_event*)(event_buffer + bytes_processed);
            process_event(event);
            ssize_t event_size = (sizeof(inotify_event) + event->len);
            bytes_processed += event_size;
        }
    }

    finalise();

    return EXIT_SUCCESS;
}
