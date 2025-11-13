/*
 * VIEW COMMAND HEADER
 * Person 1, Days 8-9 - NOT YET IMPLEMENTED
 *
 * Displays file contents to user. Contacts Name Server to locate file,
 * then connects to Storage Server to retrieve and display contents.
 */

#ifndef CLIENT_VIEW_H
#define CLIENT_VIEW_H

// Handle VIEW command
int handle_view_command(const char *command, const char *username);
void handle_view(int ns_sock, char *cmd);

#endif
