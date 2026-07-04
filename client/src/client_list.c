#include "../include/client_list.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

/* LIST COMMAND
 * Lists all users registered in the system
 * Process:
 * 1. Send LIST request to Name Server
 * 2. NS returns list of all registered usernames
 * 3. Display formatted list to user
 */

 // LLM GENERATED CODE STARTS HERE
int handle_list_command(const char *username)
{
    printf("Listing all users in the system...\n");

    // Connect to Name Server
    int ns_fd = connect_to_ns_default();
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send LIST request (no username needed for listing all users)
    char request[256];
    snprintf(request, sizeof(request), "LIST\n");

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send LIST request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS (may span multiple packets)
    char response[8192];
    int total_received = 0;
    while (total_received < (int)sizeof(response) - 1)
    {
        int n = recv(ns_fd,
                     response + total_received,
                     sizeof(response) - 1 - total_received,
                     0);
        if (n <= 0)
        {
            break;
        }
        total_received += n;
    }

    if (total_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    response[total_received] = '\0';

    // Check for error response
    if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("%s\n", response + 7);
        close(ns_fd);
        return -1;
    }

    // Parse and display user list
    // Expected format: "USER_LIST\n<username1>\n<username2>\n..."
    if (strncmp(response, "USER_LIST", 9) == 0)
    {
        printf("\n=== Registered Users ===\n");

        // Parse each line after USER_LIST header
        char *line = strtok(response + 10, "\n"); // Skip "USER_LIST\n"
        int user_count = 0;

        while (line != NULL && strlen(line) > 0)
        {
            printf("--> %s\n", line);
            user_count++;
            line = strtok(NULL, "\n");
        }

        printf("\nTotal users: %d\n", user_count);

        if (user_count == 0)
        {
            printf("No users registered in the system.\n");
        }
    }
    else if (strncmp(response, "NO_USERS", 8) == 0)
    {
        printf("No users found in the system.\n");
    }
    else
    {
        printf("Unexpected response from Name Server:\n%s\n", response);
    }

    close(ns_fd);
    return 0;
}

// Legacy function kept for compatibility
void handle_list(int ns_sock)
{
    send_to_ns(ns_sock, "LIST");

    char buffer[4096];
    recv_from_ns(ns_sock, buffer, sizeof(buffer));

    printf("%s\n", buffer);
}

// LLM GENERATED CODE ENDS HERE