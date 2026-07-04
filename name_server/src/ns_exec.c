/* EXEC COMMAND
 * Execute files stored in distributed file system on Name Server
 *
 * Flow:
 * 1. Client sends EXEC request with filename
 * 2. NS verifies ACL permissions (READ access required)
 * 3. NS requests file content from Storage Server
 * 4. NS executes file content as shell script
 * 5. NS captures output and returns to client
 *
 * Implementation uses popen() to execute commands and capture output
 */

#include "../include/ns_exec.h"
#include "../include/ns_storage.h"
#include "../include/ns_registration.h"
#include "../../common/include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern Logger *global_logger;

/* Request file content from Storage Server
 * Parameters:
 *   ss_info - Storage Server information (IP, port)
 *   filename - Name of file to retrieve
 *   content - Buffer to store file content
 *   max_size - Maximum size of content buffer
 *
 * Returns: Number of bytes read, or -1 on failure
 */
static int get_file_from_ss(StorageServerInfo *ss_info, const char *filename,
                            char *content, size_t max_size)
{
    if (!ss_info || !filename || !content || max_size == 0)
        return -1;

    // Use the existing NS-to-SS connection (ns_fd) instead of creating a new connection
    // Lock to ensure only one command is sent at a time
    pthread_mutex_lock(&ss_info->command_lock);

    int ss_fd = ss_info->ns_fd;

    // Send GET_FILE request to SS
    // Format: "GET_FILE <filename>\n"
    char request[512];
    snprintf(request, sizeof(request), "GET_FILE %s\n", filename);

    if (send(ss_fd, request, strlen(request), 0) < 0)
    {
        log_message(global_logger, LOG_ERROR, "EXEC: Failed to send GET_FILE request");
        pthread_mutex_unlock(&ss_info->command_lock);
        return -1;
    }

    // Receive file content (wait for EOF marker: "\n<<<EOF>>>\n")
    int total_bytes = 0;
    const char *eof_marker = "\n<<<EOF>>>\n";
    int eof_marker_len = strlen(eof_marker);

    while (total_bytes < max_size - 1)
    {
        int n = recv(ss_fd, content + total_bytes, max_size - total_bytes - 1, 0);
        if (n <= 0)
            break;

        total_bytes += n;
        content[total_bytes] = '\0';

        // Check for error response
        if (total_bytes > 6 && strncmp(content, "ERROR:", 6) == 0)
        {
            log_message(global_logger, LOG_ERROR, "EXEC: SS returned error: %s", content + 7);
            pthread_mutex_unlock(&ss_info->command_lock);
            return -1;
        }

        // Check for EOF marker
        if (total_bytes >= eof_marker_len)
        {
            if (strcmp(content + total_bytes - eof_marker_len, eof_marker) == 0)
            {
                // Remove EOF marker from content
                total_bytes -= eof_marker_len;
                content[total_bytes] = '\0';
                break;
            }
        }
    }

    pthread_mutex_unlock(&ss_info->command_lock);

    log_message(global_logger, LOG_INFO, "EXEC: Retrieved %d bytes from SS for file '%s'",
                total_bytes, filename);

    return total_bytes;
}

/* Execute file content as shell script and capture output
 *
 * Parameters:
 *   file_content - Content of the file to execute
 *   output - Buffer to store execution output
 *   output_size - Maximum size of output buffer
 *
 * Returns: 0 on success, -1 on failure
 */
static int execute_script(const char *file_content, char *output, size_t output_size)
{
    if (!file_content || !output || output_size == 0)
        return -1;

    // Create temporary file for script execution
    char temp_filename[256];
    snprintf(temp_filename, sizeof(temp_filename), "/tmp/ns_exec_%d.sh", getpid());

    FILE *temp_file = fopen(temp_filename, "w");
    if (!temp_file)
    {
        log_message(global_logger, LOG_ERROR, "EXEC: Failed to create temp file");
        snprintf(output, output_size, "ERROR: Failed to create temporary script file\n");
        return -1;
    }

    // Write content to temp file
    fprintf(temp_file, "%s", file_content);
    fclose(temp_file);

    // Make script executable
    char chmod_cmd[300];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", temp_filename);
    system(chmod_cmd);

    // Execute script and capture output
    char exec_cmd[300];
    snprintf(exec_cmd, sizeof(exec_cmd), "%s 2>&1", temp_filename);

    FILE *pipe = popen(exec_cmd, "r");
    if (!pipe)
    {
        log_message(global_logger, LOG_ERROR, "EXEC: Failed to execute script");
        unlink(temp_filename);
        snprintf(output, output_size, "ERROR: Failed to execute script\n");
        return -1;
    }

    // Read output from pipe using read() for more reliable large output handling
    int fd = fileno(pipe);
    size_t bytes_read = 0;

    while (bytes_read < output_size - 1)
    {
        ssize_t n = read(fd, output + bytes_read, output_size - bytes_read - 1);
        if (n > 0)
        {
            bytes_read += n;
        }
        else if (n == 0)
        {
            // EOF
            break;
        }
        else
        {
            // Error
            if (errno != EINTR && errno != EAGAIN)
                break;
        }
    }
    output[bytes_read] = '\0';

    int status = pclose(pipe);

    // Clean up temp file
    unlink(temp_filename);

    if (status != 0)
    {
        log_message(global_logger, LOG_WARN, "EXEC: Script exited with status %d", status);
    }
    else
    {
        log_message(global_logger, LOG_INFO, "EXEC: Script executed successfully, output size=%zu", bytes_read);
    }

    return 0;
}

/* Main execute_file function - retrieves file from SS and executes it
 * This is called by the EXEC request handler in ns_main.c
 *
 * Parameters:
 *   ss_info - Storage Server information
 *   filename - Name of file to execute
 *   output - Buffer to store execution output
 *   output_size - Maximum size of output buffer
 *   ss_id - Storage Server ID (for logging)
 *
 * Returns: 0 on success, -1 on failure
 */
int execute_file_from_ss(StorageServerInfo *ss_info, const char *filename,
                         char *output, size_t output_size, int ss_id)
{
    if (!ss_info || !filename || !output || output_size == 0)
    {
        if (output && output_size > 0)
            snprintf(output, output_size, "ERROR: Invalid parameters\n");
        return -1;
    }

    log_message(global_logger, LOG_INFO, "EXEC: Executing file '%s' from SS_%d",
                filename, ss_id);

    // Step 1: Get file content from Storage Server
    char file_content[65536]; // 64KB max file size for execution
    int content_size = get_file_from_ss(ss_info, filename, file_content, sizeof(file_content));

    if (content_size < 0)
    {
        snprintf(output, output_size, "ERROR: Failed to retrieve file from Storage Server\n");
        return -1;
    }

    if (content_size == 0)
    {
        snprintf(output, output_size, "ERROR: File is empty\n");
        return -1;
    }

    // Step 2: Execute the script
    int result = execute_script(file_content, output, output_size);

    return result;
}

/* Legacy function for compatibility - kept as stub */
int execute_file(const char *filename, char *output, size_t output_size)
{
    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "EXEC: execute_file called (deprecated) - %s", filename);
    }

    if (output && output_size > 0)
    {
        snprintf(output, output_size, "ERROR: Use execute_file_from_ss instead\n");
    }

    return -1;
}
