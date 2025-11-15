/* ACCESS CONTROL COMMANDS
 * GRANT and REVOKE commands for managing file permissions
 *
 * GRANT <filename> <username> READ|WRITE
 *   - Gives the specified user read or write access to the file
 *   - Only the file owner can grant permissions
 *   - Communicates with Name Server ACL module (ns_acl.c)
 *
 * REVOKE <filename> <username> READ|WRITE
 *   - Removes the specified user's access from the file
 *   - Only the file owner can revoke permissions
 *   - Communicates with Name Server ACL module (ns_acl.c)
 *
 * Process:
 * 1. Parse command to extract filename, target username, and permission type
 * 2. Connect to Name Server
 * 3. Send GRANT/REVOKE request with requester's username (for owner verification)
 * 4. NS ACL module verifies requester is file owner
 * 5. NS ACL module updates permissions in ACL database
 * 6. Display success/error message to user
 */

#include "../include/client_access.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* GRANT COMMAND - Give user access to file
 * Syntax: GRANT <filename> <username> READ|WRITE
 * Returns: 0 on success, -1 on failure
 */
int handle_grant_command(const char *command, const char *username)
{
    char filename[256];
    char target_user[64];
    char permission[16];

    // Parse command: "GRANT <filename> <target_user> <permission>"
    if (sscanf(command, "GRANT %255s %63s %15s", filename, target_user, permission) != 3)
    {
        printf("Error: Invalid GRANT command format\n");
        printf("Usage: GRANT <filename> <username> READ|WRITE\n");
        return -1;
    }

    // Validate permission type
    if (strcmp(permission, "READ") != 0 && strcmp(permission, "WRITE") != 0)
    {
        printf("Error: Permission must be READ or WRITE\n");
        return -1;
    }

    printf("Granting %s permission on '%s' to user '%s'\n",
           permission, filename, target_user);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send GRANT request
    // Format: "GRANT <filename> <requester_username> <target_user> <permission>"
    char request[512];
    snprintf(request, sizeof(request), "GRANT %s %s %s %s\n",
             filename, username, target_user, permission);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send GRANT request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[512];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    // Parse response: "SUCCESS" or "ERROR <message>"
    if (strncmp(response, "SUCCESS", 7) == 0)
    {
        printf("Success: %s permission granted to '%s' on file '%s'\n",
               permission, target_user, filename);
    }
    else if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("Error: %s\n", response + 7);
        close(ns_fd);
        return -1;
    }
    else
    {
        printf("Unexpected response: %s\n", response);
        close(ns_fd);
        return -1;
    }

    close(ns_fd);
    return 0;
}

/* REVOKE COMMAND - Remove user access from file
 * Syntax: REVOKE <filename> <username> READ|WRITE
 * Returns: 0 on success, -1 on failure
 */
int handle_revoke_command(const char *command, const char *username)
{
    char filename[256];
    char target_user[64];
    char permission[16];

    // Parse command: "REVOKE <filename> <target_user> <permission>"
    if (sscanf(command, "REVOKE %255s %63s %15s", filename, target_user, permission) != 3)
    {
        printf("Error: Invalid REVOKE command format\n");
        printf("Usage: REVOKE <filename> <username> READ|WRITE\n");
        return -1;
    }

    // Validate permission type
    if (strcmp(permission, "READ") != 0 && strcmp(permission, "WRITE") != 0)
    {
        printf("Error: Permission must be READ or WRITE\n");
        return -1;
    }

    printf("Revoking %s permission on '%s' from user '%s'\n",
           permission, filename, target_user);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send REVOKE request
    // Format: "REVOKE <filename> <requester_username> <target_user> <permission>"
    char request[512];
    snprintf(request, sizeof(request), "REVOKE %s %s %s %s\n",
             filename, username, target_user, permission);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send REVOKE request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[512];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    // Parse response: "SUCCESS" or "ERROR <message>"
    if (strncmp(response, "SUCCESS", 7) == 0)
    {
        printf("Success: %s permission revoked from '%s' on file '%s'\n",
               permission, target_user, filename);
    }
    else if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("Error: %s\n", response + 7);
        close(ns_fd);
        return -1;
    }
    else
    {
        printf("Unexpected response: %s\n", response);
        close(ns_fd);
        return -1;
    }

    close(ns_fd);
    return 0;
}
