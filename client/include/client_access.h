/*
 * ACCESS CONTROL (GRANT/REVOKE) HEADER
 * Implements GRANT and REVOKE commands for ACL management.
 * Server-side implementation in ns_acl.c is complete.
 */
// LLM GENERATED CODE STARTS HERE
#ifndef CLIENT_ACCESS_H
#define CLIENT_ACCESS_H

// Handle GRANT command
int handle_grant_command(const char *command, const char *username);

// Handle REVOKE command
int handle_revoke_command(const char *command, const char *username);

#endif
// LLM GENERATED CODE ENDS HERE