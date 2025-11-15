/*
 * FILE OPERATIONS HEADER
 * Person 2, Days 3-10
 *
 * Implements core file operations: CREATE, READ, WRITE, DELETE, UNDO
 *
 * These operations form the foundation of the distributed file system:
 * - CREATE: Creates new empty files with owner ACL
 * - READ: Retrieves complete file content from Storage Server
 * - WRITE: Modifies files with sentence-level locking and concurrency control
 * - DELETE: Removes files and cleans up metadata/ACL
 * - UNDO: Reverts last change to a file (file-specific, not user-specific)
 *
 * All operations go through Name Server for routing and ACL checks,
 * then connect directly to Storage Server for data operations.
 */

#ifndef CLIENT_FILE_OPS_H
#define CLIENT_FILE_OPS_H

/* CREATE command - Person 2, Days 3-4
 * Creates a new empty file on Storage Server
 * Parameters:
 *   filename - Name of file to create
 *   username - Creator's username (becomes owner)
 * Returns: 0 on success, -1 on failure
 */
int handle_create_command(const char *filename, const char *username);

/* READ command - Person 2, Days 3-4
 * Reads and displays complete file content
 * Parameters:
 *   filename - Name of file to read
 *   username - Reader's username (for ACL check)
 * Returns: 0 on success, -1 on failure
 */
int handle_read_command(const char *filename, const char *username);

/* WRITE command - Person 2, Days 5-7
 * Writes to file with sentence-level locking
 * Uses protocol: WRITE -> word updates -> ETIRW
 * Parameters:
 *   filename - Name of file to write
 *   username - Writer's username (for ACL check)
 * Returns: 0 on success, -1 on failure
 */
int handle_write_command(const char *filename, const char *username);

/* DELETE command - Person 2, Day 10
 * Deletes file and cleans up all metadata
 * Parameters:
 *   filename - Name of file to delete
 *   username - User requesting deletion (for ACL check)
 * Returns: 0 on success, -1 on failure
 */
int handle_delete_command(const char *filename, const char *username);

/* UNDO command - Person 2, Days 8-9
 * Reverts last change to a file (file-specific, not user-specific)
 * Parameters:
 *   filename - Name of file to undo
 *   username - User requesting undo (for ACL check)
 * Returns: 0 on success, -1 on failure
 */
int handle_undo_command(const char *filename, const char *username);

#endif
