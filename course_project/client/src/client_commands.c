#include "../include/client_commands.h"
#include "../include/client_view.h"
#include "../include/client_list.h"
#include "../include/client_info.h"
#include "../include/client_file_ops.h"
#include <stdio.h>
#include <string.h>

// Execute a command
int execute_command(const char *command, const char *username)
{
    if (!command || strlen(command) == 0)
    {
        return -1;
    }

    // Parse command
    char cmd[64];
    sscanf(command, "%63s", cmd);

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
        printf("Type 'EXIT' for help\n");
        return -1;
    }

    return 0;
}
