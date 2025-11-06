#include <stdio.h>
#include "../include/ss_network.h"
#include "../../common/include/protocol.h"

int main()
{
    int ns_fd = ss_connect_to_ns("127.0.0.1", NS_PORT);
    (void)ns_fd; // suppress unused variable warning
    // Register with NS, send hello
    return 0;
}
