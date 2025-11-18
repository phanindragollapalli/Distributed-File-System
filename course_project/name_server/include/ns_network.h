#ifndef NS_NETWORK_H
#define NS_NETWORK_H

int ns_server_init(int port);
int ns_accept_connection(int server_fd);

#endif
