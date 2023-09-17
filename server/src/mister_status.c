#define _POSIX_C_SOURCE 200809L
#include "error.h"
#include "helpers.h"
#include "network.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PORT 41212
#define MAX_CONNECTIONS 10
#define CORE_BUFFER_SIZE 100
#define BUFFER_SIZE 500
#define MAX_SUBFOLDERS 2000
#define COREPATH "/tmp/CORENAME"
#define BASE_GAMEPATH "/media/fat/games/"

typedef struct inotify_event inotify_event;
typedef struct dirent dirent;

enum
{
    SOCKET_FD,
    NOTIFY_FD,
    NUM_FDS
};

char current_core[CORE_BUFFER_SIZE];
char current_game[BUFFER_SIZE];
char event_buffer[BUFFER_SIZE];
int game_wds[MAX_SUBFOLDERS];
int connection_fds[MAX_CONNECTIONS];
int core_wd;
struct pollfd fds[NUM_FDS];
time_t start_time;
bool running;

void int_handler(int signal)
{
    printf("Received %s, exiting...\n", strsignal(signal));
    running = false;
}

dirent *readdir_check(DIR *dir)
{
    errno = 0;
    dirent *file = readdir(dir);
    if (file == NULL && errno > 0)
    {
        perror("readdir failed");
        exit(1);
    }
    return file;
}

DIR *open_game_dir(const char path[])
{
    DIR *game_dir = opendir(path);
    if (game_dir == NULL)
    {
        if (errno == ENOENT)
        {
            return NULL;
        }
        else
        {
            perror("opendir failed");
            exit(1);
        }
    }
    return game_dir;
}

bool is_hidden_file(const dirent *file)
{
    return (file->d_name[0] == '.');
}

bool is_folder(const char path[])
{
    struct stat file_stat;
    int error = stat(path, &file_stat);
    error_check(error, "stat failed");
    return (file_stat.st_mode & S_IFDIR);
}

int process_folder(int notify_fd, char path[], int max_path_length, int index);
int process_folder(int notify_fd, char path[], int max_path_length, int index)
{
    int current_path_length = strlen(path);

    DIR *game_dir = open_game_dir(path);
    if (game_dir == NULL)
        return -1;

    game_wds[index] = inotify_add_watch(notify_fd, path, IN_ACCESS | IN_ONLYDIR);
    error_check(game_wds[index], "inotify_add_watch failed");

    dirent *file = readdir_check(game_dir);
    while (file != NULL)
    {
        if (!is_hidden_file(file))
        {
            int file_name_length = strlen(file->d_name);
            if (current_path_length + file_name_length + 1 >= max_path_length)
            {
                return -1;
            }
            sprintf(path + current_path_length, "/%s", file->d_name);
            if (is_folder(path))
            {
                index = process_folder(notify_fd, path, max_path_length, index + 1);
            }
        }
        file = readdir_check(game_dir);
    }

    int error = closedir(game_dir);
    error_check(error, "closedir failed");

    strcpy_safe(path + current_path_length, "", 1);
    return index;
}

void add_game_watches(int notify_fd, const char current_core[])
{
    char game_path[BUFFER_SIZE];
    sprintf(game_path, "%s%s", BASE_GAMEPATH, current_core);

    for (int i = 0; i < MAX_SUBFOLDERS && game_wds[i] >= 0; i++)
    {
        int error = inotify_rm_watch(notify_fd, game_wds[i]);
        error_check(error, "inotify_rm_watch failed");
        game_wds[i] = -1;
    }

    process_folder(notify_fd, game_path, sizeof(game_path), 0);
}

void get_core_name(const char path[], char *buffer)
{
    int file_fd = open(path, O_RDONLY);
    error_check(file_fd, "open failed");

    ssize_t bytes_read = read(file_fd, buffer, BUFFER_SIZE);
    error_check((int)bytes_read, "read failed");
    buffer[bytes_read] = '\0';

    int error = close(file_fd);
    error_check(error, "close failed");
}

