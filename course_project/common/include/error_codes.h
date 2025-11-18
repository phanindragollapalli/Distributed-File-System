/*
 * ERROR CODES MODULE HEADER
 * Person 1, Days 1-2 and Day 7
 *
 * Defines error codes used throughout the system for consistent
 * error handling and reporting.
 */

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// Error code enumeration
typedef enum
{
    // Success codes
    SUCCESS = 0,
    ACK_SUCCESS = 1,

    // General error codes
    ERR_GENERAL = -1,
    ERR_INVALID_ARGS = -2,
    ERR_MEMORY = -3,
    ERR_NULL_POINTER = -4,

    // Network error codes
    ERR_SOCKET_CREATE = -100,
    ERR_SOCKET_BIND = -101,
    ERR_SOCKET_LISTEN = -102,
    ERR_SOCKET_CONNECT = -103,
    ERR_SOCKET_ACCEPT = -104,
    ERR_SEND_FAILED = -105,
    ERR_RECV_FAILED = -106,
    ERR_CONNECTION_CLOSED = -107,

    // File operation error codes
    ERR_FILE_NOT_FOUND = -200,
    ERR_FILE_EXISTS = -201,
    ERR_FILE_OPEN = -202,
    ERR_FILE_READ = -203,
    ERR_FILE_WRITE = -204,
    ERR_FILE_DELETE = -205,
    ERR_FILE_LOCKED = -206,
    ERR_INVALID_INDEX = -207,
    ERR_SENTENCE_OUT_OF_RANGE = -208,
    ERR_WORD_OUT_OF_RANGE = -209,

    // Access control error codes
    ERR_UNAUTHORIZED = -300,
    ERR_NO_READ_ACCESS = -301,
    ERR_NO_WRITE_ACCESS = -302,
    ERR_PERMISSION_DENIED = -303,

    // Parse error codes
    ERR_PARSE_FAILED = -400,
    ERR_INVALID_FORMAT = -401
} ErrorCode;

// Function prototypes
const char *error_code_to_string(ErrorCode code);
void send_error_response(int fd, ErrorCode code);
ErrorCode parse_error_response(const char *msg);

#endif
