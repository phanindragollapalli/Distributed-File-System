/*
 * ACCESS CONTROL (GRANT/REVOKE) HEADER
 * Person 1, Days 10-11 - NOT YET IMPLEMENTED (client side)
 *
 * Implements GRANT and REVOKE commands for ACL management.
 * Server-side implementation in ns_acl.c is complete.
 */

#ifndef CLIENT_ACCESS_H
#define CLIENT_ACCESS_H

// Handle GRANT command
int handle_grant_command(const char *command, const char *username);

// Handle REVOKE command
int handle_revoke_command(const char *command, const char *username);

#endif
