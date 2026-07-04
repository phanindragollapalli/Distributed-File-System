#include "../include/client_view.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* VIEW COMMAND
 * Lists files available to the user
 * Process:
 * 1. Send VIEW request to Name Server with flags and username
 * 2. NS returns list of files based on flags:
 *    - No flags: files user has access to
 *    - -a: all files on system
 *    - -l: files with details (word count, char count, last access, owner)
 *    - -al/-la: all files with details
 * 3. Display formatted list to user
 */
// LLM GENERATED CODE STARTS HERE
int handle_view_command(const char *command, const char *username)
{
    // Parse command: "VIEW" or "VIEW -a" or "VIEW -l" or "VIEW -al"
    char flags[16] = "";
    sscanf(command, "VIEW %15s", flags);

    // Validate flags if present
    if (strlen(flags) > 0 && flags[0] == '-')
    {
        int valid = 1;
        for (int i = 1; flags[i] != '\0'; i++)
        {
            if (flags[i] != 'a' && flags[i] != 'l')
            {
                valid = 0;
                break;
            }
        }
        if (!valid)
        {
            printf("Error: Invalid VIEW flags\n");
            printf("Usage: VIEW [-a] [-l] [-al]\n");
            return -1;
        }
    }
    else if (strlen(flags) > 0 && flags[0] != '-')
    {
        // If there's a non-flag argument, it's an error
        printf("Error: Invalid VIEW command format\n");
        printf("Usage: VIEW [-a] [-l] [-al]\n");
        return -1;
    }

    // Connect to Name Server
    int ns_fd = connect_to_ns_default();
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send VIEW request with flags and username
    char request[512];
    if (strlen(flags) > 0)
    {
        snprintf(request, sizeof(request), "VIEW %s %s\n", flags, username);
    }
    else
    {
        snprintf(request, sizeof(request), "VIEW %s\n", username);
    }

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send VIEW request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS (may arrive in multiple chunks)
    char response[16384];
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
        if (n == 0)
        {
            break;
        }
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

    if (strncmp(response, "FILE_LIST", 9) == 0)
    {
        printf("\n=== Files You Can Access ===\n");
        printf("%-40s %-15s\n", "Filename", "Permissions");
        printf("%-40s %-15s\n", "----------------------------------------", "---------------");

        char *data = response + 9;
        if (*data == '\n')
            data++;

        int shown = 0;
        char *line = strtok(data, "\n");
        while (line)
        {
            char filename[256] = {0};
            char perms[64] = {0};
            if (sscanf(line, "%255s %63[^\n]", filename, perms) >= 1)
            {
                printf("%-40s %-15s\n", filename, perms[0] ? perms : "-");
                shown++;
            }
            line = strtok(NULL, "\n");
        }

        if (shown == 0)
        {
            printf("(No files match the current filters)\n");
        }
    }
    else if (strncmp(response, "FILE_DETAILS", 12) == 0)
    {
        printf("\n=== File Details ===\n");
        printf("%-25s %-12s %-10s %-20s %-12s\n",
               "Filename", "Owner", "Chars", "Last Access", "Permissions");
        printf("%-25s %-12s %-10s %-20s %-12s\n",
               "-------------------------", "------------", "----------",
               "--------------------", "------------");

        char *data = response + 12;
        if (*data == '\n')
            data++;

        int shown = 0;
        char *line = strtok(data, "\n");
        while (line)
        {
            char filename[256] = {0};
            char owner[64] = {0};
            char last_access[64] = {0};
            char perms[64] = {0};
            size_t char_count = 0;

            if (sscanf(line, "%255[^|]|%63[^|]|%zu|%63[^|]|%63[^\n]",
                       filename, owner, &char_count, last_access, perms) == 5)
            {
                printf("%-25s %-12s %-10zu %-20s %-12s\n",
                       filename,
                       owner[0] ? owner : "unknown",
                       char_count,
                       last_access[0] ? last_access : "N/A",
                       perms[0] ? perms : "-");
                shown++;
            }
            line = strtok(NULL, "\n");
        }

        if (shown == 0)
        {
            printf("(No files to display)\n");
        }
    }
    else if (strncmp(response, "NO_FILES", 8) == 0)
    {
        printf("No files found for the requested view.\n");
    }
    else
    {
        printf("%s", response);
    }

    close(ns_fd);
    return 0;
}

// Legacy function kept for compatibility
void handle_view(int ns_sock, char *cmd)
{
    char msg[128];
    sprintf(msg, "VIEW:%s", cmd + 4);

    send_to_ns(ns_sock, msg);

    char buffer[4096];
    recv_from_ns(ns_sock, buffer, sizeof(buffer));

    printf("%s\n", buffer);
}
// LLM GENERATED CODE ENDS HERE