/*
LIST COMMAND HEADER
 * Lists all files accessible to the user based on ACL permissions.
 *
 * Functionality:
 * - Queries Name Server for files the user can access
 * - Filters files based on ACL (READ or WRITE permissions)
 * - Displays formatted table with filename and permissions
 * - Shows total count of accessible files
 *
 * Flow:
 * 1. Client sends LIST request with username to Name Server
 * 2. NS queries ACL system to find all files user has access to
 * 3. NS returns FILE_LIST with filename and permission pairs
 * 4. Client parses and displays formatted table
 * 5. Shows count of accessible files
 *
 * Output Format:
 * Filename                                 Permissions
 * ---------------------------------------- ---------------
 * example.txt                              READ,WRITE
 * data.csv                                 READ
 *
 * The LIST command helps users discover which files they can
 * access based on their granted permissions in the ACL system.
 */

#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

/* Handle LIST command
 * Retrieves and displays all files accessible to the user
 *
 * Parameters:
 *   username - Username for ACL filtering
 *
 * Returns: 0 on success, -1 on failure
 *
 * Note: Only files where user has READ or WRITE permission are shown
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
