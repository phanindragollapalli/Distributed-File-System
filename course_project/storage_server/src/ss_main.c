/*
 * STORAGE SERVER MAIN
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
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include "../../common/include/protocol.h"
#include "../include/ss_network.h"
#include "../include/ss_registration.h"
#include "../include/ss_commands.h"
#include "../include/ss_persistence.h"
#include "../include/file_operations.h"
#include "../../common/include/logger.h"
#include "../../common/include/error_codes.h"

Logger *ss_logger = NULL;
static char storage_dir[256] = "";
static FileOperationContext *file_ops_ctx = NULL;

static int directory_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
}

static ssize_t read_line_from_socket(int fd, char *buffer, size_t max_len)
{
    size_t pos = 0;
    while (pos < max_len - 1)
    {
        char c;
        ssize_t bytes = recv(fd, &c, 1, 0);
        if (bytes < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }

        if (bytes == 0)
        {
            return 0;
        }

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            break;
        }

        buffer[pos++] = c;
    }

    buffer[pos] = '\0';
    return (ssize_t)pos;
}

static int start_client_listener(int base_port, int max_attempts, int *out_fd, int *actual_port)
{
    if (!out_fd || !actual_port)
    {
        return -1;
    }

    for (int attempt = 0; attempt < max_attempts; attempt++)
    {
        int trial_port = base_port + attempt;
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to create client server socket: %s",
                        strerror(errno));
            return -1;
        }

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

#ifdef SO_REUSEPORT
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(trial_port);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            if (listen(fd, 10) < 0)
            {
                log_message(ss_logger, LOG_ERROR, "Failed to listen on client port %d: %s",
                            trial_port, strerror(errno));
                close(fd);
                return -1;
            }

            *out_fd = fd;
            *actual_port = trial_port;

            if (attempt > 0)
            {
                log_message(ss_logger, LOG_WARN,
                            "Client port %d was busy; switched to available port %d",
                            base_port, trial_port);
            }
            else
            {
                log_message(ss_logger, LOG_INFO, "Bound client listener to port %d", trial_port);
            }

            return 0;
        }

        int err = errno;
        close(fd);

        if (err == EADDRINUSE)
        {
            log_message(ss_logger, LOG_WARN,
                        "Client port %d already in use, trying next...", trial_port);
            continue;
        }

        log_message(ss_logger, LOG_ERROR,
                    "Failed to bind client port %d: %s", trial_port, strerror(err));
        return -1;
    }

    log_message(ss_logger, LOG_ERROR,
                "Unable to find available client port in range [%d, %d]",
                base_port, base_port + max_attempts - 1);
    return -1;
}

static void handle_write_session(int client_fd, const char *filename, int sentence_index, const char *username)
{
    if (!file_ops_ctx)
    {
        send_error_response(client_fd, ERR_GENERAL);
        return;
    }

    int error_code = SUCCESS;
    WriteContext *wctx = begin_write(file_ops_ctx, filename, sentence_index, username, &error_code);
    if (!wctx)
    {
        if (error_code == SUCCESS)
        {
            error_code = ERR_GENERAL;
        }
        send_error_response(client_fd, error_code);
        log_message(ss_logger, LOG_WARN, "BEGIN_WRITE failed for file '%s' (user=%s)", filename, username);
        return;
    }

    log_message(ss_logger, LOG_INFO, "BEGIN_WRITE accepted for file='%s', sentence=%d, user='%s'",
                filename, sentence_index, username);
    const char *ack = "ACK BEGIN_WRITE\n";
    write(client_fd, ack, strlen(ack));

    char line[1024];
    while (1)
    {
        ssize_t len = read_line_from_socket(client_fd, line, sizeof(line));
        if (len <= 0)
        {
            log_message(ss_logger, LOG_WARN,
                        "WRITE session aborted due to client disconnect for file='%s'", filename);
            rollback_write(file_ops_ctx, wctx, filename);
            cleanup_write_context(wctx);
            return;
        }

        if (len == 0)
        {
            continue;
        }

        if (strcmp(line, "COMMIT") == 0)
        {
            int result = commit_write(file_ops_ctx, wctx, filename);
            if (result == SUCCESS)
            {
                const char *success = "SUCCESS Write committed\n";
                write(client_fd, success, strlen(success));
                log_message(ss_logger, LOG_INFO, "WRITE committed for file='%s' by '%s'", filename, username);
            }
            else
            {
                send_error_response(client_fd, result);
                log_message(ss_logger, LOG_ERROR,
                            "WRITE commit failed for file='%s': %s",
                            filename, error_code_to_string(result));
            }

            cleanup_write_context(wctx);
            return;
        }
        else if (strcmp(line, "ROLLBACK") == 0)
        {
            rollback_write(file_ops_ctx, wctx, filename);
            cleanup_write_context(wctx);
            const char *rolled_back = "SUCCESS Write rolled back\n";
            write(client_fd, rolled_back, strlen(rolled_back));
            log_message(ss_logger, LOG_INFO, "WRITE rolled back for file='%s' by '%s'", filename, username);
            return;
        }
        else if (strncmp(line, "WORD", 4) == 0)
        {
            const char *args = line + 4;
            while (*args == ' ')
            {
                args++;
            }

            if (*args == '\0')
            {
                send_error_response(client_fd, ERR_INVALID_FORMAT);
                continue;
            }

            char *endptr;
            long index_long = strtol(args, &endptr, 10);
            if (args == endptr)
            {
                send_error_response(client_fd, ERR_INVALID_FORMAT);
                continue;
            }

            while (*endptr == ' ')
            {
                endptr++;
            }

            if (*endptr == '\0')
            {
                send_error_response(client_fd, ERR_INVALID_FORMAT);
                continue;
            }

            if (index_long < 0 || index_long > INT_MAX)
            {
                send_error_response(client_fd, ERR_INVALID_INDEX);
                continue;
            }

            int result = write_word(wctx, (int)index_long, endptr);
            if (result == SUCCESS)
            {
                char ack_word[64];
                snprintf(ack_word, sizeof(ack_word), "ACK WORD %ld\n", index_long);
                write(client_fd, ack_word, strlen(ack_word));
                log_message(ss_logger, LOG_DEBUG,
                            "WORD updated: file='%s', sentence=%d, word=%ld",
                            filename, sentence_index, index_long);
            }
            else
            {
                send_error_response(client_fd, result);
                log_message(ss_logger, LOG_WARN,
                            "WORD update failed: file='%s', word=%ld, error=%s",
                            filename, index_long, error_code_to_string(result));
            }
        }
        else
        {
            send_error_response(client_fd, ERR_INVALID_FORMAT);
        }
    }
}

static void resolve_storage_directory()
{
    const char *candidates[] = {
        "storage/files",
        "storage_server/storage/files",
        "../storage_server/storage/files"};

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
    {
        if (directory_exists(candidates[i]))
        {
            strncpy(storage_dir, candidates[i], sizeof(storage_dir) - 1);
            storage_dir[sizeof(storage_dir) - 1] = '\0';
            log_message(ss_logger, LOG_INFO, "Using storage directory: %s", storage_dir);
            return;
        }
    }

    // Fall back to default path and warn user
    strncpy(storage_dir, candidates[0], sizeof(storage_dir) - 1);
    storage_dir[sizeof(storage_dir) - 1] = '\0';
    log_message(ss_logger, LOG_WARN,
                "Storage directory not found; defaulting to %s (ensure this path exists)",
                storage_dir);
}

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

/* Thread handler for client file requests - Person 1, Days 5-7 + Person 2
 * Serves file contents directly to clients after NS routing
 * Handles READ and STREAM requests from clients
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

    // Parse command: "READ <filename>", "STREAM <filename>", or write protocol commands
    char cmd[64], filename[256];
    int parsed = sscanf(buffer, "%63s %255s", cmd, filename);
    if (parsed < 1)
    {
        const char *error = "ERROR: Invalid command format\n";
        write(client_fd, error, strlen(error));
        log_message(ss_logger, LOG_WARN, "Invalid client command: %s", buffer);
        close(client_fd);
        return NULL;
    }

    // Build full file path (used by READ/STREAM)
    char filepath[512];
    if (parsed >= 2)
    {
        snprintf(filepath, sizeof(filepath), "%s/%s", storage_dir, filename);
    }
    else
    {
        filepath[0] = '\0';
    }

    if (strcmp(cmd, "READ") == 0)
    {
        if (parsed < 2)
        {
            const char *error = "ERROR: READ requires filename\n";
            write(client_fd, error, strlen(error));
            close(client_fd);
            return NULL;
        }
        log_message(ss_logger, LOG_INFO, "Client READ request for file: %s", filename);

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
    else if (strcmp(cmd, "BEGIN_WRITE") == 0)
    {
        int sentence_index;
        char user[64];
        if (sscanf(buffer, "BEGIN_WRITE %255s %d %63s", filename, &sentence_index, user) != 3)
        {
            const char *error = "ERROR: Invalid BEGIN_WRITE format\n";
            write(client_fd, error, strlen(error));
            log_message(ss_logger, LOG_WARN, "Malformed BEGIN_WRITE command: %s", buffer);
            close(client_fd);
            return NULL;
        }

        handle_write_session(client_fd, filename, sentence_index, user);
        close(client_fd);
        return NULL;
    }
    else if (strcmp(cmd, "STREAM") == 0)
    {
        if (parsed < 2)
        {
            const char *error = "ERROR: STREAM requires filename\n";
            write(client_fd, error, strlen(error));
            close(client_fd);
            return NULL;
        }
        log_message(ss_logger, LOG_INFO, "Client STREAM request for file: %s", filename);

        // Open file for streaming
        FILE *f = fopen(filepath, "r");
        if (!f)
        {
            const char *error = "ERROR: File not found or inaccessible\n";
            write(client_fd, error, strlen(error));
            log_message(ss_logger, LOG_WARN, "File not found: %s", filepath);
            close(client_fd);
            return NULL;
        }

        // Read entire file into memory for parsing
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *content = malloc(file_size + 1);
        if (!content)
        {
            fclose(f);
            const char *error = "ERROR: Memory allocation failed\n";
            write(client_fd, error, strlen(error));
            log_message(ss_logger, LOG_ERROR, "Memory allocation failed for streaming");
            close(client_fd);
            return NULL;
        }

        fread(content, 1, file_size, f);
        content[file_size] = '\0';
        fclose(f);

        // Stream word by word with 0.1 second delay
        char word[256];
        int word_idx = 0;
        int total_words = 0;

        for (long i = 0; i <= file_size; i++)
        {
            char c = (i < file_size) ? content[i] : '\0';

            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\0')
            {
                if (word_idx > 0)
                {
                    word[word_idx] = '\0';

                    // Send word followed by space
                    if (write(client_fd, word, word_idx) < 0 ||
                        write(client_fd, " ", 1) < 0)
                    {
                        log_message(ss_logger, LOG_WARN, "Client disconnected during streaming");
                        free(content);
                        close(client_fd);
                        return NULL;
                    }

                    total_words++;
                    word_idx = 0;

                    // 0.1 second delay (100,000 microseconds)
                    usleep(100000);
                }

                // Preserve newlines
                if (c == '\n')
                {
                    write(client_fd, "\n", 1);
                }
            }
            else
            {
                if (word_idx < sizeof(word) - 1)
                {
                    word[word_idx++] = c;
                }
            }
        }

        free(content);
        log_message(ss_logger, LOG_INFO, "Streamed %d words from file '%s' to client",
                    total_words, filename);
    }
    else
    {
        const char *error = "ERROR: Unknown command. Use: READ or STREAM\n";
        write(client_fd, error, strlen(error));
        log_message(ss_logger, LOG_WARN, "Unknown command: %s", cmd);
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

    resolve_storage_directory();

    file_ops_ctx = init_file_operations(storage_dir, 0, "127.0.0.1", NS_PORT);
    if (!file_ops_ctx)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize file operations context");
        logger_close(ss_logger);
        return 1;
    }

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

    // Prepare client listener before registration so advertised port is reachable
    int client_server_fd = -1;
    int client_port = SS_BASE_PORT + 100; // Default client port
    if (start_client_listener(client_port, 32, &client_server_fd, &client_port) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Unable to create client listener socket");
        close(ns_fd);
        logger_close(ss_logger);
        return 1;
    }

    // Register with NS using the actual listener port
    if (register_with_ns(ns_fd, "127.0.0.1", SS_BASE_PORT, client_port))
    {
        log_message(ss_logger, LOG_INFO, "Successfully registered with Name Server (client_port=%d)",
                    client_port);
    }
    else
    {
        log_message(ss_logger, LOG_ERROR, "Registration with Name Server failed");
    }

    // Send file list to NS
    if (send_file_list_to_ns(ns_fd, storage_dir) == 0)
    {
        log_message(ss_logger, LOG_INFO, "Sent file list to Name Server");
    }
    else
    {
        log_message(ss_logger, LOG_ERROR,
                    "Failed to enumerate files in %s; Storage Server will not be registered",
                    storage_dir);
    }

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
