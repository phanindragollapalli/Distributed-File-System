/*
 * PROTOCOL MODULE - Person 1, Days 1-2
 *
 * Implements protocol parsing, serialization, and helper functions for
 * communication between Name Server, Storage Servers, and Clients.
 *
 * Provides command parsing, message formatting, and protocol validation
 * for the distributed file system.
 */

#include "../include/protocol.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ========== COMMAND PARSING FUNCTIONS ========== */

/* Parse command type from string
 * Parameters:
 *   command - Command string (e.g., "READ file.txt")
 *   cmd_type - Buffer to store command type (e.g., "READ")
 *   max_len - Maximum length of cmd_type buffer
 * Returns: 0 on success, -1 on failure
 */
int parse_command_type(const char *command, char *cmd_type, size_t max_len)
{
    if (!command || !cmd_type || max_len == 0)
        return -1;

    // Extract first word as command type
    int i = 0;
    while (command[i] && !isspace(command[i]) && i < max_len - 1)
    {
        cmd_type[i] = command[i];
        i++;
    }
    cmd_type[i] = '\0';

    return (i > 0) ? 0 : -1;
}

/* Parse filename from command
 * Parameters:
 *   command - Full command string
 *   filename - Buffer to store filename
 *   max_len - Maximum length of filename buffer
 * Returns: 0 on success, -1 on failure
 */
int parse_filename(const char *command, char *filename, size_t max_len)
{
    if (!command || !filename || max_len == 0)
        return -1;

    // Skip command type
    const char *ptr = command;
    while (*ptr && !isspace(*ptr))
        ptr++;

    // Skip whitespace
    while (*ptr && isspace(*ptr))
        ptr++;

    // Extract filename
    int i = 0;
    while (*ptr && !isspace(*ptr) && i < max_len - 1)
    {
        filename[i++] = *ptr++;
    }
    filename[i] = '\0';

    return (i > 0) ? 0 : -1;
}

/* Parse username from command (for ACL operations)
 * Parameters:
 *   command - Full command string
 *   username - Buffer to store username
 *   max_len - Maximum length of username buffer
 * Returns: 0 on success, -1 on failure
 */
int parse_username(const char *command, char *username, size_t max_len)
{
    if (!command || !username || max_len == 0)
        return -1;

    // Skip command type
    const char *ptr = command;
    while (*ptr && !isspace(*ptr))
        ptr++;

    // Skip whitespace
    while (*ptr && isspace(*ptr))
        ptr++;

    // Skip filename
    while (*ptr && !isspace(*ptr))
        ptr++;

    // Skip whitespace
    while (*ptr && isspace(*ptr))
        ptr++;

    // Extract username
    int i = 0;
    while (*ptr && !isspace(*ptr) && i < max_len - 1)
    {
        username[i++] = *ptr++;
    }
    username[i] = '\0';

    return (i > 0) ? 0 : -1;
}

/* ========== MESSAGE FORMATTING FUNCTIONS ========== */

/* Format VIEW command
 * Format: "VIEW <filename> <username>\n"
 */
int format_view_command(char *buffer, size_t buffer_size,
                        const char *filename, const char *username)
{
    if (!buffer || !filename || !username || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "VIEW %s %s\n", filename, username);
}

/* Format READ command
 * Format: "READ <filename>\n"
 */
int format_read_command(char *buffer, size_t buffer_size, const char *filename)
{
    if (!buffer || !filename || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "READ %s\n", filename);
}

/* Format CREATE command
 * Format: "CREATE <filename>\n"
 */
int format_create_command(char *buffer, size_t buffer_size, const char *filename)
{
    if (!buffer || !filename || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "CREATE %s\n", filename);
}

/* Format DELETE command
 * Format: "DELETE <filename>\n"
 */
int format_delete_command(char *buffer, size_t buffer_size, const char *filename)
{
    if (!buffer || !filename || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "DELETE %s\n", filename);
}

/* Format INFO command
 * Format: "INFO <filename> <username>\n"
 */
int format_info_command(char *buffer, size_t buffer_size,
                        const char *filename, const char *username)
{
    if (!buffer || !filename || !username || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "INFO %s %s\n", filename, username);
}

/* Format LIST command
 * Format: "LIST <username>\n"
 */
int format_list_command(char *buffer, size_t buffer_size, const char *username)
{
    if (!buffer || !username || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "LIST %s\n", username);
}

/* Format EXEC command
 * Format: "EXEC <filename> <username>\n"
 */
int format_exec_command(char *buffer, size_t buffer_size,
                        const char *filename, const char *username)
{
    if (!buffer || !filename || !username || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "EXEC %s %s\n", filename, username);
}

/* Format GRANT command
 * Format: "GRANT <filename> <username> <permission>\n"
 * Permission: "READ" or "WRITE"
 */
int format_grant_command(char *buffer, size_t buffer_size,
                         const char *filename, const char *username,
                         const char *permission)
{
    if (!buffer || !filename || !username || !permission || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "GRANT %s %s %s\n",
                    filename, username, permission);
}

