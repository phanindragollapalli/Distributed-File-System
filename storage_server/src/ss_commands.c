/*STORAGE SERVER COMMAND HANDLER.
 Processes commands received from Name Server (CREATE, DELETE, etc.).
 */

#include "../include/ss_commands.h"
#include "../include/ss_persistence.h"
#include "../../common/include/logger.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

extern Logger *ss_logger;

// Handle command from Name Server
int ss_handle_ns_command(int ns_fd, const char *storage_dir)
{
    char buf[256];
    int n = read(ns_fd, buf, sizeof(buf) - 1);
    if (n <= 0)
        return -1;
    buf[n] = '\0';

    char cmd[64], arg[128];
    if (sscanf(buf, "%s %127s", cmd, arg) < 2)
        return 0; // Ignore malformed

    if (strcmp(cmd, "CREATE") == 0)
    {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", storage_dir, arg);
        FILE *f = fopen(path, "w");
        if (f)
        {
            fclose(f);
            // Save metadata for persistence
            save_file_info(arg, "system");
            if (ss_logger)
            {
                log_message(ss_logger, LOG_INFO, "Created file: %s", arg);
            }
            write(ns_fd, "ACK CREATE\n", 11);
        }
        else
        {
            if (ss_logger)
            {
                log_message(ss_logger, LOG_ERROR, "Failed to create file: %s", arg);
            }
            write(ns_fd, "ERROR CREATE_FAILED\n", 20);
        }
    }
    else if (strcmp(cmd, "DELETE") == 0)
    {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", storage_dir, arg);
        if (unlink(path) == 0)
        {
            // Delete metadata
            delete_file_info(arg);
            if (ss_logger)
            {
                log_message(ss_logger, LOG_INFO, "Deleted file: %s", arg);
            }
            write(ns_fd, "ACK DELETE\n", 11);
        }
        else
        {
            if (ss_logger)
            {
                log_message(ss_logger, LOG_ERROR, "Failed to delete file: %s", arg);
            }
            write(ns_fd, "ERROR DELETE_FAILED\n", 20);
        }
    }
    else if (strcmp(cmd, "GET_FILE") == 0)
    {
        // GET_FILE command
        // NS requests file content for EXEC command
        // Read entire file and send to NS
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", storage_dir, arg);

        FILE *f = fopen(path, "r");
        if (!f)
        {
            if (ss_logger)
            {
                log_message(ss_logger, LOG_ERROR, "GET_FILE failed: file not found: %s", arg);
            }
            write(ns_fd, "ERROR: File not found\n", 22);
            return 1;
        }

        // Read and send file contents
        char buffer[4096];
        size_t bytes_read;
        size_t total_sent = 0;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
        {
            ssize_t sent = write(ns_fd, buffer, bytes_read);
            if (sent < 0)
            {
                if (ss_logger)
                {
                    log_message(ss_logger, LOG_ERROR, "GET_FILE: Failed to send data");
                }
                break;
            }
            total_sent += sent;
        }

        fclose(f);

        // Send EOF marker to indicate end of file
        const char *eof_marker = "\n<<<EOF>>>\n";
        write(ns_fd, eof_marker, strlen(eof_marker));

        if (ss_logger)
        {
            log_message(ss_logger, LOG_INFO, "GET_FILE: Sent %zu bytes for file: %s",
                        total_sent, arg);
        }
    }
    return 1;
}
