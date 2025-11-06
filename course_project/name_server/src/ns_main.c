#include "../../common/include/protocol.h"
#include "../include/ns_network.h"
#include "../include/ns_registration.h"
#include "../include/ns_storage.h"
#include "../include/ns_routing.h"
#include "../include/ns_metadata.h"
#include "../../common/include/logger.h"
#include "../../common/include/error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

Logger *global_logger = NULL;
TrieNode *global_file_trie = NULL;

// Handler for storage server connections
void *handle_storage_server(void *arg)
{
    int conn_fd = *(int *)arg;
    free(arg);

    char buffer[4096];
    int bytes_read;

    // Read registration message
    bytes_read = read(conn_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0)
    {
        log_message(global_logger, LOG_ERROR, "Failed to read from SS");
        close(conn_fd);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    // Parse REGISTER_SS message
    char ss_ip[32];
    int ss_nm_port, ss_client_port;
    if (sscanf(buffer, "REGISTER_SS %s %d %d", ss_ip, &ss_nm_port, &ss_client_port) == 3)
    {
        log_message(global_logger, LOG_INFO, "Received SS registration: %s:%d (client:%d)",
                    ss_ip, ss_nm_port, ss_client_port);

        // Collect file list
        char files[128][128];
        int file_count = 0;

        // Read file list
        while (1)
        {
            bytes_read = read(conn_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0)
                break;
            buffer[bytes_read] = '\0';

            // Parse FILE messages
            char *line = strtok(buffer, "\n");
            while (line != NULL)
            {
                if (strncmp(line, "FILE ", 5) == 0)
                {
                    strncpy(files[file_count], line + 5, 128);
                    files[file_count][127] = '\0';
                    file_count++;
                    if (file_count >= 128)
                        break;
                }
                else if (strcmp(line, "END_FILE_LIST") == 0)
                {
                    goto done_reading;
                }
                line = strtok(NULL, "\n");
            }
        }

    done_reading:
        // Register the storage server
        int ss_id = register_storage_server(ss_ip, ss_nm_port, ss_client_port,
                                            (const char (*)[128])files, file_count);

        // Insert files into trie
        for (int i = 0; i < file_count; i++)
        {
            trie_insert(global_file_trie, files[i], ss_id);
            add_file_metadata(files[i], ss_ip, ss_id);
            log_message(global_logger, LOG_INFO, "Inserted file '%s' into Trie for SS_ID=%d",
                        files[i], ss_id);
        }

        // Send ACK
        const char *ack = "ACK REGISTERED\n";
        write(conn_fd, ack, strlen(ack));

        log_message(global_logger, LOG_INFO, "Successfully registered SS %d with %d files",
                    ss_id, file_count);
    }
    else
    {
        log_message(global_logger, LOG_ERROR, "Invalid registration message");
        send_error_response(conn_fd, ERR_INVALID_REQUEST);
    }

    close(conn_fd);
    return NULL;
}

// Handler for client connections
void *handle_client(void *arg)
{
    int conn_fd = *(int *)arg;
    free(arg);

    char buffer[4096];
    int bytes_read;

    // Read client message
    bytes_read = read(conn_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0)
    {
        log_message(global_logger, LOG_ERROR, "Failed to read from client");
        close(conn_fd);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    // Parse REGISTER_CLIENT message
    char username[32], client_ip[32];
    int client_port;
    if (sscanf(buffer, "REGISTER_CLIENT %s %s %d", username, client_ip, &client_port) == 3)
    {
        int client_id = register_client(username, client_ip, client_port);
        log_message(global_logger, LOG_INFO, "Registered client: %s (%s:%d) with ID=%d",
                    username, client_ip, client_port, client_id);

        const char *ack = "ACK CLIENT_REGISTERED\n";
        write(conn_fd, ack, strlen(ack));
    }
    else
    {
        log_message(global_logger, LOG_WARN, "Client connected without proper registration");
        // Handle client commands here in future
    }

    close(conn_fd);
    return NULL;
}

int main()
{
    // Initialize logger
    global_logger = logger_init("logs/ns.log", "NameServer", LOG_DEBUG, 1);
    if (!global_logger)
    {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    log_message(global_logger, LOG_INFO, "Starting Name Server on port %d", NS_PORT);

    // Initialize global file trie
    global_file_trie = trie_create_node();
    log_message(global_logger, LOG_INFO, "Initialized file Trie");

    // Initialize metadata storage
    init_metadata_storage();

    int server_fd = ns_server_init(NS_PORT);
    if (server_fd < 0)
    {
        log_message(global_logger, LOG_ERROR, "Failed to initialize server");
        logger_close(global_logger);
        return 1;
    }

    log_message(global_logger, LOG_INFO, "Name Server ready, accepting connections");
    printf("Name Server running on port %d\n", NS_PORT);

    while (1)
    {
        int conn_fd = ns_accept_connection(server_fd);
        if (conn_fd < 0)
        {
            log_message(global_logger, LOG_ERROR, "Failed to accept connection");
            continue;
        }

        log_connection(global_logger, "ACCEPTED", "connection", conn_fd);

        // Create thread to handle connection
        pthread_t thread_id;
        int *conn_fd_ptr = malloc(sizeof(int));
        *conn_fd_ptr = conn_fd;

        // For now, assume it's a storage server (we'll improve detection later)
        // A proper implementation would peek at the message to determine type
        if (pthread_create(&thread_id, NULL, handle_storage_server, conn_fd_ptr) != 0)
        {
            log_message(global_logger, LOG_ERROR, "Failed to create handler thread");
            free(conn_fd_ptr);
            close(conn_fd);
        }
        else
        {
            pthread_detach(thread_id);
        }
    }

    logger_close(global_logger);
    return 0;
}
