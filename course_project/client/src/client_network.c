/*
 * CLIENT NETWORK MODULE
 * Person 1, Days 1-2
 *
 * Handles TCP socket connections from Client to Name Server and Storage Servers.
 * Provides connection establishment and socket management.
 */

#include "client_network.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Connect to Name Server
int connect_to_ns(const char *ns_ip, int ns_port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Client: socket failed");
        return -1;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ns_port);

    if (inet_pton(AF_INET, ns_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid NS IP address");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Client: NS connection failed");
        return -1;
    }

    return sockfd;
}

int send_to_ns(int sockfd, const char *msg)
{
    return send(sockfd, msg, strlen(msg), 0);
}

int recv_from_ns(int sockfd, char *buffer, int size)
{
    memset(buffer, 0, size);
    return recv(sockfd, buffer, size - 1, 0);
}
