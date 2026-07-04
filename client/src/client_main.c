/*
 * CLIENT MAIN
 * Main entry point for Client application. Connects to Name Server,
 * implements command loop for user interaction.
 */

 // LLM GENERATED CODE STARTS HERE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../include/client_network.h"
#include "../include/client_commands.h"
#include "../../common/include/protocol.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <nameserver_ip>\n", argv[0]);
        return 1;
    }

    client_set_ns_ip(argv[1]);

    printf("=== Distributed File System Client ===\n");

    // Get username
    char username[32];
    while (1)
    {
        printf("Enter username: ");
        if (!fgets(username, sizeof(username), stdin))
        {
            fprintf(stderr, "Failed to read username. Please try again.\n");
            clearerr(stdin);
            continue;
        }

        // Remove newline
        username[strcspn(username, "\n")] = 0;

        // Trim leading whitespace
        char *start = username;
        while (*start && isspace((unsigned char)*start))
        {
            start++;
        }

        // Trim trailing whitespace
        char *end = start + strlen(start);
        while (end > start && isspace((unsigned char)*(end - 1)))
        {
            *(--end) = '\0';
        }

        if (start != username)
        {
            memmove(username, start, strlen(start) + 1);
        }

        if (strlen(username) == 0)
        {
            fprintf(stderr, "Username cannot be empty. Please try again.\n");
            continue;
        }

        break;
    }

    // Connect to Name Server
    int ns_fd = connect_to_ns_default();
    if (ns_fd < 0)
    {
        fprintf(stderr, "Failed to connect to Name Server\n");
        return 1;
    }

    char local_ip[INET_ADDRSTRLEN] = "0.0.0.0";
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(ns_fd, (struct sockaddr *)&local_addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));
    }

    // Register with Name Server
    char reg_msg[256];
    snprintf(reg_msg, sizeof(reg_msg), "REGISTER_CLIENT %s %s %d\n",
             username, local_ip, CLIENT_BASE_PORT);
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

// LLM GENERATED CODE ENDS HERE