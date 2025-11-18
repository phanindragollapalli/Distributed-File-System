/*
 * VIEW COMMAND HEADER
 *
 * Lists files available to the user based on access permissions.
 * Supports flags: -a (all files), -l (detailed info), -al (both)
 */

#ifndef CLIENT_VIEW_H
#define CLIENT_VIEW_H

// Handle VIEW command
int handle_view_command(const char *command, const char *username);
void handle_view(int ns_sock, char *cmd);

#endif
