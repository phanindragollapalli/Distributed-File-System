#include "../include/client_info.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// INFO command - Person 1 provides basic framework (Day 1-7)
// Full implementation by Person 2 (Days 8-9)
// Framework: Basic structure to send INFO request to NS
int handle_info_command(const char *filename, const char *username)
{
    printf("INFO command framework ready for '%s'\n", filename);
    printf("TODO (Person 2 - Days 8-9): Implement full INFO functionality\n");
    printf("Usage: INFO <filename>\n");
    printf("Should display: size, owner, permissions, timestamps\n");

    // Basic framework for Person 2 to build on:
    // 1. Connect to NS
    // 2. Send INFO request for specific file
    // 3. Parse response with file metadata
    // 4. Display formatted file info (size, owner, permissions, timestamps)

    return 0;
}
