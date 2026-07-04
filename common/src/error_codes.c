#include "../include/error_codes.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// LLM GENERATED CODE STARTS HERE
/* Convert error code enum to human-readable string for logging and display */
const char *error_code_to_string(ErrorCode code)
{
    switch (code)
    {
    // Success codes
    case SUCCESS:
        return "SUCCESS";
    case ACK_SUCCESS:
        return "ACK_SUCCESS";

    // General error codes
    case ERR_GENERAL:
        return "GENERAL_ERROR";
    case ERR_INVALID_ARGS:
        return "INVALID_ARGUMENTS";
    case ERR_MEMORY:
        return "MEMORY_ERROR";
    case ERR_NULL_POINTER:
        return "NULL_POINTER";

    // Network error codes
    case ERR_SOCKET_CREATE:
        return "SOCKET_CREATE_FAILED";
    case ERR_SOCKET_BIND:
        return "SOCKET_BIND_FAILED";
    case ERR_SOCKET_LISTEN:
        return "SOCKET_LISTEN_FAILED";
    case ERR_SOCKET_CONNECT:
        return "SOCKET_CONNECT_FAILED";
    case ERR_SOCKET_ACCEPT:
        return "SOCKET_ACCEPT_FAILED";
    case ERR_SEND_FAILED:
        return "SEND_FAILED";
    case ERR_RECV_FAILED:
        return "RECV_FAILED";
    case ERR_CONNECTION_CLOSED:
        return "CONNECTION_CLOSED";

    // File operation error codes
    case ERR_FILE_NOT_FOUND:
        return "FILE_NOT_FOUND";
    case ERR_FILE_EXISTS:
        return "FILE_ALREADY_EXISTS";
    case ERR_FILE_OPEN:
        return "FILE_OPEN_FAILED";
    case ERR_FILE_READ:
        return "FILE_READ_FAILED";
    case ERR_FILE_WRITE:
        return "FILE_WRITE_FAILED";
    case ERR_FILE_DELETE:
        return "FILE_DELETE_FAILED";
    case ERR_FILE_LOCKED:
        return "FILE_LOCKED";
    case ERR_INVALID_INDEX:
        return "INVALID_INDEX";
    case ERR_SENTENCE_OUT_OF_RANGE:
        return "SENTENCE_OUT_OF_RANGE";
    case ERR_WORD_OUT_OF_RANGE:
        return "WORD_OUT_OF_RANGE";

    // Access control error codes
    case ERR_UNAUTHORIZED:
        return "UNAUTHORIZED";
    case ERR_NO_READ_ACCESS:
        return "NO_READ_ACCESS";
    case ERR_NO_WRITE_ACCESS:
        return "NO_WRITE_ACCESS";
    case ERR_PERMISSION_DENIED:
        return "PERMISSION_DENIED";

    // Parse error codes
    case ERR_PARSE_FAILED:
        return "PARSE_FAILED";
    case ERR_INVALID_FORMAT:
        return "INVALID_FORMAT";

    default:
        return "UNKNOWN_ERROR";
    }
}

/* Send error response message to client/server over socket connection */
void send_error_response(int fd, ErrorCode code)
{
    char msg[256];
    snprintf(msg, sizeof(msg), "ERROR %d %s\n", code, error_code_to_string(code));
    write(fd, msg, strlen(msg));
}

/* Parse error response string received from socket to extract error code */
ErrorCode parse_error_response(const char *msg)
{
    int code;
    if (sscanf(msg, "ERROR %d", &code) == 1)
    {
        return (ErrorCode)code;
    }
    return ERR_GENERAL;
}

// LLM GENERATED CODE ENDS HERE