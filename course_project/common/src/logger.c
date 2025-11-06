#include "../include/logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

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

static char *get_timestamp()
{
    static char buffer[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

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

    // Write to file
    if (logger->log_file)
    {
        fprintf(logger->log_file, "%s", log_line);
        fflush(logger->log_file);
    }

    // Write to terminal if enabled
    if (logger->enable_terminal)
    {
        printf("%s", log_line);
    }
}

void log_request(Logger *logger, const char *username, const char *ip, int port, const char *request)
{
    log_message(logger, LOG_INFO, "REQUEST from %s@%s:%d - %s",
                username ? username : "unknown", ip, port, request);
}

void log_response(Logger *logger, const char *username, ErrorCode code, const char *details)
{
    LogLevel level = (code == ERR_SUCCESS) ? LOG_INFO : LOG_ERROR;
    log_message(logger, level, "RESPONSE to %s - %s: %s",
                username ? username : "unknown", error_code_to_string(code), details);
}

void log_connection(Logger *logger, const char *type, const char *ip, int port)
{
    log_message(logger, LOG_INFO, "CONNECTION %s from %s:%d", type, ip, port);
}
