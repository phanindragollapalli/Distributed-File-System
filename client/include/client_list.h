/*
 * LIST COMMAND HEADER
 * Lists all users registered in the system.
 *
 * Functionality:
 * - Queries Name Server for all registered users
 * - Displays formatted list of usernames
 * - Shows total count of registered users
 *
 * Flow:
 * 1. Client sends LIST request to Name Server
 * 2. NS returns USER_LIST with all usernames
 * 3. Client parses and displays formatted list
 * 4. Shows count of total users
 *
 * Output Format:
 * === Registered Users ===
 * --> user1
 * --> user2
 * --> kaevi
 *
 * Total users: 3
 */

 // LLM GENERATED CODE STARTS HERE
#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

/* Handle LIST command
 * Retrieves and displays all users in the system
 *
 * Parameters:
 *   username - Current username (not used for LIST, but kept for consistency)
 *
 * Returns: 0 on success, -1 on failure
 */
int handle_list_command(const char *username);

/* Legacy function for backward compatibility
 * Kept for compatibility with older code
 *
 * Parameters:
 *   ns_sock - Name Server socket file descriptor
 */
void handle_list(int ns_sock);

#endif
// LLM GENERATED CODE ENDS HERE