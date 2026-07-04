/*
 * EXEC COMMAND HEADER
 *
 * Executes files stored in the distributed file system on Name Server.
 *
 * Flow:
 * 1. Client sends EXEC request to Name Server with filename
 * 2. NS verifies ACL permissions (user must have READ access)
 * 3. NS requests file content from appropriate Storage Server
 * 4. NS executes the file content as shell commands
 * 5. NS captures output (stdout/stderr) and sends back to client
 * 6. Client displays the execution output
 *
 * Note: Execution happens on Name Server, not on client machine.
 * This allows centralized execution with proper access control.
 */

 // LLM GENERATED CODE STARTS HERE
#ifndef CLIENT_EXEC_H
#define CLIENT_EXEC_H

/* Handle EXEC command
 * Sends EXEC request to Name Server and displays execution output
 *
 * Parameters:
 *   command  - Full command string (e.g., "EXEC script.sh")
 *   username - Current user's username (for ACL verification)
 *
 * Returns: 0 on success, -1 on failure
 */
int handle_exec_command(const char *command, const char *username);

#endif

// LLM GENERATED CODE ENDS HERE