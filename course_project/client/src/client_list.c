#include "../include/client_list.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// LIST command - Person 1 provides basic framework (Day 1-7)
// Full implementation by Person 2 (Days 8-9)
// Framework: Basic structure to send LIST request to NS
int handle_list_command(const char *username)
{
    printf("LIST command framework ready\n");
    printf("TODO (Person 2 - Days 8-9): Implement full LIST functionality\n");
    printf("Usage: LIST (shows all connected users)\n");

    // Basic framework for Person 2 to build on:
    // 1. Connect to NS
    // 2. Send LIST request
    // 3. Parse response with user list
    // 4. Display formatted user list

    return 0;
}
