/*
 * NAME SERVER NETWORK MODULE

 *
 * Handles TCP socket initialization and connection acceptance for Name Server.
 * Uses POSIX sockets API for network communication.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include "ns_network.h"

// Initialize Name Server socket and bind to specified port
int ns_server_init(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Failed to create socket");
        return -1;
    }

    // Set SO_REUSEADDR to allow quick restart
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Failed to set SO_REUSEADDR");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Failed to bind socket");
        return -1;
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("Failed to listen on socket");
        return -1;
    }

    return server_fd;
}

int ns_accept_connection(int server_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
    return client_fd;
}
