/*
 * INFO COMMAND HEADER
 *
 * Displays detailed file metadata including:
 * - Filename
 * - Owner (username who created the file)
 * - File size (in bytes and KB)
 * - Access permissions (READ, WRITE, or READ,WRITE)
 * - Storage Server ID
 * - Timestamps: Created, Modified, Last Accessed
 *
 * Flow:
 * 1. Client sends INFO request to Name Server with filename and username
 * 2. NS checks ACL (user must have at least READ or WRITE permission)
 * 3. NS retrieves file metadata from metadata storage
 * 4. NS sends formatted metadata response to client
 * 5. Client displays formatted information
 *
 * The INFO command provides comprehensive insights into file properties
 * and access rights, helping users understand file status and permissions.
 */

#ifndef CLIENT_INFO_H
#define CLIENT_INFO_H

/* Handle INFO command
 * Retrieves and displays detailed file metadata
 *
 * Parameters:
 *   filename - Name of file to get info for
 *   username - Username for ACL permission check
 *
 * Returns: 0 on success, -1 on failure
 *
 * Note: User must have at least READ or WRITE permission to view file info
 */
int handle_info_command(const char *filename, const char *username);

/* Legacy function for backward compatibility
 * Kept for compatibility with older code
 *
 * Parameters:
 *   ns_sock - Name Server socket file descriptor
 *   cmd - Command string containing "INFO <filename>"
 */
void handle_info(int ns_sock, char *cmd);

#endif
