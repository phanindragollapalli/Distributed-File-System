/* FILE OPERATIONS - Person 2, Days 3-10
 * Implements CREATE, READ, WRITE, DELETE, and UNDO commands
 *
 * File operations flow:
 * - CREATE: Client → NS → SS (creates empty file)
 * - READ: Client → NS → Client → SS (direct connection for file content)
 * - WRITE: Client → NS → Client → SS (direct connection with sentence locking)
 * - DELETE: Client → NS → SS (removes file, clears ACL)
 * - UNDO: Client → NS → SS (reverts last change)
 */

#include "../include/client_file_ops.h"
#include "../include/client_network.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* CREATE COMMAND - Person 2, Days 3-4
 * Creates a new empty file on the Storage Server
 *
 * Flow:
 * 1. Client sends CREATE request to NS
 * 2. NS selects appropriate SS
 * 3. NS forwards CREATE command to SS
 * 4. SS creates empty file on disk
 * 5. SS sends ACK to NS
 * 6. NS updates metadata (owner, timestamps)
 * 7. NS creates ACL entry (owner has full access)
 * 8. NS sends ACK to client
 *
 * Parameters:
 *   filename - Name of file to create
 *   username - Creator's username (becomes owner)
 * Returns: 0 on success, -1 on failure
 */
int handle_create_command(const char *filename, const char *username)
{
    if (!filename || !username)
    {
        printf("Error: Invalid parameters\n");
        return -1;
    }

    printf("Creating file: %s\n", filename);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send CREATE request with username
    // Format: "CREATE <filename> <username>\n"
    char request[512];
    snprintf(request, sizeof(request), "CREATE %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send CREATE request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[1024];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    // Check response: "ACK <message>" or "ERROR <message>"
    if (strncmp(response, "ACK", 3) == 0)
    {
        printf("Success: File '%s' created successfully\n", filename);
        printf("You are the owner with full READ/WRITE access\n");
        close(ns_fd);
        return 0;
    }
    else if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("Error: %s\n", response + 7); // Skip "ERROR: "
        close(ns_fd);
        return -1;
    }
    else
    {
        printf("Error: Unexpected response from Name Server\n");
        close(ns_fd);
        return -1;
    }
}

/* READ COMMAND - Person 2, Days 3-4
 * Reads and displays complete file content
 *
 * Flow:
 * 1. Client sends READ request to NS
 * 2. NS checks ACL (user must have READ access)
 * 3. NS returns SS info (IP, port)
 * 4. Client connects to SS directly
 * 5. Client requests file content
 * 6. SS loads file, converts to linked list
 * 7. SS sends complete file content
 * 8. Client displays content
 *
 * Parameters:
 *   filename - Name of file to read
 *   username - Reader's username (for ACL check)
 * Returns: 0 on success, -1 on failure
 */
