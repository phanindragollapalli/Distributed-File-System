#include "../include/error_codes.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

const char *error_code_to_string(ErrorCode code)
{
    switch (code)
    {
    case ERR_SUCCESS:
        return "SUCCESS";
    case ERR_UNAUTHORIZED:
        return "UNAUTHORIZED";
    case ERR_FILE_NOT_FOUND:
        return "FILE_NOT_FOUND";
    case ERR_FILE_LOCKED:
        return "FILE_LOCKED";
    case ERR_PERMISSION_DENIED:
        return "PERMISSION_DENIED";
    case ERR_INVALID_OPERATION:
        return "INVALID_OPERATION";
    case ERR_NETWORK_FAILURE:
        return "NETWORK_FAILURE";
    case ERR_SS_UNAVAILABLE:
        return "STORAGE_SERVER_UNAVAILABLE";
    case ERR_NS_UNAVAILABLE:
        return "NAME_SERVER_UNAVAILABLE";
    case ERR_FILE_EXISTS:
        return "FILE_ALREADY_EXISTS";
    case ERR_INVALID_PATH:
        return "INVALID_PATH";
    case ERR_DISK_FULL:
        return "DISK_FULL";
    case ERR_TIMEOUT:
        return "TIMEOUT";
    case ERR_INVALID_REQUEST:
        return "INVALID_REQUEST";
    case ERR_INTERNAL_ERROR:
        return "INTERNAL_ERROR";
    case ERR_ACL_VIOLATION:
        return "ACCESS_CONTROL_VIOLATION";
    case ERR_MAX_CLIENTS:
        return "MAX_CLIENTS_REACHED";
    case ERR_REGISTRATION_FAILED:
        return "REGISTRATION_FAILED";
    default:
        return "UNKNOWN_ERROR";
    }
}

void send_error_response(int fd, ErrorCode code)
{
    char msg[256];
    snprintf(msg, sizeof(msg), "ERROR %d %s\n", code, error_code_to_string(code));
    write(fd, msg, strlen(msg));
}

ErrorCode parse_error_response(const char *msg)
{
    int code;
    if (sscanf(msg, "ERROR %d", &code) == 1)
    {
        return (ErrorCode)code;
    }
    return ERR_INTERNAL_ERROR;
}
