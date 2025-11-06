#include <stdio.h>
#include "../include/client_network.h"
#include "../../common/include/protocol.h"

int main()
{
    int ns_fd = client_connect_to_ns("127.0.0.1", NS_PORT);
    (void)ns_fd; // suppress unused variable warning
    // Send registration message, or further requests as needed
    return 0;
}
