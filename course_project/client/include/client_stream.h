/*
 * STREAM COMMAND HEADER
 * Implements file streaming functionality - displays file content word-by-word
 * with 0.1 second delay between words for a streaming effect.
 *
 * The client establishes direct connection with Storage Server and fetches
 * content word-by-word. Handles SS disconnection gracefully.
 */

#ifndef CLIENT_STREAM_H
#define CLIENT_STREAM_H

/* Handle STREAM command
 * Connects to NS to get SS info, then streams file content from SS
 * Parameters:
 *   filename - Name of file to stream
 *   username - Username for ACL permission check
 * Returns: 0 on success, -1 on failure
 */
int handle_stream_command(const char *filename, const char *username);

#endif
