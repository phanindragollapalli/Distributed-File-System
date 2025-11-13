#include "../include/client_list.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* LIST COMMAND - Person 1, Day 8-9
 * Lists all accessible files for the user based on ACL permissions
 * Process:
 * 1. Send LIST request to Name Server with username
 * 2. NS queries ACL system to find files user can access (READ or WRITE)
 * 3. NS returns list of filenames with their permissions
 * 4. Display formatted list to user
 */

int handle_list_command(const char *username)
{
    printf("Listing accessible files for user: %s\n", username);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send LIST request with username for ACL filtering
    char request[256];
    snprintf(request, sizeof(request), "LIST %s\n", username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send LIST request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[4096];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    // Check for error response
    if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("%s\n", response + 7);
        close(ns_fd);
        return -1;
    }

    // Parse and display file list
    // Expected format: "FILE_LIST\n<filename1> <permissions>\n<filename2> <permissions>\n..."
    if (strncmp(response, "FILE_LIST", 9) == 0)
    {
        printf("\n=== Accessible Files ===\n");
        printf("%-40s %-15s\n", "Filename", "Permissions");
        printf("%-40s %-15s\n", "----------------------------------------", "---------------");

        // Parse each line after FILE_LIST header
        char *line = strtok(response + 10, "\n"); // Skip "FILE_LIST\n"
        int file_count = 0;

        while (line != NULL)
        {
            char filename[256];
            char permissions[32];

            // Parse line: "filename permissions"
            if (sscanf(line, "%255s %31s", filename, permissions) == 2)
            {
                printf("%-40s %-15s\n", filename, permissions);
                file_count++;
            }

            line = strtok(NULL, "\n");
        }

        printf("\nTotal files accessible: %d\n", file_count);

        if (file_count == 0)
        {
            printf("You don't have access to any files yet.\n");
        }
    }
    else if (strncmp(response, "NO_FILES", 8) == 0)
    {
        printf("No files found in the system or you don't have access to any files.\n");
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