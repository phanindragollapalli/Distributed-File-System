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
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
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
static volatile sig_atomic_t shutdown_requested = 0;
static int client_server_fd_global = -1;
static int ns_fd_global = -1;

static void sigint_handler(int sig)
{
    (void)sig; // Unused parameter
    shutdown_requested = 1;

    // Write newline for cleaner terminal output
    write(STDOUT_FILENO, "\n", 1);
}

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

        // Provide helpful error message for delimiter issue
        if (error_code == ERR_SENTENCE_OUT_OF_RANGE && sentence_index > 0)
        {
            char msg[512];
            snprintf(msg, sizeof(msg), "ERROR Cannot create sentence %d. Previous sentence needs a delimiter (. ! or ?). Write to sentence %d to add one.\n",
                     sentence_index, sentence_index - 1);
            write(client_fd, msg, strlen(msg));
        }
        else if (error_code == ERR_FILE_LOCKED)
        {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "ERROR Write denied: sentence %d of '%s' is currently being edited by another user. Please try again later.\n",
                     sentence_index, filename);
            write(client_fd, msg, strlen(msg));
        }
        else
        {
            send_error_response(client_fd, error_code);
        }
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
    else if (strcmp(cmd, "UNDO") == 0)
    {
        if (parsed < 2)
        {
            const char *error = "ERROR: UNDO requires filename\n";
            write(client_fd, error, strlen(error));
            close(client_fd);
            return NULL;
        }
        log_message(ss_logger, LOG_INFO, "Client UNDO request for file: %s", filename);

        if (!file_ops_ctx)
        {
            const char *error = "ERROR: File operations not initialized\n";
            write(client_fd, error, strlen(error));
            log_message(ss_logger, LOG_ERROR, "UNDO failed: file_ops_ctx is NULL");
            close(client_fd);
            return NULL;
        }

        // Call undo_file from file_operations.c
        int result = undo_file(file_ops_ctx, filename, "client");
        if (result == SUCCESS)
        {
            const char *ack = "ACK UNDO_SUCCESS\n";
            write(client_fd, ack, strlen(ack));
            log_message(ss_logger, LOG_INFO, "Successfully undone changes to file: %s", filename);

            // Notify NS about metadata update after undo
            notify_ns_metadata_update(file_ops_ctx, filename);
        }
        else
        {
            const char *error = "ERROR: Failed to undo changes\n";
            write(client_fd, error, strlen(error));
            log_message(ss_logger, LOG_ERROR, "Failed to undo file: %s (code=%d)", filename, result);
        }
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

        // Check initial file existence
        if (!file_exists(filepath))
        {
            const char *error = "ERROR: File not found or inaccessible\n";
            write(client_fd, error, strlen(error));
            log_message(ss_logger, LOG_WARN, "File not found: %s", filepath);
            close(client_fd);
            return NULL;
        }

        int total_words = 0;
        int iteration = 0;
        FileStructure *last_snapshot = NULL;

        // Stream with live updates: reload file snapshots periodically
        while (1)
        {
            // Wait briefly if this is a reload check (allow active writes to complete)
            if (iteration > 0)
            {
                usleep(300000); // 0.3 seconds between snapshot checks
            }

            // Check if file is being actively written to
            int file_locked = is_file_locked(file_ops_ctx, filename);
            if (file_locked && iteration > 0)
            {
                // File is being written; wait for write to complete before taking snapshot
                iteration++;
                if (iteration > 100) // Safety: don't wait forever
                {
                    break;
                }
                continue;
            }

            // Load clean snapshot of current file state
            FileStructure *current_snapshot = load_and_parse_file(filepath);
            if (!current_snapshot)
            {
                log_message(ss_logger, LOG_WARN, "File vanished or corrupted during streaming: %s", filepath);
                if (last_snapshot)
                {
                    destroy_file_structure(last_snapshot);
                }
                break;
            }

            // Determine starting point (stream new content only on reloads)
            int start_sentence = 0;
            if (last_snapshot)
            {
                start_sentence = last_snapshot->sentence_count;
                // If no new sentences, we're done
                if (current_snapshot->sentence_count <= start_sentence)
                {
                    destroy_file_structure(current_snapshot);
                    break;
                }
            }

            // Stream sentences from start_sentence onwards
            for (int s = start_sentence; s < current_snapshot->sentence_count; s++)
            {
                SentenceNode *sentence = get_sentence(current_snapshot, s);
                if (!sentence)
                    continue;

                WordNode *word_node = sentence->words;
                while (word_node)
                {
                    if (write(client_fd, word_node->word, strlen(word_node->word)) < 0 ||
                        write(client_fd, " ", 1) < 0)
                    {
                        log_message(ss_logger, LOG_WARN, "Client disconnected during streaming");
                        destroy_file_structure(current_snapshot);
                        if (last_snapshot)
                        {
                            destroy_file_structure(last_snapshot);
                        }
                        close(client_fd);
                        return NULL;
                    }

                    total_words++;
                    word_node = word_node->next;

                    // 0.1 second delay (100,000 microseconds)
                    usleep(100000);
                }

                // Add delimiter if present (no space before punctuation)
                if (sentence->delimiter != '\0')
                {
                    char delim[2] = {sentence->delimiter, '\0'};
                    write(client_fd, delim, 1);
                    write(client_fd, " ", 1); // Space after delimiter
                }
            }

            // Clean up previous snapshot and save current
            if (last_snapshot)
            {
                destroy_file_structure(last_snapshot);
            }
            last_snapshot = current_snapshot;
            iteration++;

            // Check if this is the initial stream (no previous snapshot)
            // If so, we've streamed the initial content; wait for updates
            if (iteration == 1)
            {
                // Initial stream complete, now wait for new content
                continue;
            }
        }

        // Clean up final snapshot
        if (last_snapshot)
        {
            destroy_file_structure(last_snapshot);
        }

        // Send END_STREAM signal to indicate successful completion
        const char *end_signal = "END_STREAM\n";
        write(client_fd, end_signal, strlen(end_signal));

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
    // Set up signal handler for graceful shutdown (Ctrl+C)
    signal(SIGINT, sigint_handler);

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

    // Store NS FD globally for cleanup
    ns_fd_global = ns_fd;

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

    // Store client server FD globally for cleanup
    client_server_fd_global = client_server_fd;

    log_message(ss_logger, LOG_INFO, "Storage Server ready on port %d (clients), handling requests",
                client_port);
    printf("Storage Server listening on port %d for client connections\n", client_port);

    // Main loop: Accept and handle client connections
    while (!shutdown_requested)
    {
        // Use select() with timeout to allow periodic checking of shutdown_requested
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_server_fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1; // 1 second timeout
        timeout.tv_usec = 0;

        int select_result = select(client_server_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (select_result < 0)
        {
            if (errno == EINTR)
            {
                // Interrupted by signal, check shutdown flag
                continue;
            }
            log_message(ss_logger, LOG_ERROR, "select() failed: %s", strerror(errno));
            break;
        }

        if (select_result == 0)
        {
            // Timeout, loop back to check shutdown_requested
            continue;
        }

        // Socket is ready, accept connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(client_server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (errno == EINTR || shutdown_requested)
            {
                // Interrupted by signal or shutting down
                break;
            }
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

    // Graceful cleanup on shutdown
    // printf("\nShutting down Storage Server gracefully...\n");
    log_message(ss_logger, LOG_INFO, "Shutdown signal received, cleaning up");

    // Close sockets
    if (client_server_fd_global >= 0)
    {
        close(client_server_fd_global);
        log_message(ss_logger, LOG_INFO, "Closed client listener socket");
    }

    if (ns_fd_global >= 0)
    {
        close(ns_fd_global);
        log_message(ss_logger, LOG_INFO, "Closed NS connection");
    }

    // Cleanup file operations context
    if (file_ops_ctx)
    {
        cleanup_file_operations(file_ops_ctx);
        log_message(ss_logger, LOG_INFO, "Cleaned up file operations context");
    }

    log_message(ss_logger, LOG_INFO, "Storage Server shutdown complete");
    logger_close(ss_logger);

    printf("Storage Server exited cleanly.\n");
    return 0;
}
