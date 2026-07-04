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

// LLM GENERATED CODE STARTS HERE
typedef struct
{
    char ip[32];
    int port;
    int ss_id;
} StreamTarget;

static int is_punctuation_only(const char *token)
{
    if (!token || *token == '\0')
    {
        return 1;
    }

    for (int i = 0; token[i] != '\0'; i++)
    {
        if (token[i] != '.' && token[i] != ',' && token[i] != '!' &&
            token[i] != '?' && token[i] != ';' && token[i] != ':')
        {
            return 0;
        }
    }
    return 1;
}

static int count_words_in_chunk(const char *chunk)
{
    if (!chunk)
    {
        return 0;
    }

    char temp[256];
    strncpy(temp, chunk, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    int count = 0;
    char *token = strtok(temp, " \t\n");
    while (token)
    {
        if (!is_punctuation_only(token))
        {
            count++;
        }
        token = strtok(NULL, " \t\n");
    }
    return count;
}

static int parse_ss_targets(char *response, StreamTarget *targets, int max_targets)
{
    int count = 0;
    char *saveptr;
    char *line = strtok_r(response, "\n", &saveptr);

    while (line && count < max_targets)
    {
        StreamTarget target = {.port = 0, .ss_id = -1};
        if (strncmp(line, "SS_INFO", 7) == 0)
        {
            if (sscanf(line, "SS_INFO %31s %d %d", target.ip, &target.port, &target.ss_id) < 2)
            {
                return 0;
            }
        }
        else if (strncmp(line, "ALT_SS", 6) == 0)
        {
            if (sscanf(line, "ALT_SS %31s %d %d", target.ip, &target.port, &target.ss_id) < 2)
            {
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }
        }
        else
        {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        if (target.port > 0)
        {
            targets[count++] = target;
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    return count;
}

static int stream_from_target(const StreamTarget *target, const char *filename, int *word_count)
{
    if (!target)
    {
        return -1;
    }

    int start_word = *word_count;

    int ss_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_fd < 0)
    {
        perror("Error creating socket for Storage Server");
        return 1;
    }

    struct sockaddr_in ss_addr;
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(target->port);
    inet_pton(AF_INET, target->ip, &ss_addr.sin_addr);

    if (connect(ss_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        perror("Error connecting to Storage Server");
        close(ss_fd);
        return 1;
    }

    char ss_request[512];
    snprintf(ss_request, sizeof(ss_request), "STREAM %s %d\n", filename, start_word);
    send(ss_fd, ss_request, strlen(ss_request), 0);

    char buffer[256];
    int bytes;

    while ((bytes = recv(ss_fd, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytes] = '\0';

        if (*word_count == start_word && strncmp(buffer, "ERROR", 5) == 0)
        {
            printf("%s\n", buffer + 7);
            close(ss_fd);
            return -1;
        }

        if (strcmp(buffer, "END_STREAM\n") == 0)
        {
            close(ss_fd);
            return 0;
        }

        printf("%s", buffer);
        fflush(stdout);

        *word_count += count_words_in_chunk(buffer);
    }

    if (bytes == 0)
    {
        close(ss_fd);
        return 1; // Disconnected, try next SS
    }
    else
    {
        if (errno == ECONNRESET || errno == EPIPE)
        {
            close(ss_fd);
            return 1;
        }
        perror("Error receiving data from Storage Server");
        close(ss_fd);
        return -1;
    }
}

int handle_stream_command(const char *filename, const char *username)
{
    if (!filename || !username)
    {
        printf("Error: Invalid parameters\n");
        return -1;
    }

    printf("Streaming file: %s\n", filename);

    int ns_fd = connect_to_ns_default();
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

    StreamTarget targets[16];
    int target_count = parse_ss_targets(response, targets, 16);
    close(ns_fd);

    if (target_count <= 0)
    {
        printf("Error: Invalid response from Name Server\n");
        return -1;
    }

    printf("\n--- Streaming: %s ---\n", filename);

    int total_words = 0;
    int success = 0;

    for (int i = 0; i < target_count; ++i)
    {
        int status = stream_from_target(&targets[i], filename, &total_words);

        if (status == 0)
        {
            success = 1;
            break;
        }
        else if (status == -1)
        {
            return -1;
        }
        else if (status == 1)
        {
            printf("\n⚠ Storage Server %s:%d disconnected. Trying next replica...\n",
                   targets[i].ip, targets[i].port);
        }
    }

    if (!success)
    {
        printf("\n\n✗ Error: Unable to complete stream from any Storage Server\n");
        return -1;
    }

    printf("\n--- End of Stream ---\n");
    printf("Total words streamed: %d\n", total_words);

    return 0;
}

// LLM GENERATED CODE ENDS HERE