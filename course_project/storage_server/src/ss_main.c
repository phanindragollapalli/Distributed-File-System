/*
 * STORAGE SERVER MAIN
 * Person 1, Days 5-7
 *
 * Main entry point for Storage Server. Initializes logger, persistence,
 * registers with Name Server, and starts command processing loop.
 * Also listens for direct client connections to serve file contents.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../../common/include/protocol.h"
#include "../include/ss_network.h"
#include "../include/ss_registration.h"
#include "../include/ss_commands.h"
#include "../include/ss_persistence.h"
#include "../../common/include/logger.h"
#include "../../common/include/error_codes.h"

Logger *ss_logger = NULL;
static char storage_dir[256] = "storage/files";

/* Thread handler for NS commands - Person 1, Days 5-7
 * Handles commands from Name Server (CREATE, DELETE, etc.)
 */
void *handle_ns_connection(void *arg)
{
    int ns_fd = *(int *)arg;
    free(arg);

    log_message(ss_logger, LOG_INFO, "Started NS command handler thread");

    // Main loop to handle NS commands
    while (1)
    {
        int result = ss_handle_ns_command(ns_fd, storage_dir);
        if (result < 0)
        {
            log_message(ss_logger, LOG_ERROR, "Connection to NS lost");
            break;
        }
    }

    close(ns_fd);
    return NULL;
}

/* Thread handler for client file requests - Person 1, Days 5-7
 * Serves file contents directly to clients after NS routing
 * Handles READ requests from clients
 */
void *handle_client_request(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[4096];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to read client request");
        close(client_fd);
        return NULL;
    }

    buffer[bytes_read] = '\0';

    // Parse command: "READ <filename>"
    char cmd[64], filename[256];
    if (sscanf(buffer, "%63s %255s", cmd, filename) == 2 && strcmp(cmd, "READ") == 0)
    {
        log_message(ss_logger, LOG_INFO, "Client READ request for file: %s", filename);

        // Build full file path
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", storage_dir, filename);

        // Open and send file contents
        FILE *f = fopen(filepath, "r");
        if (f)
        {
            // Read and send file in chunks
            size_t total_sent = 0;
            while (!feof(f))
            {
                size_t n = fread(buffer, 1, sizeof(buffer), f);
                if (n > 0)
                {
                    write(client_fd, buffer, n);
                    total_sent += n;
                }
            }
            fclose(f);

            log_message(ss_logger, LOG_INFO, "Sent %zu bytes of file '%s' to client",
                        total_sent, filename);
        }
        else
        {
            const char *error = "ERROR: File not found or inaccessible\n";
            write(client_fd, error, strlen(error));
            log_message(ss_logger, LOG_WARN, "File not found: %s", filepath);
        }
    }
    else
    {
        const char *error = "ERROR: Invalid command format. Use: READ <filename>\n";
        write(client_fd, error, strlen(error));
        log_message(ss_logger, LOG_WARN, "Invalid client command: %s", buffer);
    }

    close(client_fd);
    return NULL;
}

int main()
{
    // Initialize logger
    ss_logger = logger_init("logs/ss.log", "StorageServer", LOG_DEBUG, 1);
    if (!ss_logger)
    {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    log_message(ss_logger, LOG_INFO, "Starting Storage Server");

    // Initialize persistence system
    if (init_persistence("data") < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize persistence");
        logger_close(ss_logger);
        return 1;
    }

    // Connect to NS
    int ns_fd = ss_connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to connect to Name Server");
        logger_close(ss_logger);
        return 1;
    }

    log_connection(ss_logger, "CONNECTED", "127.0.0.1", NS_PORT);

    // Register with NS (using SS_PORT for client connections)
    int client_port = SS_PORT + 100; // Different port for direct client connections
    if (register_with_ns(ns_fd, "127.0.0.1", SS_PORT, client_port))
    {
        log_message(ss_logger, LOG_INFO, "Successfully registered with Name Server (client_port=%d)",
                    client_port);
    }
    else
    {
        log_message(ss_logger, LOG_ERROR, "Registration with Name Server failed");
    }

    // Send file list to NS
    send_file_list_to_ns(ns_fd, storage_dir);
    log_message(ss_logger, LOG_INFO, "Sent file list to Name Server");

    // Create thread to handle NS commands
    pthread_t ns_thread;
    int *ns_fd_ptr = malloc(sizeof(int));
    *ns_fd_ptr = ns_fd;
    if (pthread_create(&ns_thread, NULL, handle_ns_connection, ns_fd_ptr) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create NS handler thread");
        free(ns_fd_ptr);
        close(ns_fd);
        logger_close(ss_logger);
        return 1;
    }
    pthread_detach(ns_thread);

    log_message(ss_logger, LOG_INFO, "NS handler thread started");

    // Create server socket for direct client connections
    int client_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_server_fd < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create client server socket");
        logger_close(ss_logger);
        return 1;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(client_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to client port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(client_port);

    if (bind(client_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to bind client server socket to port %d", client_port);
        close(client_server_fd);
        logger_close(ss_logger);
        return 1;
    }

    // Listen for client connections
    if (listen(client_server_fd, 10) < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to listen on client server socket");
        close(client_server_fd);
        logger_close(ss_logger);
        return 1;
    }

    log_message(ss_logger, LOG_INFO, "Storage Server ready on port %d (clients), handling requests",
                client_port);
    printf("Storage Server listening on port %d for client connections\n", client_port);

    // Main loop: Accept and handle client connections
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(client_server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to accept client connection");
            continue;
        }

        log_message(ss_logger, LOG_INFO, "Accepted client connection");

        // Create thread to handle client
        pthread_t client_thread;
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;

        if (pthread_create(&client_thread, NULL, handle_client_request, client_fd_ptr) != 0)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to create client handler thread");
            free(client_fd_ptr);
            close(client_fd);
        }
        else
        {
            pthread_detach(client_thread);
        }
    }

    // Cleanup (unreachable in current design)
    close(client_server_fd);
    logger_close(ss_logger);
    return 0;
}
