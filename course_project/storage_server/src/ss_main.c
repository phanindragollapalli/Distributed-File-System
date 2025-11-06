#include <stdio.h>
#include "../../common/include/protocol.h"
#include "../include/ss_network.h"
#include "../include/ss_registration.h"
#include "../include/ss_commands.h"
#include "../include/ss_persistence.h"
#include "../../common/include/logger.h"
#include "../../common/include/error_codes.h"
Logger *ss_logger = NULL;

int main()
{
    // Initialize logger
    ss_logger = logger_init("logs/ss.log", "StorageServer", LOG_DEBUG, 1);
    if (!ss_logger)
    {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    log_message(ss_logger, LOG_INFO, "Starting Storage Server");

    // Initialize persistence system
    if (init_persistence("data") < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize persistence");
        logger_close(ss_logger);
        return 1;
    }

    // Connect to NS
    int ns_fd = ss_connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to connect to Name Server");
        logger_close(ss_logger);
        return 1;
    }

    log_connection(ss_logger, "CONNECTED", "127.0.0.1", NS_PORT);

    // Register with NS
    if (register_with_ns(ns_fd, "127.0.0.1", SS_PORT, SS_PORT + 1))
    {
        log_message(ss_logger, LOG_INFO, "Successfully registered with Name Server");
    }
    else
    {
        log_message(ss_logger, LOG_ERROR, "Registration with Name Server failed");
    }

    send_file_list_to_ns(ns_fd, "storage/files");
    log_message(ss_logger, LOG_INFO, "Sent file list to Name Server");

    // Main loop
    log_message(ss_logger, LOG_INFO, "Storage Server ready, handling commands");
    while (1)
    {
        int result = ss_handle_ns_command(ns_fd, "storage/files");
        if (result < 0)
        {
            log_message(ss_logger, LOG_ERROR, "Connection to NS lost");
            break;
        }
    }

    logger_close(ss_logger);
    return 0;
}
