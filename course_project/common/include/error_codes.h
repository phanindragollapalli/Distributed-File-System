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
    ERR_SUCCESS = 0,
    ERR_UNAUTHORIZED = 1,
    ERR_FILE_NOT_FOUND = 2,
    ERR_FILE_LOCKED = 3,
    ERR_PERMISSION_DENIED = 4,
    ERR_INVALID_OPERATION = 5,
    ERR_NETWORK_FAILURE = 6,
    ERR_SS_UNAVAILABLE = 7,
    ERR_NS_UNAVAILABLE = 8,
    ERR_FILE_EXISTS = 9,
    ERR_INVALID_PATH = 10,
    ERR_DISK_FULL = 11,
    ERR_TIMEOUT = 12,
    ERR_INVALID_REQUEST = 13,
    ERR_INTERNAL_ERROR = 14,
    ERR_ACL_VIOLATION = 15,
    ERR_MAX_CLIENTS = 16,
    ERR_REGISTRATION_FAILED = 17
} ErrorCode;

// Function prototypes
const char *error_code_to_string(ErrorCode code);
void send_error_response(int fd, ErrorCode code);
ErrorCode parse_error_response(const char *msg);

#endif
