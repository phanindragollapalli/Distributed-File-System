#ifndef CLIENT_FILE_OPS_H
#define CLIENT_FILE_OPS_H

// Handle CREATE command
int handle_create_command(const char *filename, const char *username);

// Handle DELETE command
int handle_delete_command(const char *filename, const char *username);

#endif
