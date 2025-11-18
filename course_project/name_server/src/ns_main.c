/*
 * NAME SERVER MAIN
 * Central coordinator of the distributed file system (similar to HDFS NameNode)
 * Responsibilities:
 * - Accept connections from Storage Servers and Clients on separate ports
 * - Register Storage Servers and track their file lists
 * - Register Clients and handle their requests
 * - Route file requests to appropriate Storage Server using Trie lookup
 * - Maintain file metadata and provide efficient search using LRU cache
 * Uses pthread for concurrent connection handling
 */

#include "../../common/include/protocol.h"
#include "../include/ns_network.h"
#include "../include/ns_registration.h"
#include "../include/ns_storage.h"
#include "../include/ns_routing.h"
#include "../include/ns_metadata.h"
#include "../include/ns_acl.h"
#include "../include/ns_cache.h"
#include "../include/ns_exec.h"
#include "../../common/include/logger.h"
#include "../../common/include/error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

Logger *global_logger = NULL;
TrieNode *global_file_trie = NULL;

static void ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return;
        fprintf(stderr, "Path exists but is not a directory: %s\n", path);
        return;
    }

    if (mkdir(path, 0755) != 0 && errno != EEXIST)
    {
        fprintf(stderr, "Failed to create directory %s: %s\n", path, strerror(errno));
    }
}

static void parse_file_list_chunk(const char *data, char files[][128], int *file_count, int *list_complete)
{
    if (!data || !*data || *list_complete)
        return;

    char temp[4096];
    strncpy(temp, data, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *save_ptr;
    char *line = strtok_r(temp, "\n", &save_ptr);
    while (line && !*list_complete)
    {
        if (strncmp(line, "FILE ", 5) == 0)
        {
            if (*file_count < 128)
            {
                strncpy(files[*file_count], line + 5, 128);
                files[*file_count][127] = '\0';
                (*file_count)++;
            }
        }
        else if (strcmp(line, "END_FILE_LIST") == 0)
        {
            *list_complete = 1;
        }
        line = strtok_r(NULL, "\n", &save_ptr);
    }
}

/* ========== AUTO-SAVE FUNCTIONALITY ========== */

static pthread_t auto_save_thread;
static int auto_save_running = 0;
#define AUTO_SAVE_INTERVAL 30 // seconds

/* Auto-save thread function - saves state every 30 seconds */
void *auto_save_thread_func(void *arg)
{
    log_message(global_logger, LOG_INFO, "Auto-save thread started (interval: %d seconds)", AUTO_SAVE_INTERVAL);

    while (auto_save_running)
    {
        sleep(AUTO_SAVE_INTERVAL);

        if (auto_save_running)
        {
            log_message(global_logger, LOG_DEBUG, "Auto-save triggered");

            // Save Trie
            if (save_trie_to_disk(global_file_trie) == 0)
            {
                log_message(global_logger, LOG_DEBUG, "Trie auto-saved");
            }

            // Save metadata
            if (save_metadata_to_disk() == 0)
            {
                log_message(global_logger, LOG_DEBUG, "Metadata auto-saved");
            }

            // ACL saves itself automatically

            log_message(global_logger, LOG_INFO, "Auto-save completed");
        }
    }

    log_message(global_logger, LOG_INFO, "Auto-save thread stopped");
    return NULL;
}

/* Start auto-save thread */
int start_auto_save()
{
    if (auto_save_running)
    {
        return 0;
    }

    auto_save_running = 1;

    if (pthread_create(&auto_save_thread, NULL, auto_save_thread_func, NULL) != 0)
    {
        log_message(global_logger, LOG_ERROR, "Failed to create auto-save thread");
        auto_save_running = 0;
        return -1;
    }

    pthread_detach(auto_save_thread);
    log_message(global_logger, LOG_INFO, "Auto-save thread started");
    return 0;
}

/* Stop auto-save thread */
void stop_auto_save()
{
    if (!auto_save_running)
    {
        return;
    }

    log_message(global_logger, LOG_INFO, "Stopping auto-save thread...");
    auto_save_running = 0;
}

/* Emergency save on signal (SIGINT, SIGTERM) */
void emergency_save_handler(int signum)
{
    if (global_logger)
    {
        log_message(global_logger, LOG_WARN, "Received signal %d, performing emergency save", signum);
    }

    stop_auto_save();

    // Save all state
    if (global_file_trie)
    {
        save_trie_to_disk(global_file_trie);
    }

    save_metadata_to_disk();
    acl_save_database();

    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "Emergency save completed");
    }

    exit(0);
}

/* ========== END DAY 13 AUTO-SAVE ========== */