int handle_read_command(const char *filename, const char *username)
{
    if (!filename || !username)
    {
        printf("Error: Invalid parameters\n");
        return -1;
    }

    printf("Reading file: %s\n", filename);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send READ request with username for ACL check
    // Format: "READ <filename> <username>\n"
    char request[512];
    snprintf(request, sizeof(request), "READ %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send READ request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[1024];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    // Parse response: "SS_INFO <ip> <port>" or "ERROR <message>"
    if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("%s\n", response + 7);
        close(ns_fd);
        return -1;
    }

    char ss_ip[32];
    int ss_port;
    if (sscanf(response, "SS_INFO %31s %d", ss_ip, &ss_port) != 2)
    {
        printf("Error: Invalid response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    close(ns_fd);

    // Connect to Storage Server
    int ss_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_fd < 0)
    {
        perror("Error creating socket for Storage Server");
        return -1;
    }

    struct sockaddr_in ss_addr;
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);
    inet_pton(AF_INET, ss_ip, &ss_addr.sin_addr);

    if (connect(ss_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        perror("Error connecting to Storage Server");
        close(ss_fd);
        return -1;
    }

    // Request file content from Storage Server
    char ss_request[512];
    snprintf(ss_request, sizeof(ss_request), "READ %s\n", filename);
    send(ss_fd, ss_request, strlen(ss_request), 0);

    // Receive and display file content
    printf("\n--- File Content: %s ---\n", filename);

    char buffer[4096];
    int total_bytes = 0;
    while (1)
    {
        int n = recv(ss_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
            break;

        buffer[n] = '\0';

        // Check for error response
        if (total_bytes == 0 && strncmp(buffer, "ERROR", 5) == 0)
        {
            printf("%s\n", buffer + 7);
            close(ss_fd);
            return -1;
        }

        printf("%s", buffer);
        total_bytes += n;

        if (n < sizeof(buffer) - 1)
            break;
    }

    printf("\n--- End of File ---\n");
    printf("Total bytes read: %d\n", total_bytes);

    close(ss_fd);
    return 0;
}

/* DELETE COMMAND - Person 2, Day 10
 * Deletes a file from the Storage Server
 *
 * Flow:
 * 1. Client sends DELETE request to NS
 * 2. NS checks ACL (user must be owner or have WRITE access)
 * 3. NS forwards DELETE to SS
 * 4. SS removes file from disk
 * 5. SS clears file metadata
 * 6. SS sends ACK to NS
 * 7. NS removes file from Trie
 * 8. NS clears ACL entries
 * 9. NS removes metadata
 * 10. NS sends ACK to client
 *
 * Parameters:
 *   filename - Name of file to delete
 *   username - User requesting deletion (for ACL check)
 * Returns: 0 on success, -1 on failure
 */
int handle_delete_command(const char *filename, const char *username)
{
    if (!filename || !username)
    {
        printf("Error: Invalid parameters\n");
        return -1;
    }

    printf("Deleting file: %s\n", filename);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send DELETE request with username
    // Format: "DELETE <filename> <username>\n"
    char request[512];
    snprintf(request, sizeof(request), "DELETE %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send DELETE request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[1024];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    // Check response: "ACK <message>" or "ERROR <message>"
    if (strncmp(response, "ACK", 3) == 0)
    {
        printf("Success: File '%s' deleted successfully\n", filename);
        printf("All metadata and ACL entries have been removed\n");
        close(ns_fd);
        return 0;
    }
    else if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("Error: %s\n", response + 7);
        close(ns_fd);
        return -1;
    }
    else
    {
        printf("Error: Unexpected response from Name Server\n");
        close(ns_fd);
        return -1;
    }
}

/* WRITE COMMAND - Person 2, Days 5-7
 * Writes to a file with sentence-level locking
 *
 * Protocol:
 * WRITE <filename> <sentence_number>
 * <word_index> <content>
 * <word_index> <content>
 * ...
 * ETIRW
 *
 * Flow:
 * 1. Client sends WRITE request to NS
 * 2. NS checks ACL (user must have WRITE access)
 * 3. NS returns SS info
 * 4. Client connects to SS
 * 5. Client sends WRITE <filename> <sentence_number>
 * 6. SS locks the sentence (sentence-level read-write lock)
 * 7. Client sends word updates (<word_index> <content>)
 * 8. Client sends ETIRW to commit changes
 * 9. SS applies changes atomically (using swap file)
 * 10. SS releases sentence lock
 * 11. SS sends ACK
 *
 * Note: Content may contain sentence delimiters (. ! ?)
 * which create new sentences dynamically
 *
 * Parameters:
 *   filename - Name of file to write
 *   username - Writer's username (for ACL check)
 * Returns: 0 on success, -1 on failure
 */
int handle_write_command(const char *filename, const char *username)
{
    if (!filename || !username)
    {
        printf("Error: Invalid parameters\n");
        return -1;
    }

    printf("Write mode for file: %s\n", filename);
    printf("NOTE: WRITE command requires direct implementation\n");
    printf("Usage:\n");
    printf("  WRITE <filename> <sentence_number>\n");
    printf("  <word_index> <content>\n");
    printf("  <word_index> <content>\n");
    printf("  ...\n");
    printf("  ETIRW\n");

    // This is a complex interactive command that requires
    // direct SS connection and multi-step protocol
    // Placeholder for Person 2 to implement fully

    return 0;
}

/* UNDO COMMAND - Person 2, Days 8-9
 * Reverts last change made to a file
 *
 * Flow:
 * 1. Client sends UNDO request to NS
 * 2. NS checks ACL (any user with access can undo)
 * 3. NS forwards UNDO to SS
 * 4. SS retrieves last stored state from undo history
 * 5. SS restores file to previous state
 * 6. SS updates file on disk
 * 7. SS sends ACK to NS
 * 8. NS sends ACK to client
 *
 * Note: Undo is file-specific, not user-specific
 * Any user can undo changes made by any other user
 *
 * Parameters:
 *   filename - Name of file to undo
 *   username - User requesting undo (for ACL check)
 * Returns: 0 on success, -1 on failure
 */
int handle_undo_command(const char *filename, const char *username)
{
    if (!filename || !username)
    {
        printf("Error: Invalid parameters\n");
        return -1;
    }

    printf("Undoing last change to file: %s\n", filename);

    // Connect to Name Server
    int ns_fd = connect_to_ns("127.0.0.1", NS_PORT);
    if (ns_fd < 0)
    {
        printf("Error: Failed to connect to Name Server\n");
        return -1;
    }

    // Send UNDO request with username
    // Format: "UNDO <filename> <username>\n"
    char request[512];
    snprintf(request, sizeof(request), "UNDO %s %s\n", filename, username);

    if (send_to_ns(ns_fd, request) < 0)
    {
        printf("Error: Failed to send UNDO request\n");
        close(ns_fd);
        return -1;
    }

    // Receive response from NS
    char response[1024];
    int bytes_received = recv_from_ns(ns_fd, response, sizeof(response));

    if (bytes_received <= 0)
    {
        printf("Error: No response from Name Server\n");
        close(ns_fd);
        return -1;
    }

    // Check response: "ACK <message>" or "ERROR <message>"
    if (strncmp(response, "ACK", 3) == 0)
    {
        printf("Success: Last change to '%s' has been reverted\n", filename);
        close(ns_fd);
        return 0;
    }
    else if (strncmp(response, "ERROR", 5) == 0)
    {
        printf("Error: %s\n", response + 7);
        close(ns_fd);
        return -1;
    }
    else
    {
        printf("Error: Unexpected response from Name Server\n");
        close(ns_fd);
        return -1;
    }
}
