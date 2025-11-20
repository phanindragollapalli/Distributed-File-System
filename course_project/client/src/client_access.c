/* ACCESS CONTROL COMMANDS
 * ADDACCESS and REMACCESS commands for managing file permissions
 *
 * ADDACCESS -R <filename> <username>
 *   - Gives the specified user read access to the file
 *   - Only the file owner can add access
 *   - Communicates with Name Server ACL module (ns_acl.c)
 *
 * ADDACCESS -W <filename> <username>
 *   - Gives the specified user write (and read) access to the file
 *   - Only the file owner can add access
 *   - Communicates with Name Server ACL module (ns_acl.c)
 *
 * REMACCESS <filename> <username>
 *   - Removes all access from the specified user
 *   - Only the file owner can remove access
 *   - Communicates with Name Server ACL module (ns_acl.c)
 *
 * Process:
 * 1. Parse command to extract filename, target username, and flag/permission
 * 2. Connect to Name Server
 * 3. Send ADDACCESS/REMACCESS request with requester's username (for owner verification)
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

/* ADDACCESS COMMAND - Give user access to file
 * Syntax: ADDACCESS -R <filename> <username> OR ADDACCESS -W <filename> <username>
 * Returns: 0 on success, -1 on failure
 */
int handle_grant_command(const char *command, const char *username)
{
    char flag[16];
    char filename[256];
    char target_user[64];

    // Parse command: "ADDACCESS -R/-W <filename> <target_user>"
    if (sscanf(command, "ADDACCESS %15s %255s %63s", flag, filename, target_user) != 3)
    {
        printf("Error: Invalid ADDACCESS command format\n");
        printf("Usage: ADDACCESS -R <filename> <username> OR ADDACCESS -W <filename> <username>\n");
        return -1;
    }

    // Validate flag
    if (strcmp(flag, "-R") != 0 && strcmp(flag, "-W") != 0)
    {
        printf("Error: Flag must be -R (read) or -W (write)\n");
        return -1;
    }

    // Determine permission type from flag
    const char *permission = (strcmp(flag, "-R") == 0) ? "READ" : "WRITE";

    printf("Adding %s access on '%s' to user '%s'\n",
           permission, filename, target_user);

    // Connect to Name Server
    int ns_fd = connect_to_ns_default();
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send ADDACCESS request
    // Format: "ADDACCESS <filename> <requester_username> <target_user> <permission>"
    char request[512];
    snprintf(request, sizeof(request), "ADDACCESS %s %s %s %s\n",
             filename, username, target_user, permission);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send ADDACCESS request\n");
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
        printf("Access granted successfully!\n");
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

/* REMACCESS COMMAND - Remove all user access from file
 * Syntax: REMACCESS <filename> <username>
 * Returns: 0 on success, -1 on failure
 */
int handle_revoke_command(const char *command, const char *username)
{
    char filename[256];
    char target_user[64];

    // Parse command: "REMACCESS <filename> <target_user>"
    if (sscanf(command, "REMACCESS %255s %63s", filename, target_user) != 2)
    {
        printf("Error: Invalid REMACCESS command format\n");
        printf("Usage: REMACCESS <filename> <username>\n");
        return -1;
    }

    printf("Removing access on '%s' from user '%s'\n",
           filename, target_user);

    // Connect to Name Server
    int ns_fd = connect_to_ns_default();
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send REMACCESS request
    // Format: "REMACCESS <filename> <requester_username> <target_user>"
    char request[512];
    snprintf(request, sizeof(request), "REMACCESS %s %s %s\n",
             filename, username, target_user);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send REMACCESS request\n");
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
        printf("Access removed successfully!\n");
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
