#define _POSIX_C_SOURCE 200809L
#include "network.h"
#include "error.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CONNECTIONS 10

int socket_fd;
int connection_fds[MAX_CONNECTIONS];

int network_init(uint16_t port)
{socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    error_check(socket_fd, "socket failed");
    int reuse_address = 1;
    int error = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(int));
    error_check(error, "setsockopt failed");

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    error = bind(socket_fd, (struct sockaddr *)&sa, sizeof(sa));
    error_check(error, "bind failed");

    error = listen(socket_fd, MAX_CONNECTIONS);
    error_check(error, "listen failed");

    return socket_fd;
}

void network_accept()
{
    int connection_fd = accept(socket_fd, NULL, NULL);
    error_check(connection_fd, "accept failed");

    int index = 0;
    while (connection_fds[index] >= 0 && index < MAX_CONNECTIONS)
        index++;
    if (index == MAX_CONNECTIONS)
    {
        puts("accept_connection failed: MAX_CONNECTIONS exceeded");
        exit(EXIT_FAILURE);
    }

    connection_fds[index] = connection_fd;
    int error = shutdown(connection_fd, SHUT_RD);
    error_check(error, "shutdown failed");
}

void network_broadcast(const void *message, size_t length)
{
    for (int index = 0; index < MAX_CONNECTIONS; index++)
    {
        if (connection_fds[index] >= 0)
        {
            ssize_t bytes_sent = send(connection_fds[index], message, length, MSG_NOSIGNAL);

            if (bytes_sent <= 0)
            {
                int error = close(connection_fds[index]);
                error_check(error, "close failed");
                connection_fds[index] = -1;
            }
        }
    }
}