ssize_t read_events(char *buffer)
{
    ssize_t bytes_read = read(fds[NOTIFY_FD].fd, buffer, BUFFER_SIZE);
    error_check((int)bytes_read, "event read failed");
    return bytes_read;
}

ssize_t make_status_message(char *buffer)
{
    ssize_t length = 0;
    time_t current_time = time(NULL);
    if (current_time >= (time_t)(0) && start_time >= (time_t)(0))
    {
        time_t elapsed = current_time - start_time;
        length = sprintf(buffer, "%s/%s/%ld", current_core, current_game, elapsed);
    }
    else
    {
        length = sprintf(buffer, "%s/%s/", current_core, current_game);
    }

    return length;
}

void broadcast_status(void)
{
    char buffer[BUFFER_SIZE];

    ssize_t msg_length = make_status_message(buffer);
    network_broadcast(buffer, msg_length);
}

void switch_to_core(const char new_core[])
{
    strcpy_safe(current_core, new_core, sizeof(current_core));
    strcpy(current_game, "");
    start_time = (time_t)(-1);
    printf("Switched to %s\n", current_core);
    add_game_watches(fds[NOTIFY_FD].fd, current_core);
}

void process_event(const inotify_event *event)
{
    if (event->wd == core_wd)
    {
        char new_core[CORE_BUFFER_SIZE];
        get_core_name(COREPATH, new_core);
        if (new_core[0] != '\0' && strcmp(current_core, new_core) != 0)
        {
            switch_to_core(new_core);
            broadcast_status();
        }
    }
    else if (event->len > 1 && !(event->mask & IN_ISDIR))
    {
        const char *new_game = event->name;
        if (strcmp(current_game, new_game) != 0)
        {
            strcpy_safe(current_game, new_game, sizeof(current_game));
            printf("Started playing %s\n", current_game);
            start_time = time(NULL);
            if (start_time == (time_t)(-1))
            {
                puts("process_event: getting time failed");
            }
            broadcast_status();
        }
    }
}

void add_interrupt_handler(void)
{
    running = true;

    struct sigaction action;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    action.sa_handler = int_handler;

    int error = sigaction(SIGINT, &action, NULL);
    error_check(error, "sigaction failed");
}

void initialise(void)
{
    add_interrupt_handler();

    fds[NOTIFY_FD].fd = inotify_init();
    error_check(fds[NOTIFY_FD].fd, "inotify_init failed");
    fds[NOTIFY_FD].events = POLLIN;

    for (int i = 0; i < MAX_SUBFOLDERS; i++)
    {
        game_wds[i] = -1;
    }
    core_wd = inotify_add_watch(fds[NOTIFY_FD].fd, COREPATH, IN_MODIFY);
    error_check(core_wd, "inotify_add_watch failed");

    get_core_name(COREPATH, current_core);
    add_game_watches(fds[NOTIFY_FD].fd, current_core);
    strcpy(current_game, "");

    start_time = (time_t)(-1);

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        connection_fds[i] = -1;
    }

    fds[SOCKET_FD].fd = network_init(PORT);
    fds[SOCKET_FD].events = POLLIN;
}

void finalise(void)
{
    int error = close(fds[NOTIFY_FD].fd);
    error_check(error, "close failed");

    error = close(fds[SOCKET_FD].fd);
    error_check(error, "close failed");
}

int main(void)
{
    initialise();

    while (running)
    {
        int error = poll(fds, NUM_FDS, -1);
        error_check(error, "poll failed");

        if (fds[NOTIFY_FD].revents & POLLIN)
        {
            ssize_t read_size = read_events(event_buffer);
            ssize_t bytes_processed = 0;
            while (bytes_processed < read_size)
            {
                const inotify_event *event = (inotify_event *)(event_buffer + bytes_processed);
                process_event(event);
                ssize_t event_size = (sizeof(inotify_event) + event->len);
                bytes_processed += event_size;
            }
        }

        if (fds[SOCKET_FD].revents & POLLIN)
        {
            network_accept();
            broadcast_status();
        }
    }

    finalise();

    return EXIT_SUCCESS;
}
