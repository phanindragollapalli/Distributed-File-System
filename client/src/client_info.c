#include "../include/client_info.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* INFO COMMAND
 * Displays detailed file metadata including size, owner, permissions, timestamps
 * Process:
 * 1. Send INFO request to Name Server with filename and username
 * 2. NS checks if user has access via ACL
 * 3. NS retrieves file metadata from metadata storage
 * 4. Display formatted metadata to user
 */

 // LLM GENERATED CODE STARTS HERE
int handle_info_command(const char *filename, const char *username)
{
    printf("Getting info for file: %s\n", filename);

    // Connect to Name Server
    int ns_fd = connect_to_ns_default();
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send INFO request with filename and username
    char request[512];
    snprintf(request, sizeof(request), "INFO %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send INFO request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[2048];
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

    // Parse and display file metadata
    if (strncmp(response, "FILE_INFO", 9) == 0)
    {
        char file_name[256];
        char owner[64];
        long size;
        long created, modified;
        char accessed_str[64] = "Unknown";
        char permissions[32];
        int ss_id;
        char all_perms[1024] = "";

        // Parse the response line by line
        char *line = strtok(response + 10, "\n"); // Skip "FILE_INFO\n"
        int field = 0;

        while (line != NULL && field < 9)
        {
            switch (field)
            {
            case 0:
                strncpy(file_name, line, sizeof(file_name) - 1);
                break;
            case 1:
                strncpy(owner, line, sizeof(owner) - 1);
                break;
            case 2:
                size = atol(line);
                break;
            case 3:
                created = atol(line);
                break;
            case 4:
                modified = atol(line);
                break;
            case 5:
                strncpy(accessed_str, line, sizeof(accessed_str) - 1);
                accessed_str[sizeof(accessed_str) - 1] = '\0';
                break;
            case 6:
                strncpy(permissions, line, sizeof(permissions) - 1);
                break;
            case 7:
                ss_id = atoi(line);
                break;
            case 8:
                strncpy(all_perms, line, sizeof(all_perms) - 1);
                break;
            }
            field++;
            line = strtok(NULL, "\n");
        }

        // Display formatted information
        printf("\n=== File Information ===\n");
        printf("Filename:       %s\n", file_name);
        printf("Owner:          %s\n", owner);
        printf("Size:           %ld bytes (%.2f KB)\n", size, size / 1024.0);
        printf("Your Access:    %s\n", permissions);
        printf("All Access:     %s\n", all_perms);
        printf("Storage Server: SS_%d\n", ss_id);

        // Format timestamps
        if (created > 0)
        {
            time_t t = (time_t)created;
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&t));
            printf("Created:        %s\n", time_str);
        }

        if (modified > 0)
        {
            time_t t = (time_t)modified;
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&t));
            printf("Modified:       %s\n", time_str);
        }

        printf("Last Accessed:  %s\n", accessed_str);

        printf("========================\n");
    }
    else
    {
        printf("Unexpected response from Name Server:\n%s\n", response);
    }

    close(ns_fd);
    return 0;
}

// Legacy function kept for compatibility
void handle_info(int ns_sock, char *cmd)
{
    char filename[128];
    sscanf(cmd, "INFO %s", filename);

    char msg[256];
    sprintf(msg, "INFO:%s", filename);

    send_to_ns(ns_sock, msg);

    char buffer[4096];
    recv_from_ns(ns_sock, buffer, sizeof(buffer));

    printf("%s\n", buffer);
}

// LLM GENERATED CODE ENDS HERE