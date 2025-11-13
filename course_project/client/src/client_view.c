#include "../include/client_view.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* VIEW COMMAND - Person 1, Day 8-9
 * Displays the contents of a file to the user
 * Process:
 * 1. Send VIEW request to Name Server with filename and username
 * 2. NS checks ACL permissions (user must have READ access)
 * 3. NS returns Storage Server IP and port
 * 4. Client connects to Storage Server
 * 5. Request file contents from SS
 * 6. Display contents to user
 */

int handle_view_command(const char *command, const char *username)
{
    // Parse command: "VIEW <filename>"
    char filename[256];
    if (sscanf(command, "VIEW %255s", filename) != 1)
    {
        printf("Error: Invalid VIEW command format\n");
        printf("Usage: VIEW <filename>\n");
        return -1;
    }

    printf("Viewing file: %s\n", filename);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send VIEW request with username for ACL check
    char request[512];
    snprintf(request, sizeof(request), "VIEW %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send VIEW request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[1024];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    // Parse response: "SS_INFO <ip> <port>" or "ERROR <message>"
    if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("%s\n", response + 7); // Print error message
        close(ns_fd);
        return -1;
    }

    char ss_ip[32];
    int ss_port;
    if (sscanf(response, "SS_INFO %31s %d", ss_ip, &ss_port) != 2)
    {
        printf("Error: Invalid response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    close(ns_fd);

    // Connect to Storage Server
    int ss_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_fd < 0)
    {
        perror("Error creating socket for Storage Server");
        return -1;
    }

    struct sockaddr_in ss_addr;
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);
    inet_pton(AF_INET, ss_ip, &ss_addr.sin_addr);

    if (connect(ss_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        perror("Error connecting to Storage Server");
        close(ss_fd);
        return -1;
    }

    // Request file contents from Storage Server
    char ss_request[512];
    snprintf(ss_request, sizeof(ss_request), "READ %s\n", filename);
    send(ss_fd, ss_request, strlen(ss_request), 0);

    // Receive and display file contents
    printf("\n--- File Contents: %s ---\n", filename);

    char buffer[4096];
    int total_bytes = 0;
    while (1)
    {
        int n = recv(ss_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
            break;

        buffer[n] = '\0';

        // Check for error response
        if (total_bytes == 0 && strncmp(buffer, "ERROR", 5) == 0)
        {
            printf("%s\n", buffer + 7);
            close(ss_fd);
            return -1;
        }

        printf("%s", buffer);
        total_bytes += n;

        // If we received less than buffer size, we're done
        if (n < sizeof(buffer) - 1)
            break;
    }

    printf("\n--- End of File ---\n");
    printf("Total bytes read: %d\n", total_bytes);

    close(ss_fd);
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