/*
 * CLIENT COMMAND DISPATCHER
 * Person 1, Days 1-7
 *
 * Parses user commands and routes them to appropriate handlers.
 * Provides the main command processing infrastructure.
 */

#include "../include/client_commands.h"
#include "../include/client_view.h"
#include "../include/client_list.h"
#include "../include/client_info.h"
#include "../include/client_exec.h"
#include "../include/client_file_ops.h"
#include "../include/client_access.h"
#include "../include/client_network.h"
#include <stdio.h>
#include <string.h>

// Execute a command (router to specific handlers)
int execute_command(const char *command, const char *username)
{
    if (!command || strlen(command) == 0)
    {
        return -1;
    }

    // Parse command
    char cmd[64];
    sscanf(command, "%63s", cmd);

    // Person 1 Commands - Days 8-9: VIEW, LIST, INFO
    if (strncmp(cmd, "VIEW", 4) == 0)
    {
        return handle_view_command(command, username);
    }
    else if (strcmp(cmd, "LIST") == 0)
    {
        return handle_list_command(username);
    }
    else if (strcmp(cmd, "INFO") == 0)
    {
        char filename[256];
        if (sscanf(command, "INFO %255s", filename) == 1)
        {
            return handle_info_command(filename, username);
        }
        else
        {
            printf("Usage: INFO <filename>\n");
            return -1;
        }
    }
    // Person 1 Commands - Days 10-11: GRANT, REVOKE
    else if (strcmp(cmd, "GRANT") == 0)
    {
        return handle_grant_command(command, username);
    }
    else if (strcmp(cmd, "REVOKE") == 0)
    {
        return handle_revoke_command(command, username);
    }
    // Person 1 Command - Day 12: EXEC
    else if (strcmp(cmd, "EXEC") == 0)
    {
        return handle_exec_command(command, username);
    }
    // Person 2 Commands: CREATE, DELETE, READ, WRITE
    else if (strcmp(cmd, "CREATE") == 0)
    {
        char filename[256];
        if (sscanf(command, "CREATE %255s", filename) == 1)
        {
            return handle_create_command(filename, username);
        }
        else
        {
            printf("Usage: CREATE <filename>\n");
            return -1;
        }
    }
    else if (strcmp(cmd, "DELETE") == 0)
    {
        char filename[256];
        if (sscanf(command, "DELETE %255s", filename) == 1)
        {
            return handle_delete_command(filename, username);
        }
        else
        {
            printf("Usage: DELETE <filename>\n");
            return -1;
        }
    }
    else
    {
        printf("Unknown command: %s\n", cmd);
        printf("\nAvailable commands:\n");
        printf("  VIEW <filename>              - Display file contents\n");
        printf("  LIST                         - List accessible files\n");
        printf("  INFO <filename>              - Show file metadata\n");
        printf("  GRANT <file> <user> READ|WRITE  - Grant permissions\n");
        printf("  REVOKE <file> <user> READ|WRITE - Revoke permissions\n");
        printf("  EXEC <filename>              - Execute script file\n");
        printf("  CREATE <filename>            - Create new file\n");
        printf("  DELETE <filename>            - Delete file\n");
        printf("  EXIT                         - Quit client\n");
        return -1;
    }

    return 0;
}

// Legacy function for direct NS communication
void process_client_command(int ns_sock, char *cmd)
{
    if (strncmp(cmd, "VIEW", 4) == 0)
    {
        handle_view(ns_sock, cmd);
    }
    else if (strncmp(cmd, "INFO", 4) == 0)
    {
        handle_info(ns_sock, cmd);
    }
    else if (strncmp(cmd, "LIST", 4) == 0)
    {
        handle_list(ns_sock);
    }
    else
    {
        printf("Unknown command.\n");
    }
}