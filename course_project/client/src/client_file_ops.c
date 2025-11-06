#include "../include/client_file_ops.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// CREATE command - Person 1 provides basic framework (Day 1-7)
// Full implementation by Person 2 (Days 3-4)
// Framework: Basic structure to send CREATE request to NS
int handle_create_command(const char *filename, const char *username)
{
    printf("CREATE command framework ready for '%s'\n", filename);
    printf("TODO (Person 2 - Days 3-4): Implement full CREATE functionality\n");
    printf("Usage: CREATE <filename>\n");

    // Basic framework for Person 2 to build on:
    // 1. Connect to NS
    // 2. Send CREATE request
    // 3. NS routes to appropriate SS
    // 4. SS creates empty file on disk
    // 5. Update NS metadata
    // 6. Return ACK to client

    return 0;
}

// DELETE command - Person 1 provides basic framework (Day 1-7)
// Full implementation by Person 2 (Day 10)
// Framework: Basic structure to send DELETE request to NS
int handle_delete_command(const char *filename, const char *username)
{
    printf("DELETE command framework ready for '%s'\n", filename);
    printf("TODO (Person 2 - Day 10): Implement full DELETE functionality\n");
    printf("Usage: DELETE <filename>\n");

    // Basic framework for Person 2 to build on:
    // 1. Check permissions (ACL)
    // 2. Connect to NS
    // 3. Send DELETE request
    // 4. NS routes to SS
    // 5. SS removes file from disk
    // 6. Clear ACLs
    // 7. Update NS metadata
    // 8. Return ACK to client

    return 0;
}
