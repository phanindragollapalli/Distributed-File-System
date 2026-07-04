// LLM GENERATED CODE STARTS HERE
#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

#include <arpa/inet.h>

#define MAX_BUFFER 4096

void client_set_ns_ip(const char *ns_ip);
const char *client_get_ns_ip();

int connect_to_ns(const char *ns_ip, int ns_port);
int connect_to_ns_default();
int send_to_ns(int sockfd, const char *msg);
int recv_from_ns(int sockfd, char *buffer, int size);

#endif

// LLM GENERATED CODE ENDS HERE