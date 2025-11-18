#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

#include <arpa/inet.h>

#define MAX_BUFFER 4096

int connect_to_ns(const char *ns_ip, int ns_port);
int send_to_ns(int sockfd, const char *msg);
int recv_from_ns(int sockfd, char *buffer, int size);

#endif