/* Helper: Send error response to client */
void send_error_to_client(int client_fd, const char *error_msg)
{
    char response[512];
    snprintf(response, sizeof(response), "ERROR: %s\n", error_msg);
    write(client_fd, response, strlen(response));
}

/* Handle VIEW command - Person 1, Day 8-9
 * Client requests to view a file
 * Process: Check Trie → Check ACL → Return SS info
 */
void handle_view_request(int client_fd, const char *filename, const char *username)
{
    log_message(global_logger, LOG_INFO, "VIEW request: file='%s', user='%s'", filename, username);

    // 1. Check LRU cache first for speed
    int ss_id;
    int found_in_cache = lru_get(filename, &ss_id);

    // 2. If not in cache, search Trie
    if (!found_in_cache)
    {
        if (!trie_search(global_file_trie, filename, &ss_id))
        {
            log_message(global_logger, LOG_WARN, "File not found: %s", filename);
            send_error_to_client(client_fd, "File not found");
            return;
        }
        // Add to cache for future requests
        lru_put(filename, ss_id);
    }

    // 3. Check ACL permissions (user must have READ access)
    if (!acl_can_read(filename, username))
    {
        log_message(global_logger, LOG_WARN, "Permission denied: user='%s', file='%s'", username, filename);
        send_error_to_client(client_fd, "Permission denied. You need READ access to view this file");
        return;
    }

    // 4. Get Storage Server info
    StorageServerInfo *ss_info = get_ss_by_id(ss_id);
    if (!ss_info)
    {
        log_message(global_logger, LOG_ERROR, "SS_ID=%d not found", ss_id);
        send_error_to_client(client_fd, "Storage Server not available");
        return;
    }

    // 5. Send SS info to client
    send_ss_info_to_client(client_fd, ss_info);
}

/* Handle file listing (VIEW) command
 * Supports combinations of flags: -a (all files) and -l (detailed view)
 */