/* Format REVOKE command
 * Format: "REVOKE <filename> <username>\n"
 */
int format_revoke_command(char *buffer, size_t buffer_size,
                          const char *filename, const char *username)
{
    if (!buffer || !filename || !username || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "REVOKE %s %s\n", filename, username);
}

/* Format STREAM command
 * Format: "STREAM <filename> <username>\n"
 */
int format_stream_command(char *buffer, size_t buffer_size,
                          const char *filename, const char *username)
{
    if (!buffer || !filename || !username || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "STREAM %s %s\n", filename, username);
}

/* Format GET_FILE command (NS to SS)
 * Format: "GET_FILE <filename>\n"
 */
int format_get_file_command(char *buffer, size_t buffer_size, const char *filename)
{
    if (!buffer || !filename || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "GET_FILE %s\n", filename);
}

/* ========== RESPONSE FORMATTING FUNCTIONS ========== */

/* Format SS info response
 * Format: "SS_INFO <ip> <port>\n"
 */
int format_ss_info_response(char *buffer, size_t buffer_size,
                            const char *ip, int port)
{
    if (!buffer || !ip || buffer_size == 0)
        return -1;

    return snprintf(buffer, buffer_size, "SS_INFO %s %d\n", ip, port);
}

/* Format ACK response
 * Format: "ACK <message>\n"
 */
int format_ack_response(char *buffer, size_t buffer_size, const char *message)
{
    if (!buffer || buffer_size == 0)
        return -1;

    if (message)
        return snprintf(buffer, buffer_size, "ACK %s\n", message);
    else
        return snprintf(buffer, buffer_size, "ACK\n");
}

/* Format ERROR response
 * Format: "ERROR <message>\n"
 */
int format_error_response(char *buffer, size_t buffer_size, const char *message)
{
    if (!buffer || buffer_size == 0)
        return -1;

    if (message)
        return snprintf(buffer, buffer_size, "ERROR %s\n", message);
    else
        return snprintf(buffer, buffer_size, "ERROR\n");
}

/* ========== PROTOCOL VALIDATION FUNCTIONS ========== */

/* Check if command is valid
 * Returns: 1 if valid, 0 otherwise
 */
int is_valid_command(const char *command)
{
    if (!command)
        return 0;

    // List of valid commands
    const char *valid_commands[] = {
        "VIEW", "READ", "WRITE", "CREATE", "DELETE",
        "INFO", "LIST", "EXEC", "STREAM", "GRANT",
        "REVOKE", "GET_FILE", "UNDO", NULL};

    char cmd_type[64];
    if (parse_command_type(command, cmd_type, sizeof(cmd_type)) < 0)
        return 0;

    for (int i = 0; valid_commands[i] != NULL; i++)
    {
        if (strcmp(cmd_type, valid_commands[i]) == 0)
            return 1;
    }

    return 0;
}

/* Check if response is an error
 * Returns: 1 if error response, 0 otherwise
 */
int is_error_response(const char *response)
{
    if (!response)
        return 0;

    return (strncmp(response, "ERROR", 5) == 0);
}

/* Check if response is an ACK
 * Returns: 1 if ACK response, 0 otherwise
 */
int is_ack_response(const char *response)
{
    if (!response)
        return 0;

    return (strncmp(response, "ACK", 3) == 0);
}

/* ========== UTILITY FUNCTIONS ========== */

/* Trim whitespace from string (in-place)
 * Returns: Pointer to trimmed string
 */
char *trim_whitespace(char *str)
{
    if (!str)
        return NULL;

    // Trim leading whitespace
    while (isspace(*str))
        str++;

    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end))
        end--;

    *(end + 1) = '\0';

    return str;
}

/* Extract error message from error response
 * Parameters:
 *   response - Error response string
 *   error_msg - Buffer to store error message
 *   max_len - Maximum length of error_msg buffer
 * Returns: 0 on success, -1 on failure
 */
int extract_error_message(const char *response, char *error_msg, size_t max_len)
{
    if (!response || !error_msg || max_len == 0)
        return -1;

    if (!is_error_response(response))
        return -1;

    // Skip "ERROR "
    const char *ptr = response + 6;

    // Copy error message
    int i = 0;
    while (*ptr && *ptr != '\n' && i < max_len - 1)
    {
        error_msg[i++] = *ptr++;
    }
    error_msg[i] = '\0';

    return 0;
}

/* Convert MessageType enum to string
 * Returns: String representation of MessageType
 */
const char *message_type_to_string(MessageType type)
{
    switch (type)
    {
    case MSG_HELLO:
        return "MSG_HELLO";
    case MSG_REGISTER_SS:
        return "MSG_REGISTER_SS";
    case MSG_REGISTER_CLIENT:
        return "MSG_REGISTER_CLIENT";
    case MSG_ACK:
        return "MSG_ACK";
    case MSG_ERROR:
        return "MSG_ERROR";
    default:
        return "MSG_UNKNOWN";
    }
}
