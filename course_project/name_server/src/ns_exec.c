#include "../include/ns_exec.h"
#include "../../common/include/logger.h"
#include <stdio.h>
#include <string.h>

extern Logger *global_logger;

// Execute file contents as shell commands
int execute_file(const char *filename, char *output, size_t output_size)
{
    // To be fully implemented in Week 2 (Day 12)
    // Steps:
    // 1. Get SS for file using routing
    // 2. Request file content from SS
    // 3. Execute contents using popen() or system()
    // 4. Capture output
    // 5. Return output to client

    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "EXEC: execute_file called (stub) - %s", filename);
    }

    if (output && output_size > 0)
    {
        snprintf(output, output_size, "EXEC command not yet implemented (Week 2 Day 12)\n");
    }

    return -1;
}
