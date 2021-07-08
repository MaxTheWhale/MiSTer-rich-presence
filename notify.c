#define _POSIX_C_SOURCE 200809L
#include <sys/inotify.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>

#define CORE_BUFFER_SIZE 100
#define BUFFER_SIZE 500
#define MAX_SUBFOLDERS 2000
#define COREPATH "/tmp/CORENAME"
#define BASE_GAMEPATH "/media/fat/games/"

typedef struct inotify_event inotify_event;

char current_core[CORE_BUFFER_SIZE];
char current_game[BUFFER_SIZE];
char event_buffer[BUFFER_SIZE];
int game_wds[MAX_SUBFOLDERS];
int notify_fd, core_wd;
bool running;

void int_handler(int signal) {
    running = false;
}

void check_error(int error, const char *message) {
    if (error < 0) {
        if (!running && errno == EINTR) return;
        perror(message);
        exit(1);
    }
}

struct dirent *readdir_check(DIR *dir) {
    errno = 0;
    struct dirent *file = readdir(dir);
    if (file == NULL && errno > 0) {
        perror("readdir failed");
        exit(1);
    }
    return file;
}

int add_game_watches(int notify_fd, char *path, int index);
int add_game_watches(int notify_fd, char *path, int index) {
    char *path_root = path + strlen(path);

    DIR *game_dir = opendir(path);
    if (game_dir == NULL) {
        if (errno == ENOENT) {
            printf("No games directory for %s\n", current_core);
            return -1;
        } else {
            perror("opendir failed");
            exit(1);
        }
    }

    game_wds[index] = inotify_add_watch(notify_fd, path, IN_ACCESS | IN_ONLYDIR);
    check_error(game_wds[index], "inotify_add_watch failed");

    struct dirent *dir = readdir_check(game_dir);
    struct stat file_stat;
    while (dir != NULL) {
        if (dir->d_name[0] != '.') {
            sprintf(path_root, "/%s", dir->d_name);
            int error = stat(path, &file_stat);
            check_error(error, "stat failed");
            if (file_stat.st_mode & S_IFDIR) {
                index = add_game_watches(notify_fd, path, index + 1);
            }
        }
        dir = readdir_check(game_dir);
    }

    int error = closedir(game_dir);
    check_error(error, "closedir failed");

    path_root[0] = '\0';
    return index;
}

void add_game_watch(int notify_fd, const char *current_core) {

    char game_path[500];
    sprintf(game_path, "%s%s", BASE_GAMEPATH, current_core);

    for (int i = 0; i < MAX_SUBFOLDERS && game_wds[i] >= 0; i++) {
        int error = inotify_rm_watch(notify_fd, game_wds[i]);
        check_error(error, "inotify_rm_watch failed");
        game_wds[i] = -1;
    }

    add_game_watches(notify_fd, game_path, 0);
}

void get_core_name(const char *path, char *buffer) {
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

void process_event(const inotify_event *event) {
    if (event->wd == core_wd) {
        char new_core[CORE_BUFFER_SIZE];
        get_core_name(COREPATH, new_core);

        if (new_core[0] != '\0' && strcmp(current_core, new_core) != 0) {
            strcpy(current_core, new_core);
            printf("Switched to %s\n", current_core);
            add_game_watch(notify_fd, current_core);
        }
    } else if (event->len > 1 && !(event->mask & IN_ISDIR)) {
        const char *new_game = event->name;
        if (strcmp(current_game, new_game) != 0) {
            strcpy(current_game, new_game);
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

void initialise() {

    add_interrupt_handler();

    current_core[0] = '\0';
    current_game[0] = '\0';

    notify_fd = inotify_init();
    check_error(notify_fd, "inotify_init failed");

    for (int i = 0; i < MAX_SUBFOLDERS; i++) {
        game_wds[i] = -1;
    }
    core_wd = inotify_add_watch(notify_fd, COREPATH, IN_MODIFY);
    check_error(core_wd, "inotify_add_watch failed");
}

void finalise() {
    int error = close(notify_fd);
    check_error(error, "close failed");
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

    return 0;
}
