/*
 * LIST COMMAND HEADER
 * Person 1, Days 8-9 - NOT YET IMPLEMENTED
 *
 * Lists all accessible files for the user. Queries Name Server
 * for file list based on user permissions.
 */

#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

// Handle LIST command
int handle_list_command(const char *username);
void handle_list(int ns_sock);

#endif