void handle_list_request(int client_fd, const char *username, int include_all, int show_details)
{
    if (!username || strlen(username) == 0)
    {
        send_error_to_client(client_fd, "Username required for VIEW command");
        return;
    }

    log_message(global_logger, LOG_INFO,
                "LIST request from user='%s' (all=%d, details=%d)",
                username, include_all, show_details);

    char all_files[512][256];
    int total_files = list_all_files(all_files, 512);

    if (total_files <= 0)
    {
        const char *msg = "NO_FILES\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    char entries[512][512];
    int entry_count = 0;

    for (int i = 0; i < total_files && entry_count < 512; i++)
    {
        const char *filename = all_files[i];
        int has_read = acl_can_read(filename, username);
        int has_write = acl_can_write(filename, username);

        if (!include_all && !has_read && !has_write)
        {
            continue;
        }

        char perms[32];
        if (has_read && has_write)
            strcpy(perms, "READ,WRITE");
        else if (has_write)
            strcpy(perms, "WRITE");
        else if (has_read)
            strcpy(perms, "READ");
        else
            strcpy(perms, "NO_ACCESS");

        if (show_details)
        {
            FileMetadata *meta = get_file_metadata(filename);
            char owner[32] = "unknown";
            char last_access[64] = "N/A";
            size_t char_count = 0;

            if (meta)
            {
                if (meta->owner[0] != '\0')
                {
                    strncpy(owner, meta->owner, sizeof(owner) - 1);
                    owner[sizeof(owner) - 1] = '\0';
                }
                char_count = meta->size;

                if (meta->last_accessed > 0)
                {
                    struct tm tm_info;
                    localtime_r(&meta->last_accessed, &tm_info);
                    strftime(last_access, sizeof(last_access), "%Y-%m-%d %H:%M:%S", &tm_info);
                }
            }

            snprintf(entries[entry_count], sizeof(entries[entry_count]),
                     "%s|%s|%zu|%s|%s\n",
                     filename, owner, char_count, last_access, perms);
        }
        else
        {
            snprintf(entries[entry_count], sizeof(entries[entry_count]),
                     "%s %s\n", filename, perms);
        }

        entry_count++;
    }

    if (entry_count == 0)
    {
        const char *msg = include_all ? "NO_FILES\n" : "NO_FILES\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    const char *header = show_details ? "FILE_DETAILS\n" : "FILE_LIST\n";
    write(client_fd, header, strlen(header));

    for (int i = 0; i < entry_count; i++)
    {
        write(client_fd, entries[i], strlen(entries[i]));
    }

    log_message(global_logger, LOG_INFO,
                "LIST: Sent %d files to user='%s' (all=%d, details=%d)",
                entry_count, username, include_all, show_details);
}

/* Handle LIST (users) command */
void handle_user_list_request(int client_fd)
{
    char users[128][32];
    int user_count = list_registered_users(users, 128);

    if (user_count <= 0)
    {
        const char *msg = "NO_USERS\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    const char *header = "USER_LIST\n";
    write(client_fd, header, strlen(header));

    for (int i = 0; i < user_count; i++)
    {
        char line[64];
        snprintf(line, sizeof(line), "%s\n", users[i]);
        write(client_fd, line, strlen(line));
    }

    log_message(global_logger, LOG_INFO, "LIST: Sent %d registered users", user_count);
}

/* Handle INFO command - Person 1, Day 8-9
 * Client requests metadata for a specific file
 * Process: Check Trie → Check ACL → Get metadata → Return info
 */
void handle_info_request(int client_fd, const char *filename, const char *username)
{
    log_message(global_logger, LOG_INFO, "INFO request: file='%s', user='%s'", filename, username);

    // 1. Check if file exists in Trie
    int ss_id;
    if (!trie_search(global_file_trie, filename, &ss_id))
    {
        log_message(global_logger, LOG_WARN, "File not found: %s", filename);
        send_error_to_client(client_fd, "File not found in the system");
        return;
    }

    // 2. Check ACL (user needs at least READ permission to view info)
    if (!acl_can_read(filename, username) && !acl_can_write(filename, username))
    {
        log_message(global_logger, LOG_WARN, "Permission denied for INFO: user='%s', file='%s'",
                    username, filename);
        send_error_to_client(client_fd, "Permission denied. You need access to view file info");
        return;
    }

    // 3. Get file metadata
    FileMetadata *metadata = get_file_metadata(filename);
    if (!metadata)
    {
        log_message(global_logger, LOG_ERROR, "Metadata not found for file: %s", filename);
        send_error_to_client(client_fd, "File metadata not available");
        return;
    }

    // 4. Build permissions string
    char perms[32] = "";
    int has_read = acl_can_read(filename, username);
    int has_write = acl_can_write(filename, username);
    if (has_read && has_write)
        strcpy(perms, "READ,WRITE");
    else if (has_read)
        strcpy(perms, "READ");
    else if (has_write)
        strcpy(perms, "WRITE");

    // 5. Send metadata to client (format: FILE_INFO\nfield1\nfield2\n...)
    char response[2048];
    snprintf(response, sizeof(response),
             "FILE_INFO\n%s\n%s\n%ld\n%ld\n%ld\n%ld\n%s\n%d\n",
             metadata->filename,
             metadata->owner,
             (long)metadata->size,
             (long)metadata->created,
             (long)metadata->modified,
             (long)metadata->last_accessed,
             perms,
             metadata->storage_server_id);

    write(client_fd, response, strlen(response));
    log_message(global_logger, LOG_INFO, "INFO: Sent metadata for file='%s' to user='%s'",
                filename, username);
}

/* Handle GRANT command - Person 1, Day 10-11
 * File owner grants READ or WRITE permission to another user
 * Process: Check ownership → Call ACL grant → Return success/error
 */
void handle_grant_request(int client_fd, const char *filename, const char *requester,
                          const char *target_user, const char *permission)
{
    log_message(global_logger, LOG_INFO, "GRANT request: file='%s', requester='%s', target='%s', perm='%s'",
                filename, requester, target_user, permission);

    // 1. Check if file exists
    int ss_id;
    if (!trie_search(global_file_trie, filename, &ss_id))
    {
        send_error_to_client(client_fd, "File not found");
        return;
    }

    // 2. Get file metadata to check owner
    FileMetadata *metadata = get_file_metadata(filename);
    if (!metadata)
    {
        send_error_to_client(client_fd, "File metadata not found");
        return;
    }

    // 3. Verify requester is the file owner
    if (strcmp(metadata->owner, requester) != 0)
    {
        log_message(global_logger, LOG_WARN, "GRANT denied: user='%s' is not owner of '%s'",
                    requester, filename);
        send_error_to_client(client_fd, "Only the file owner can grant permissions");
        return;
    }

    // 4. Parse permission type and call ACL function
    bool success = false;
    if (strcmp(permission, "READ") == 0)
    {
        success = acl_add_read(filename, target_user);
    }
    else if (strcmp(permission, "WRITE") == 0)
    {
        success = acl_add_write(filename, target_user);
    }
    else
    {
        send_error_to_client(client_fd, "Invalid permission type. Use READ or WRITE");
        return;
    }

    // 5. Send response
    if (success)
    {
        const char *ack = "SUCCESS\n";
        write(client_fd, ack, strlen(ack));
        log_message(global_logger, LOG_INFO, "GRANT successful: '%s' permission on '%s' to '%s'",
                    permission, filename, target_user);
    }
    else
    {
        send_error_to_client(client_fd, "Failed to grant permission");
    }
}

/* Handle REVOKE command - Person 1, Day 10-11
 * File owner revokes READ or WRITE permission from a user
 * Process: Check ownership → Call ACL revoke → Return success/error
 */
void handle_revoke_request(int client_fd, const char *filename, const char *requester,
                           const char *target_user, const char *permission)
{
    log_message(global_logger, LOG_INFO, "REVOKE request: file='%s', requester='%s', target='%s', perm='%s'",
                filename, requester, target_user, permission);

    // 1. Check if file exists
    int ss_id;
    if (!trie_search(global_file_trie, filename, &ss_id))
    {
        send_error_to_client(client_fd, "File not found");
        return;
    }

    // 2. Get file metadata to check owner
    FileMetadata *metadata = get_file_metadata(filename);
    if (!metadata)
    {
        send_error_to_client(client_fd, "File metadata not found");
        return;
    }

    // 3. Verify requester is the file owner
    if (strcmp(metadata->owner, requester) != 0)
    {
        log_message(global_logger, LOG_WARN, "REVOKE denied: user='%s' is not owner of '%s'",
                    requester, filename);
        send_error_to_client(client_fd, "Only the file owner can revoke permissions");
        return;
    }

    // 4. Revoke permission via ACL
    bool success = acl_remove_access(filename, target_user);

    // 5. Send response
    if (success)
    {
        const char *ack = "SUCCESS\n";
        write(client_fd, ack, strlen(ack));
        log_message(global_logger, LOG_INFO, "REVOKE successful: '%s' permission on '%s' from '%s'",
                    permission, filename, target_user);
    }
    else
    {
        send_error_to_client(client_fd, "Failed to revoke permission");
    }
}

/* Handle EXEC command - Person 1, Day 12
 * Client requests to execute a file stored in distributed file system
 * Process: Check Trie → Check ACL → Get file from SS → Execute → Return output
 */
void handle_exec_request(int client_fd, const char *filename, const char *username)
{
    log_message(global_logger, LOG_INFO, "EXEC request: file='%s', user='%s'", filename, username);

    // 1. Check if file exists in Trie
    int ss_id;
    if (!trie_search(global_file_trie, filename, &ss_id))
    {
        send_error_to_client(client_fd, "File not found");
        return;
    }

    // 2. Check ACL permissions (user must have READ access to execute)
    if (!acl_can_read(filename, username))
    {
        log_message(global_logger, LOG_WARN, "EXEC denied: user='%s' lacks READ access to '%s'",
                    username, filename);
        send_error_to_client(client_fd, "Permission denied. You need READ access to execute this file");
        return;
    }

    // 3. Get Storage Server info
    StorageServerInfo *ss_info = get_ss_by_id(ss_id);
    if (!ss_info)
    {
        send_error_to_client(client_fd, "Storage Server not available");
        return;
    }

    log_message(global_logger, LOG_INFO, "EXEC: Found file '%s' on SS_%d (%s:%d)",
                filename, ss_id, ss_info->ss_ip, ss_info->ss_nm_port);

    // 4. Execute file and capture output
    char output[65536]; // 64KB output buffer
    int result = execute_file_from_ss(ss_info, filename, output, sizeof(output), ss_id);

    // 5. Send output to client
    if (result == 0)
    {
        // Send success header + output
        char response[70000]; // Slightly larger than output buffer
        snprintf(response, sizeof(response), "EXEC_OUTPUT\n%s", output);
        write(client_fd, response, strlen(response));

        log_message(global_logger, LOG_INFO, "EXEC successful: file='%s', output_size=%zu",
                    filename, strlen(output));
    }
    else
    {
        // Send error from execution
        write(client_fd, output, strlen(output));
        log_message(global_logger, LOG_ERROR, "EXEC failed: file='%s', error='%s'",
                    filename, output);
    }
}

/* Thread handler for Storage Server connections - Person 1, Day 5-6
 * Processes SS registration, receives file list, inserts files into Trie
 * Sends ACK back to Storage Server upon successful registration
 */
void *handle_storage_server(void *arg)
{
    int conn_fd = *(int *)arg;
    free(arg);

    char buffer[4096];
    int bytes_read;

    /* Read and parse registration message from Storage Server */
    bytes_read = read(conn_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0)
    {
        log_message(global_logger, LOG_ERROR, "Failed to read from SS");
        close(conn_fd);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    /* Extract SS IP, Name Server port, and Client port from registration message */
    char ss_ip[32];
    int ss_nm_port, ss_client_port;
    if (sscanf(buffer, "REGISTER_SS %s %d %d", ss_ip, &ss_nm_port, &ss_client_port) == 3)
    {
        log_message(global_logger, LOG_INFO, "Received SS registration: %s:%d (client:%d)",
                    ss_ip, ss_nm_port, ss_client_port);

        /* Prepare to collect list of accessible files from this Storage Server */
        char files[128][128];
        int file_count = 0;
        int list_complete = 0;

        /* Process any file entries that arrived with the registration line */
        char *first_line_end = strchr(buffer, '\n');
        if (first_line_end && *(first_line_end + 1) != '\0')
        {
            parse_file_list_chunk(first_line_end + 1, files, &file_count, &list_complete);
        }

        /* Read file list messages until END_FILE_LIST marker */
        while (!list_complete)
        {
            bytes_read = read(conn_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0)
            {
                log_message(global_logger, LOG_WARN,
                            "SS registration: connection closed before END_FILE_LIST");
                break;
            }
            buffer[bytes_read] = '\0';
            parse_file_list_chunk(buffer, files, &file_count, &list_complete);
        }

        /* Register the Storage Server and get assigned SS_ID */
        int ss_id = register_storage_server(ss_ip, ss_nm_port, ss_client_port,
                                            (const char (*)[128])files, file_count, conn_fd);

        /* Insert all files into Trie for efficient O(m) lookup where m is path length */
        for (int i = 0; i < file_count; i++)
        {
            trie_insert(global_file_trie, files[i], ss_id);
            add_file_metadata(files[i], ss_ip, ss_id);
            log_message(global_logger, LOG_INFO, "Inserted file '%s' into Trie for SS_ID=%d",
                        files[i], ss_id);
        }

        /* Send acknowledgment to Storage Server confirming successful registration */
        const char *ack = "ACK REGISTERED\n";
        write(conn_fd, ack, strlen(ack));

        log_message(global_logger, LOG_INFO, "Successfully registered SS %d with %d files",
                    ss_id, file_count);

        // Keep connection open for future NS→SS commands
        return NULL;
    }
    else
    {
        log_message(global_logger, LOG_ERROR, "Invalid registration message");
        send_error_response(conn_fd, ERR_INVALID_FORMAT);
    }

    close(conn_fd);
    return NULL;
}

// Handler for client connections - Person 1, Days 8-11
// Routes client commands to appropriate handlers
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

    // Parse command type
    char cmd[64], arg1[256], arg2[256], arg3[64], arg4[64];
    int parsed = sscanf(buffer, "%63s %255s %255s %63s %63s", cmd, arg1, arg2, arg3, arg4);

    if (parsed < 1)
    {
        log_message(global_logger, LOG_WARN, "Invalid command received");
        send_error_to_client(conn_fd, "Invalid command format");
        close(conn_fd);
        return NULL;
    }

    // Handle REGISTER_CLIENT
    if (strcmp(cmd, "REGISTER_CLIENT") == 0 && parsed >= 4)
    {
        // Format: REGISTER_CLIENT <username> <client_ip> <client_port>
        char *username = arg1;
        char *client_ip = arg2;
        int client_port = atoi(arg3);

        int client_id = register_client(username, client_ip, client_port);
        log_message(global_logger, LOG_INFO, "Registered client: %s (%s:%d) with ID=%d",
                    username, client_ip, client_port, client_id);

        const char *ack = "ACK CLIENT_REGISTERED\n";
        write(conn_fd, ack, strlen(ack));
    }
    // Handle VIEW command
    else if (strcmp(cmd, "VIEW") == 0)
    {
        if (parsed >= 3 && arg1[0] != '-')
        {
            // Legacy behavior: VIEW <filename> <username>
            handle_view_request(conn_fd, arg1, arg2);
        }
        else
        {
            const char *username = NULL;
            const char *flag_arg = "";

            if (parsed == 2)
            {
                username = arg1;
            }
            else if (parsed >= 3 && arg1[0] == '-')
            {
                flag_arg = arg1;
                username = arg2;
            }
            else
            {
                send_error_to_client(conn_fd, "Invalid VIEW command format");
                close(conn_fd);
                return NULL;
            }

            int include_all = 0;
            int show_details = 0;

            if (flag_arg && strlen(flag_arg) > 0)
            {
                if (flag_arg[0] != '-')
                {
                    send_error_to_client(conn_fd, "Invalid VIEW flags");
                    close(conn_fd);
                    return NULL;
                }

                for (size_t i = 1; flag_arg[i] != '\0'; i++)
                {
                    if (flag_arg[i] == 'a' || flag_arg[i] == 'A')
                        include_all = 1;
                    else if (flag_arg[i] == 'l' || flag_arg[i] == 'L')
                        show_details = 1;
                    else
                    {
                        send_error_to_client(conn_fd, "Unsupported VIEW flag");
                        close(conn_fd);
                        return NULL;
                    }
                }
            }

            handle_list_request(conn_fd, username, include_all, show_details);
        }
    }
    // Handle LIST command - no args for users, legacy LIST <username> for files
    else if (strcmp(cmd, "LIST") == 0)
    {
        if (parsed >= 2)
        {
            handle_list_request(conn_fd, arg1, 0, 0);
        }
        else
        {
            handle_user_list_request(conn_fd);
        }
    }
    // Handle INFO command - arg1=filename, arg2=username
    else if (strcmp(cmd, "INFO") == 0 && parsed >= 3)
    {
        handle_info_request(conn_fd, arg1, arg2);
    }
    // Handle GRANT command - arg1=filename, arg2=requester, arg3=target_user, arg4=permission
    else if (strcmp(cmd, "GRANT") == 0 && parsed >= 5)
    {
        handle_grant_request(conn_fd, arg1, arg2, arg3, arg4);
    }
    // Handle REVOKE command - arg1=filename, arg2=requester, arg3=target_user, arg4=permission
    else if (strcmp(cmd, "REVOKE") == 0 && parsed >= 5)
    {
        handle_revoke_request(conn_fd, arg1, arg2, arg3, arg4);
    }
    // Handle EXEC command - arg1=filename, arg2=username
    else if (strcmp(cmd, "EXEC") == 0 && parsed >= 3)
    {
        handle_exec_request(conn_fd, arg1, arg2);
    }
    // Handle READ command - arg1=filename, arg2=username
    else if (strcmp(cmd, "READ") == 0 && parsed >= 3)
    {
        handle_view_request(conn_fd, arg1, arg2); // Same as VIEW - returns SS_INFO
    }
    // Handle WRITE command - arg1=filename, arg2=username, arg3=sentence_index
    else if (strcmp(cmd, "WRITE") == 0 && parsed >= 4)
    {
        // Check if user has WRITE permission
        if (!acl_can_write(arg1, arg2))
        {
            log_message(global_logger, LOG_WARN, "WRITE denied: user='%s', file='%s'", arg2, arg1);
            send_error_to_client(conn_fd, "Permission denied. You need WRITE access");
        }
        else
        {
            // Return SS info for direct connection
            int ss_id;
            if (trie_search(global_file_trie, arg1, &ss_id))
            {
                StorageServerInfo *ss_info = get_ss_by_id(ss_id);
                send_ss_info_to_client(conn_fd, ss_info);
            }
            else
            {
                send_error_to_client(conn_fd, "File not found");
            }
        }
    }
    // Handle STREAM command - arg1=filename, arg2=username
    else if (strcmp(cmd, "STREAM") == 0 && parsed >= 3)
    {
        handle_view_request(conn_fd, arg1, arg2); // Same as VIEW - returns SS_INFO
    }
    // Handle CREATE command - arg1=filename, arg2=username
    else if (strcmp(cmd, "CREATE") == 0 && parsed >= 3)
    {
        log_message(global_logger, LOG_INFO, "CREATE request: file='%s', user='%s'", arg1, arg2);

        // Check if file already exists
        int ss_id;
        if (trie_search(global_file_trie, arg1, &ss_id))
        {
            send_error_to_client(conn_fd, "File already exists");
        }
        else
        {
            // Select SS (round-robin or first available)
            int total_ss = get_ss_count();
            int target_ss_id = 0; // TODO: implement better selection policy
            log_message(global_logger, LOG_DEBUG, "CREATE: Total SS count = %d", total_ss);
            StorageServerInfo *ss_info = get_ss_by_id(target_ss_id);
            if (!ss_info || ss_info->ns_fd <= 0)
            {
                log_message(global_logger, LOG_ERROR,
                            "CREATE failed: No storage server control connection available (count=%d)", total_ss);
                send_error_to_client(conn_fd, "No storage server available");
            }
            else
            {
                char error_detail[256] = "Storage server failed to create file";
                int create_success = 0;

                pthread_mutex_lock(&ss_info->command_lock);

                do
                {
                    char ss_cmd[512];
                    snprintf(ss_cmd, sizeof(ss_cmd), "CREATE %s\n", arg1);

                    if (write(ss_info->ns_fd, ss_cmd, strlen(ss_cmd)) < 0)
                    {
                        log_message(global_logger, LOG_ERROR,
                                    "CREATE: Failed to send command to SS (%s:%d)",
                                    ss_info->ss_ip, ss_info->ss_nm_port);
                        snprintf(error_detail, sizeof(error_detail),
                                 "Failed to contact storage server");
                        break;
                    }

                    char ss_response[256];
                    int n = read(ss_info->ns_fd, ss_response, sizeof(ss_response) - 1);
                    if (n <= 0)
                    {
                        log_message(global_logger, LOG_ERROR,
                                    "CREATE: Failed to read response from SS (%s:%d)",
                                    ss_info->ss_ip, ss_info->ss_nm_port);
                        snprintf(error_detail, sizeof(error_detail),
                                 "No response from storage server");
                        break;
                    }

                    ss_response[n] = '\0';
                    if (strncmp(ss_response, "ACK", 3) == 0)
                    {
                        create_success = 1;
                    }
                    else
                    {
                        snprintf(error_detail, sizeof(error_detail), "%s", ss_response);
                        log_message(global_logger, LOG_ERROR,
                                    "CREATE: SS returned error: %s", ss_response);
                    }
                } while (0);

                pthread_mutex_unlock(&ss_info->command_lock);

                if (create_success)
                {
                    // Add to Trie and metadata
                    trie_insert(global_file_trie, arg1, target_ss_id);
                    add_file_metadata(arg1, arg2, target_ss_id);

                    // Initialize ACL (owner gets full access)
                    acl_create_file(arg1, arg2);

                    const char *ack = "ACK CREATED\n";
                    write(conn_fd, ack, strlen(ack));
                    log_message(global_logger, LOG_INFO, "File '%s' created by '%s'", arg1, arg2);
                }
                else
                {
                    send_error_to_client(conn_fd, error_detail);
                }
            }
        }
    }
    // Handle DELETE command - arg1=filename, arg2=username
    else if (strcmp(cmd, "DELETE") == 0 && parsed >= 3)
    {
        log_message(global_logger, LOG_INFO, "DELETE request: file='%s', user='%s'", arg1, arg2);

        // Check if file exists
        int ss_id;
        if (!trie_search(global_file_trie, arg1, &ss_id))
        {
            send_error_to_client(conn_fd, "File not found");
        }
        else
        {
            // Check if user is owner
            FileMetadata *meta = get_file_metadata(arg1);
            if (meta && strcmp(meta->owner, arg2) != 0)
            {
                send_error_to_client(conn_fd, "Only the owner can delete the file");
            }
            else
            {
                StorageServerInfo *ss_info = get_ss_by_id(ss_id);
                if (!ss_info || ss_info->ns_fd <= 0)
                {
                    send_error_to_client(conn_fd, "Storage Server connection unavailable");
                    close(conn_fd);
                    return NULL;
                }

                char error_detail[256] = "Storage server failed to delete file";
                int delete_success = 0;

                pthread_mutex_lock(&ss_info->command_lock);

                do
                {
                    char ss_cmd[512];
                    snprintf(ss_cmd, sizeof(ss_cmd), "DELETE %s\n", arg1);

                    if (write(ss_info->ns_fd, ss_cmd, strlen(ss_cmd)) < 0)
                    {
                        log_message(global_logger, LOG_ERROR,
                                    "DELETE: Failed to send command to SS (%s:%d)",
                                    ss_info->ss_ip, ss_info->ss_nm_port);
                        snprintf(error_detail, sizeof(error_detail),
                                 "Failed to contact storage server");
                        break;
                    }

                    char ss_response[256];
                    int n = read(ss_info->ns_fd, ss_response, sizeof(ss_response) - 1);
                    if (n <= 0)
                    {
                        log_message(global_logger, LOG_ERROR,
                                    "DELETE: Failed to read response from SS (%s:%d)",
                                    ss_info->ss_ip, ss_info->ss_nm_port);
                        snprintf(error_detail, sizeof(error_detail),
                                 "No response from storage server");
                        break;
                    }

                    ss_response[n] = '\0';
                    if (strncmp(ss_response, "ACK", 3) == 0)
                    {
                        delete_success = 1;
                    }
                    else
                    {
                        snprintf(error_detail, sizeof(error_detail), "%s", ss_response);
                        log_message(global_logger, LOG_ERROR,
                                    "DELETE: SS returned error: %s", ss_response);
                    }
                } while (0);

                pthread_mutex_unlock(&ss_info->command_lock);

                if (delete_success)
                {
                    delete_file_metadata(arg1);
                    acl_delete_file(arg1);
                    trie_remove(global_file_trie, arg1);

                    const char *ack = "ACK DELETED\n";
                    write(conn_fd, ack, strlen(ack));
                    log_message(global_logger, LOG_INFO, "File '%s' deleted by '%s'", arg1, arg2);
                }
                else
                {
                    send_error_to_client(conn_fd, error_detail);
                }
            }
        }
    }
    else
    {
        log_message(global_logger, LOG_WARN, "Unknown or malformed command: %s", cmd);
        send_error_to_client(conn_fd, "Unknown command or invalid format");
    }

    close(conn_fd);
    return NULL;
}

int main()
{
    ensure_directory("logs");
    ensure_directory("name_server");
    ensure_directory("name_server/data");

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

    // Initialize LRU cache (capacity 16)
    // Note: cache is initialized internally in ns_cache.c
    log_message(global_logger, LOG_INFO, "Initialized LRU cache");

    // Initialize metadata storage
    init_metadata_storage();

    /* ========== DAY 13: LOAD PERSISTENT STATE ========== */
    log_message(global_logger, LOG_INFO, "Loading persistent state...");

    // Load Trie from disk
    if (load_trie_from_disk(global_file_trie) == 0)
    {
        log_message(global_logger, LOG_INFO, "Trie state restored from disk");
    }
    else
    {
        log_message(global_logger, LOG_WARN, "Failed to load Trie state, starting fresh");
    }

    // Load metadata from disk
    if (load_metadata_from_disk() == 0)
    {
        log_message(global_logger, LOG_INFO, "Metadata restored from disk");
    }
    else
    {
        log_message(global_logger, LOG_WARN, "Failed to load metadata, starting fresh");
    }

    // Initialize ACL system
    acl_load_database();
    log_message(global_logger, LOG_INFO, "Initialized ACL system");

    /* ========== DAY 13: SETUP SIGNAL HANDLERS AND AUTO-SAVE ========== */
    // Register signal handlers for graceful shutdown
    signal(SIGINT, emergency_save_handler);
    signal(SIGTERM, emergency_save_handler);
    log_message(global_logger, LOG_INFO, "Signal handlers registered");

    // Start auto-save thread
    if (start_auto_save() == 0)
    {
        log_message(global_logger, LOG_INFO, "Auto-save enabled (every %d seconds)", AUTO_SAVE_INTERVAL);
    }
    else
    {
        log_message(global_logger, LOG_WARN, "Auto-save disabled (failed to start)");
    }
    /* ========== END DAY 13 SETUP ========== */

    int server_fd = ns_server_init(NS_PORT);
    if (server_fd < 0)
    {
        log_message(global_logger, LOG_ERROR, "Failed to initialize server");
        logger_close(global_logger);
        return 1;
    }

    log_message(global_logger, LOG_INFO, "Name Server ready, accepting connections");
    printf("Name Server running on port %d\n", NS_PORT);
    printf("Commands: VIEW, LIST, INFO, GRANT, REVOKE, EXEC\n");
    printf("Persistence: Enabled (auto-save every %d seconds)\n", AUTO_SAVE_INTERVAL);
    printf("Press Ctrl+C for graceful shutdown\n");

    while (1)
    {
        int conn_fd = ns_accept_connection(server_fd);
        if (conn_fd < 0)
        {
            log_message(global_logger, LOG_ERROR, "Failed to accept connection");
            continue;
        }

        log_connection(global_logger, "ACCEPTED", "connection", conn_fd);

        // Peek at the first few bytes to determine connection type
        char peek_buffer[64];
        int peek_bytes = recv(conn_fd, peek_buffer, sizeof(peek_buffer) - 1, MSG_PEEK);

        if (peek_bytes > 0)
        {
            peek_buffer[peek_bytes] = '\0';

            // Create thread to handle connection
            pthread_t thread_id;
            int *conn_fd_ptr = malloc(sizeof(int));
            *conn_fd_ptr = conn_fd;

            // Determine handler based on message prefix
            void *(*handler)(void *) = handle_client; // Default to client handler

            if (strncmp(peek_buffer, "REGISTER_SS", 11) == 0)
            {
                handler = handle_storage_server;
                log_message(global_logger, LOG_DEBUG, "Routing to SS handler");
            }
            else
            {
                log_message(global_logger, LOG_DEBUG, "Routing to client handler");
            }

            if (pthread_create(&thread_id, NULL, handler, conn_fd_ptr) != 0)
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
        else
        {
            log_message(global_logger, LOG_ERROR, "Failed to peek at connection");
            close(conn_fd);
        }
    }

    // Cleanup (unreachable in current design)
    acl_save_database();
    lru_cache_free();
    logger_close(global_logger);
    return 0;
}
