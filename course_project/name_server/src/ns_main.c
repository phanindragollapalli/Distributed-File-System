#include <stdio.h>
#include "../include/ns_network.h"
#include "../../common/include/protocol.h"

// #ifndef NS_PORT
// #define NS_PORT 12345
// #endif

int main()
{
    int server_fd = ns_server_init(NS_PORT);
    printf("Name Server running on port %d\n", NS_PORT);

    while (1)
    {
        int conn_fd = ns_accept_connection(server_fd);
        printf("Accepted connection: %d\n", conn_fd);
        // Handle message protocol here
    }
    return 0;
}
