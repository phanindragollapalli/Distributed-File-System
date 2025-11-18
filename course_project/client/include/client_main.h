/*
 * CLIENT MAIN HEADER
 * Main entry point for the Distributed File System Client application.
 * Handles user authentication, Name Server connection, and interactive
 * command-line interface for file system operations.
 *
 * Functionality:
 * - User authentication (username input)
 * - Name Server connection and client registration
 * - Interactive command loop with prompt
 * - Command execution through execute_command()
 * - Graceful exit handling
 *
 * Startup Flow:
 * 1. Display welcome banner
 * 2. Prompt user for username
 * 3. Connect to Name Server (127.0.0.1:NS_PORT)
 * 4. Send REGISTER_CLIENT message with username, IP, and port
 * 5. Wait for ACK from Name Server
 * 6. Display available commands
 * 7. Enter interactive command loop
 *
 * Command Loop:
 * - Displays prompt: "username> "
 * - Reads user input (fgets)
 * - Strips newline characters
 * - Handles EXIT command for graceful shutdown
 * - Delegates command execution to execute_command()
 *
 * Available Commands:
 * - VIEW [-a] [-l] [-al]  : List files with optional flags
 * - LIST                  : List all accessible files
 * - INFO <filename>       : Display file metadata
 * - CREATE <filename>     : Create new file
 * - DELETE <filename>     : Delete existing file
 * - READ <filename>       : Read file contents
 * - WRITE <filename>      : Write to file
 * - UNDO <filename>       : Undo last change
 * - STREAM <filename>     : Stream file word-by-word
 * - EXEC <filename>       : Execute file as script
 * - GRANT <user> <file> <perm> : Grant permissions
 * - REVOKE <user> <file> <perm> : Revoke permissions
 * - EXIT                  : Exit the client
 *
 * Registration Protocol:
 * Client -> NS: "REGISTER_CLIENT <username> <ip> <port>\n"
 * NS -> Client: "ACK\n" (on success)
 *
 * The client_main module orchestrates the overall client application,
 * managing the user session lifecycle from connection to disconnection.
 */

#ifndef CLIENT_MAIN_H
#define CLIENT_MAIN_H

/* Main entry point for the client application
 *
 * Responsibilities:
 * - Initialize client session
 * - Connect to Name Server
 * - Handle interactive command loop
 * - Manage graceful shutdown
 *
 * Returns:
 *   0 on successful exit
 *   1 on connection failure or critical error
 *
 * Note: This is the main() function, automatically called on program start
 */

#endif // CLIENT_MAIN_H
