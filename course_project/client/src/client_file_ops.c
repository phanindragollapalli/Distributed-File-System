#include "../include/client_file_ops.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* CREATE, DELETE, WRITE, APPEND COMMANDS - Person 2 Responsibility
 * These file operation commands are implemented by Person 2 as part of their file operations work
 * Person 1 is NOT responsible for implementing these commands
 */

/* CREATE command - Person 2, Days 3-4
 * Creates a new empty file on the Storage Server
 */
int handle_create_command(const char *filename, const char *username)
{
    printf("CREATE command framework ready for '%s'\n", filename);
    printf("NOTE: This is Person 2's responsibility (Days 3-4)\n");
    printf("Usage: CREATE <filename>\n");

    /* Person 2 should implement:
     * 1. Connect to NS
     * 2. Send CREATE request
     * 3. NS routes to appropriate SS
     * 4. SS creates empty file on disk
     * 5. Update NS metadata
     * 6. Return ACK to client
     */

    return 0;
}

/* DELETE command - Person 2, Day 10
 * Deletes a file from the Storage Server
 */
int handle_delete_command(const char *filename, const char *username)
{
    printf("DELETE command framework ready for '%s'\n", filename);
    printf("NOTE: This is Person 2's responsibility (Day 10)\n");
    printf("Usage: DELETE <filename>\n");

    /* Person 2 should implement:
     * 1. Check permissions using ACL
     * 2. Connect to NS
     * 3. Send DELETE request
     * 4. NS routes to SS
     * 5. SS removes file from disk
     * 6. Clear ACLs
     * 7. Update NS metadata
     * 8. Return ACK to client
     */

    return 0;
}
