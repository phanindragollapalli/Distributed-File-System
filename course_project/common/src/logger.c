/*
 * LOGGER MODULE - Person 1, Day 1-2 and Day 7
 * Provides comprehensive logging functionality for Name Server, Storage Server, and Client
 * Features: Timestamps, log levels, dual output (file and terminal), request/response tracking
 */

#include "../include/logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

/* Convert log level enum to string representation for display */
static const char *log_level_string(LogLevel level)
{
    switch (level)
    {
    case LOG_DEBUG:
        return "DEBUG";
    case LOG_INFO:
        return "INFO";
    case LOG_WARN:
        return "WARN";
    case LOG_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

/* Generate timestamp string in format: YYYY-MM-DD HH:MM:SS */
static char *get_timestamp()
{
    static char buffer[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

/* Initialize logger with file path, component name, minimum log level, and terminal output option */
Logger *logger_init(const char *log_path, const char *component, LogLevel min_level, int enable_terminal)
{
    Logger *logger = (Logger *)malloc(sizeof(Logger));
    logger->log_file = fopen(log_path, "a");
    if (!logger->log_file)
    {
        fprintf(stderr, "Failed to open log file: %s\n", log_path);
        free(logger);
        return NULL;
    }
    logger->min_level = min_level;
    logger->enable_terminal = enable_terminal;
    strncpy(logger->component, component, sizeof(logger->component) - 1);

    log_message(logger, LOG_INFO, "Logger initialized for %s", component);
    return logger;
}

/* Cleanup logger resources and close log file */
void logger_close(Logger *logger)
{
    if (logger)
    {
        if (logger->log_file)
        {
            fclose(logger->log_file);
        }
        free(logger);
    }
}

/* Core logging function: formats message with timestamp and component info, writes to file and optionally terminal */
void log_message(Logger *logger, LogLevel level, const char *format, ...)
{
    if (!logger || level < logger->min_level)
        return;

    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    char *timestamp = get_timestamp();
    char log_line[1280];
    snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s] %s\n",
             timestamp, logger->component, log_level_string(level), message);

    /* Write to log file with immediate flush for real-time logging */
    if (logger->log_file)
    {
        fprintf(logger->log_file, "%s", log_line);
        fflush(logger->log_file);
    }

    /* Also display on terminal if enabled for debugging */
    if (logger->enable_terminal)
    {
        printf("%s", log_line);
    }
}

/* Log incoming requests with username, IP, port, and request details */
void log_request(Logger *logger, const char *username, const char *ip, int port, const char *request)
{
    log_message(logger, LOG_INFO, "REQUEST from %s@%s:%d - %s",
                username ? username : "unknown", ip, port, request);
}

/* Log responses with error code and details, automatically choosing log level based on success/failure */
void log_response(Logger *logger, const char *username, ErrorCode code, const char *details)
{
    LogLevel level = (code == SUCCESS) ? LOG_INFO : LOG_ERROR;
    log_message(logger, level, "RESPONSE to %s - %s: %s",
                username ? username : "unknown", error_code_to_string(code), details);
}

/* Log new connections from clients or storage servers */
void log_connection(Logger *logger, const char *type, const char *ip, int port)
{
    log_message(logger, LOG_INFO, "CONNECTION %s from %s:%d", type, ip, port);
}
