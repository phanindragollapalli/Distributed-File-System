/*
 * CLIENT NETWORK MODULE
 * Handles TCP socket connections from Client to Name Server and Storage Servers.
 * Provides connection establishment and socket management.
 */

#include "client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
// LLM GENERATED CODE STARTS HERE
// Connect to Name Server
static char g_ns_ip[INET_ADDRSTRLEN] = "127.0.0.1";

void client_set_ns_ip(const char *ns_ip)
{
    if (ns_ip && strlen(ns_ip) > 0)
    {
        strncpy(g_ns_ip, ns_ip, sizeof(g_ns_ip) - 1);
        g_ns_ip[sizeof(g_ns_ip) - 1] = '\0';
    }
}

const char *client_get_ns_ip()
{
    return g_ns_ip;
}

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

int connect_to_ns_default()
{
    return connect_to_ns(client_get_ns_ip(), NS_PORT);
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

// LLM GENERATED CODE ENDS HERE