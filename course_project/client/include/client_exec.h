/*
 * EXEC COMMAND HEADER
 * Person 1, Day 12 - NOT YET IMPLEMENTED
 *
 * Executes files stored in the distributed file system.
 * Server-side has partial implementation in ns_exec.c.
 */

#ifndef CLIENT_EXEC_H
#define CLIENT_EXEC_H

// Handle EXEC command
int handle_exec_command(const char *command, const char *username);

#endif
