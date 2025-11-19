/*
 * CLIENT MAIN
 *
 * Main entry point for Client application. Connects to Name Server,
 * implements command loop for user interaction.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/client_network.h"
#include "../include/client_commands.h"
#include "../../common/include/protocol.h"

int main()
{
    printf("=== Distributed File System Client ===\n");

    // Get username
    char username[32];
    printf("Enter username: ");
    if (fgets(username, sizeof(username), stdin))
    {
        // Remove newline
        username[strcspn(username, "\n")] = 0;
    }
    else
    {
        strcpy(username, "guest");
    }

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        fprintf(stderr, "Failed to connect to Name Server\n");
        return 1;
    }

    // Register with Name Server
    char reg_msg[256];
    snprintf(reg_msg, sizeof(reg_msg), "REGISTER_CLIENT %s 127.0.0.1 %d\n",
             username, CLIENT_BASE_PORT);
    write(ns_fd, reg_msg, strlen(reg_msg));

    // Wait for ACK
    char ack[128];
    int n = read(ns_fd, ack, sizeof(ack) - 1);
    if (n > 0)
    {
        ack[n] = '\0';
        if (strstr(ack, "ACK"))
        {
            printf("Successfully connected to Name Server as '%s'\n", username);
        }
    }

    close(ns_fd);

    // Command loop
    printf("\nAvailable commands:\n");
    printf("VIEW [-a] [-l] [-al]           - List files\n");
    printf("LIST                           - List all users\n");
    printf("READ <filename>                - Display file contents\n");
    printf("INFO <filename>                - Show file metadata\n");
    printf("HELP                           - Show the list of all available commands\n");
    printf("ADDACCESS -R <file> <user>     - Grant read access\n");
    printf("ADDACCESS -W <file> <user>     - Grant write access\n");
    printf("REMACCESS <file> <user>        - Remove all access\n");
    printf("EXEC <filename>                - Execute script file\n");
    printf("CREATE <filename>              - Create new file\n");
    printf("DELETE <filename>              - Delete file\n");
    printf("WRITE <file> <sentence_idx>    - Write to file\n");
    printf("UNDO <filename>                - Undo last change\n");
    printf("STREAM <filename>              - Stream file contents\n");
    printf("EXIT                           - Quit client\n");
    printf("\n");

    char command[512];
    while (1)
    {
        printf("%s> ", username);
        if (!fgets(command, sizeof(command), stdin))
        {
            break;
        }

        // Remove newline
        command[strcspn(command, "\n")] = 0;

        if (strlen(command) == 0)
        {
            continue;
        }

        if (strcmp(command, "HELP") == 0 || strcmp(command, "help") == 0)
        {
            printf("Available commands:\n");
            printf("VIEW [-a] [-l] [-al]           - List files\n");
            printf("LIST                           - List all users\n");
            printf("READ <filename>                - Display file contents\n");
            printf("INFO <filename>                - Show file metadata\n");
            printf("HELP                           - Show the list of all available commands\n");
            printf("ADDACCESS -R <file> <user>     - Grant read access\n");
            printf("ADDACCESS -W <file> <user>     - Grant write access\n");
            printf("REMACCESS <file> <user>        - Remove all access\n");
            printf("EXEC <filename>                - Execute script file\n");
            printf("CREATE <filename>              - Create new file\n");
            printf("DELETE <filename>              - Delete file\n");
            printf("WRITE <file> <sentence_idx>    - Write to file\n");
            printf("UNDO <filename>                - Undo last change\n");
            printf("STREAM <filename>              - Stream file contents\n");
            printf("EXIT                           - Quit client\n");
            continue;
        }

        if (strcmp(command, "EXIT") == 0 || strcmp(command, "exit") == 0)
        {
            break;
        }

        // Execute command
        execute_command(command, username);
    }

    printf("Goodbye!\n");
    return 0;
}
