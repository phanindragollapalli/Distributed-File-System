/* EXEC COMMAND - Person 1, Day 12
 * Execute files stored in the distributed file system on Name Server
 *
 * The EXEC command allows executing script files (e.g., shell scripts, Python scripts)
 * stored in the distributed file system. The flow is:
 * 1. Client sends EXEC request to Name Server with filename
 * 2. NS verifies ACL permissions (user must have READ access)
 * 3. NS requests file content from appropriate Storage Server
 * 4. NS executes the file content as a shell command
 * 5. NS captures output and sends back to client
 * 6. Client displays the execution output
 *
 * Usage: EXEC <filename>
 * Example: EXEC script.sh
 */

#include "../include/client_exec.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* EXEC COMMAND HANDLER
 * Sends EXEC request to Name Server and displays execution output
 *
 * Parameters:
 *   command  - Full command string (e.g., "EXEC script.sh")
 *   username - Current user's username (for ACL verification)
 *
 * Returns: 0 on success, -1 on failure
 */
int handle_exec_command(const char *command, const char *username)
{
    // Parse command: "EXEC <filename>"
    char filename[256];
    if (sscanf(command, "EXEC %255s", filename) != 1)
    {
        printf("Error: Invalid EXEC command format\n");
        printf("Usage: EXEC <filename>\n");
        printf("Example: EXEC script.sh\n");
        return -1;
    }

    printf("Executing file: %s\n", filename);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send EXEC request with username for ACL check
    // Format: "EXEC <filename> <username>\n"
    char request[512];
    snprintf(request, sizeof(request), "EXEC %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send EXEC request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    // Response can be:
    // - "ERROR: <message>" for errors
    // - "EXEC_OUTPUT\n<output>" for successful execution
    printf("\n--- Execution Output ---\n");

    char buffer[4096];
    int total_bytes = 0;
    int is_first_chunk = 1;

    while (1)
    {
        int n = recv_from_ns(ns_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0)
            break;

        buffer[n] = '\0';

        // Check for error on first chunk
        if (is_first_chunk)
        {
            is_first_chunk = 0;
            if (strncmp(buffer, "ERROR:", 6) == 0)
            {
                printf("%s\n", buffer + 7);
                close(ns_fd);
                return -1;
            }

            // Skip "EXEC_OUTPUT\n" header if present
            char *output_start = strstr(buffer, "EXEC_OUTPUT\n");
            if (output_start)
            {
                output_start += 12; // Skip "EXEC_OUTPUT\n"
                printf("%s", output_start);
                total_bytes += strlen(output_start);
            }
            else
            {
                printf("%s", buffer);
                total_bytes += n;
            }
        }
        else
        {
            printf("%s", buffer);
            total_bytes += n;
        }

        // If we received less than buffer size, we're done
        if (n < sizeof(buffer) - 1)
            break;
    }

    printf("\n--- End of Output ---\n");

    if (total_bytes == 0)
    {
        printf("(No output from command)\n");
    }
    else
    {
        printf("Total output: %d bytes\n", total_bytes);
    }

    close(ns_fd);
    return 0;
}
