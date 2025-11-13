/*
 * FILE OPERATIONS HEADER (CREATE/READ/WRITE/DELETE)
 * Person 2 Responsibility - NOT Person 1's work
 *
 * Handles file creation, reading, writing, and deletion operations.
 * These are Person 2's commands, not Person 1's.
 */

#ifndef CLIENT_FILE_OPS_H
#define CLIENT_FILE_OPS_H

// Handle CREATE command
int handle_create_command(const char *filename, const char *username);

// Handle DELETE command
int handle_delete_command(const char *filename, const char *username);

#endif
