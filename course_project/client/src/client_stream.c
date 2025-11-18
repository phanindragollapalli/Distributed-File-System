/* STREAM COMMAND - Person 2, Days 11-12
 * Stream file contents word-by-word with 0.1 second delay */

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

int handle_stream_command(const char *filename, const char *username)
{
    if (!filename || !username)
    {
        printf("Error: Invalid parameters\n");
        return -1;
    }

    printf("Streaming file: %s\n", filename);

    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    char request[512];
    snprintf(request, sizeof(request), "STREAM %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send STREAM request\n");
        close(ns_fd);
        return -1;
    }

    char response[1024];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("%s\n", response + 7);
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

    char ss_request[512];
    snprintf(ss_request, sizeof(ss_request), "STREAM %s\n", filename);
    send(ss_fd, ss_request, strlen(ss_request), 0);

    printf("\n--- Streaming: %s ---\n", filename);

    char buffer[256];
    int word_count = 0;
    int bytes;

    while ((bytes = recv(ss_fd, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytes] = '\0';

        if (word_count == 0 && strncmp(buffer, "ERROR", 5) == 0)
        {
            printf("%s\n", buffer + 7);
            close(ss_fd);
            return -1;
        }

        if (strcmp(buffer, "END_STREAM\n") == 0)
        {
            break;
        }

        printf("%s ", buffer);
        fflush(stdout);
        word_count++;
    }

    if (bytes == 0)
    {
        printf("\n\n✗ Error: Storage Server disconnected unexpectedly during streaming\n");
        close(ss_fd);
        return -1;
    }
    else if (bytes < 0)
    {
        if (errno == ECONNRESET || errno == EPIPE)
        {
            printf("\n\n✗ Error: Storage Server disconnected unexpectedly during streaming\n");
        }
        else
        {
            printf("\n\n✗ Error receiving data from Storage Server\n");
        }
        close(ss_fd);
        return -1;
    }

    printf("\n--- End of Stream ---\n");
    printf("Total words streamed: %d\n", word_count);

    close(ss_fd);
    return 0;
}
