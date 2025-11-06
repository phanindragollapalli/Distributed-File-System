#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "client_network.h"

int client_connect_to_ns(const char *ns_ip, int ns_port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in ns_addr;
    ns_addr.sin_family = AF_INET;
    ns_addr.sin_port = htons(ns_port);
    inet_pton(AF_INET, ns_ip, &ns_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&ns_addr, sizeof(ns_addr)) == 0)
    {
        printf("Client Connected to NS %s:%d\n", ns_ip, ns_port);
        return sock;
    }
    else
    {
        perror("Client Connection failed");
        return -1;
    }
}
