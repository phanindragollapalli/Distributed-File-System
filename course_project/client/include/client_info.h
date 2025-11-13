/*
 * INFO COMMAND HEADER
 * Person 1, Days 8-9 - NOT YET IMPLEMENTED
 *
 * Displays file metadata (size, permissions, owner, timestamps).
 * Queries Name Server for file metadata.
 */

#ifndef CLIENT_INFO_H
#define CLIENT_INFO_H

// Handle INFO command
int handle_info_command(const char *filename, const char *username);
void handle_info(int ns_sock, char *cmd);

#endif
