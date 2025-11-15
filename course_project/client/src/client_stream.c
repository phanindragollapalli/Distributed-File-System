/* STREAM COMMAND
 * Stream file contents word-by-word with 0.1 second delay between words
 *
 * Flow:
 * 1. Client sends STREAM request to Name Server with filename
 * 2. NS checks ACL permissions (user must have READ access)
 * 3. NS returns Storage Server IP and port
 * 4. Client connects directly to Storage Server
 * 5. SS loads file, parses into words
 * 6. SS sends words one-by-one with 0.1 second delay
 * 7. Client displays words as they arrive (streaming effect)
 * 8. Handle SS disconnection gracefully with error message
 *
 * Usage: STREAM <filename>
 * Example: STREAM document.txt
 */

#include "../include/client_stream.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

/* STREAM COMMAND HANDLER
 * Connects to NS to get SS info, then streams file from SS word-by-word
 *
 * Parameters:
 *   command  - Full command string (e.g., "STREAM document.txt")
 *   username - Username for ACL check
 *
 * Returns: 0 on success, -1 on failure
 */
int handle_stream_command(const char *command, const char *username)
{
    // Parse command: "STREAM <filename>"
    char filename[256];
    if (sscanf(command, "STREAM %255s", filename) != 1)
    {
        printf("Error: Invalid STREAM command format\n");
        printf("Usage: STREAM <filename>\n");
        printf("Example: STREAM document.txt\n");
        return -1;
    }

    printf("Streaming file: %s\n", filename);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send STREAM request with username for ACL check
    // Format: "STREAM <filename> <username>\n"
    char request[512];
    snprintf(request, sizeof(request), "STREAM %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send STREAM request\n");
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
        printf("%s\n", response + 7); // Print error message (skip "ERROR: ")
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

    printf("Connecting to Storage Server at %s:%d...\n", ss_ip, ss_port);

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

    // Request file streaming from Storage Server
    // Format: "STREAM <filename>\n"
    char ss_request[512];
    snprintf(ss_request, sizeof(ss_request), "STREAM %s\n", filename);

    if (send(ss_fd, ss_request, strlen(ss_request), 0) < 0)
    {
        perror("Error sending STREAM request to Storage Server");
        close(ss_fd);
        return -1;
    }

    printf("\n--- Streaming Content: %s ---\n", filename);
    printf("(Words appear with 0.1 second delay)\n\n");

    // Receive and display streamed words
    char buffer[4096];
    int total_words = 0;
    int in_word = 0;
    char current_word[256];
    int word_idx = 0;

    while (1)
    {
        int n = recv(ss_fd, buffer, sizeof(buffer) - 1, 0);

        if (n < 0)
        {
            // Error receiving data
            if (errno == ECONNRESET || errno == EPIPE)
            {
                printf("\n\n[ERROR: Storage Server disconnected mid-stream]\n");
                printf("Stream interrupted. Only %d words were received.\n", total_words);
            }
            else
            {
                perror("\nError receiving stream data");
            }
            break;
        }

        if (n == 0)
        {
            // Connection closed gracefully
            // Print last word if any
            if (in_word && word_idx > 0)
            {
                current_word[word_idx] = '\0';
                printf("%s ", current_word);
                fflush(stdout);
                total_words++;
            }
            break;
        }

        buffer[n] = '\0';

        // Check for error response at start
        if (total_words == 0 && strncmp(buffer, "ERROR", 5) == 0)
        {
            printf("\n%s\n", buffer + 7);
            close(ss_fd);
            return -1;
        }

        // Parse and display words as they come
        // Words are already sent with 0.1s delay by SS, we just display them
        for (int i = 0; i < n; i++)
        {
            char c = buffer[i];

            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                // Word boundary
                if (in_word && word_idx > 0)
                {
                    current_word[word_idx] = '\0';
                    printf("%s ", current_word);
                    fflush(stdout);
                    total_words++;
                    word_idx = 0;
                    in_word = 0;
                }

                // Preserve newlines for formatting
                if (c == '\n')
                {
                    printf("\n");
                }
            }
            else
            {
                // Part of a word
                if (word_idx < sizeof(current_word) - 1)
                {
                    current_word[word_idx++] = c;
                    in_word = 1;
                }
            }
        }
    }

    printf("\n\n--- End of Stream ---\n");
    printf("Total words streamed: %d\n", total_words);

    close(ss_fd);
    return 0;
}
