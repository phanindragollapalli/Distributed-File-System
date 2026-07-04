/*LOGGER MODULE HEADER
 * Provides logging infrastructure with multiple log levels,
 * file and terminal output, and timestamp support.
 */

 // LLM GENERATED CODE STARTS HERE
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include "error_codes.h"

typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

typedef struct
{
    FILE *log_file;
    LogLevel min_level;
    int enable_terminal;
    char component[32];
} Logger;

// Logger lifecycle
Logger *logger_init(const char *log_path, const char *component, LogLevel min_level, int enable_terminal);
void logger_close(Logger *logger);

// Logging functions
void log_message(Logger *logger, LogLevel level, const char *format, ...);
void log_request(Logger *logger, const char *username, const char *ip, int port, const char *request);
void log_response(Logger *logger, const char *username, ErrorCode code, const char *details);
void log_connection(Logger *logger, const char *type, const char *ip, int port);

#endif

// LLM GENERATED CODE ENDS HERE