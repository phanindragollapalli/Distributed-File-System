#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include "constants.h"
// LLM GENERATED CODE STARTS HERE
// Ports are defined in constants.h
// NS_PORT = 8080

// Define message types
typedef enum
{
    MSG_HELLO,
    MSG_REGISTER_SS,
    MSG_REGISTER_CLIENT,
    MSG_ACK,
    MSG_ERROR
} MessageType;

/* Parse command components */
int parse_command_type(const char *command, char *cmd_type, size_t max_len);
int parse_filename(const char *command, char *filename, size_t max_len);
int parse_username(const char *command, char *username, size_t max_len);

/* MESSAGE FORMATTING FUNCTIONS */

/* Client command formatting */
int format_view_command(char *buffer, size_t buffer_size,
                        const char *filename, const char *username);
int format_read_command(char *buffer, size_t buffer_size, const char *filename);
int format_create_command(char *buffer, size_t buffer_size, const char *filename);
int format_delete_command(char *buffer, size_t buffer_size, const char *filename);
int format_info_command(char *buffer, size_t buffer_size,
                        const char *filename, const char *username);
int format_list_command(char *buffer, size_t buffer_size, const char *username);
int format_exec_command(char *buffer, size_t buffer_size,
                        const char *filename, const char *username);
int format_grant_command(char *buffer, size_t buffer_size,
                         const char *filename, const char *username,
                         const char *permission);
int format_revoke_command(char *buffer, size_t buffer_size,
                          const char *filename, const char *username);
int format_stream_command(char *buffer, size_t buffer_size,
                          const char *filename, const char *username);

/* NS to SS command formatting */
int format_get_file_command(char *buffer, size_t buffer_size, const char *filename);

/* Response formatting */
int format_ss_info_response(char *buffer, size_t buffer_size,
                            const char *ip, int port);
int format_ack_response(char *buffer, size_t buffer_size, const char *message);
int format_error_response(char *buffer, size_t buffer_size, const char *message);

/* Validation functions */
int is_valid_command(const char *command);
int is_error_response(const char *response);
int is_ack_response(const char *response);

/* String utilities */
char *trim_whitespace(char *str);
int extract_error_message(const char *response, char *error_msg, size_t max_len);

/* Type conversion */
const char *message_type_to_string(MessageType type);

#endif

// LLM GENERATED CODE ENDS HERE