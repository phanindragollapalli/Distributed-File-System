
//   Implements message serialization and deserialization for communication
//   between Name Server, Storage Servers, and Clients.
//   Provides structured message passing with error handling for the distributed file system.

#include "../include/message.h"
#include "../include/error_codes.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>

// LLM GENERATED CODE STARTS HERE
/* Create a new message with given type and payload
 * Returns: Initialized Message structure
 */
Message create_message(MessageType type, const char *payload)
{
    Message msg;
    msg.type = type;

    if (payload)
    {
        strncpy(msg.payload, payload, MAX_MSG_SIZE - 1);
        msg.payload[MAX_MSG_SIZE - 1] = '\0';
    }
    else
    {
        msg.payload[0] = '\0';
    }

    return msg;
}

/* Send a message over a socket
 * Parameters:
 *   sockfd - Socket file descriptor
 *   msg - Pointer to Message structure to send
 * Returns: Number of bytes sent on success, -1 on failure
 */
int send_message(int sockfd, const Message *msg)
{
    if (!msg)
        return -1;

    ssize_t bytes_sent = send(sockfd, msg, sizeof(Message), 0);

    if (bytes_sent < 0)
        return -1;

    return (int)bytes_sent;
}

/* Receive a message from a socket
 * Parameters:
 *   sockfd - Socket file descriptor
 *   msg - Pointer to Message structure to fill
 * Returns: Number of bytes received on success, -1 on failure, 0 on connection close
 */
int receive_message(int sockfd, Message *msg)
{
    if (!msg)
        return -1;

    ssize_t bytes_received = recv(sockfd, msg, sizeof(Message), 0);

    if (bytes_received < 0)
        return -1;

    if (bytes_received == 0)
        return 0; // Connection closed

    return (int)bytes_received;
}

/* Send a simple text message (legacy compatibility)
 * Parameters:
 *   sockfd - Socket file descriptor
 *   text - Text string to send
 * Returns: Number of bytes sent on success, -1 on failure
 */
int send_text(int sockfd, const char *text)
{
    if (!text)
        return -1;

    size_t len = strlen(text);
    ssize_t bytes_sent = send(sockfd, text, len, 0);

    if (bytes_sent < 0)
        return -1;

    return (int)bytes_sent;
}

/* Receive text data from socket
 * Parameters:
 *   sockfd - Socket file descriptor
 *   buffer - Buffer to store received text
 *   buffer_size - Size of the buffer
 * Returns: Number of bytes received on success, -1 on failure, 0 on connection close
 */
int receive_text(int sockfd, char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0)
        return -1;

    ssize_t bytes_received = recv(sockfd, buffer, buffer_size - 1, 0);

    if (bytes_received < 0)
        return -1;

    if (bytes_received == 0)
        return 0; // Connection closed

    buffer[bytes_received] = '\0'; // Null-terminate
    return (int)bytes_received;
}

/* Create and send an ACK message
 * Parameters:
 *   sockfd - Socket file descriptor
 *   ack_msg - Optional acknowledgment message text
 * Returns: Number of bytes sent on success, -1 on failure
 */
int send_ack(int sockfd, const char *ack_msg)
{
    Message msg = create_message(MSG_ACK, ack_msg ? ack_msg : "ACK");
    return send_message(sockfd, &msg);
}

/* Create and send an ERROR message
 * Parameters:
 *   sockfd - Socket file descriptor
 *   error_code - Error code from ErrorCode enum
 *   error_msg - Optional error message text
 * Returns: Number of bytes sent on success, -1 on failure
 */
int send_error_message(int sockfd, ErrorCode error_code, const char *error_msg)
{
    char payload[MAX_MSG_SIZE];

    if (error_msg)
    {
        snprintf(payload, MAX_MSG_SIZE, "ERROR %d: %s - %s",
                 error_code, error_code_to_string(error_code), error_msg);
    }
    else
    {
        snprintf(payload, MAX_MSG_SIZE, "ERROR %d: %s",
                 error_code, error_code_to_string(error_code));
    }

    Message msg = create_message(MSG_ERROR, payload);
    return send_message(sockfd, &msg);
}

/* Parse message type from received message
 * Parameters:
 *   msg - Pointer to Message structure
 * Returns: MessageType enum value
 */
MessageType get_message_type(const Message *msg)
{
    if (!msg)
        return MSG_ERROR;

    return msg->type;
}

/* Get payload from message
 * Parameters:
 *   msg - Pointer to Message structure
 * Returns: Pointer to payload string (internal buffer, do not free)
 */
const char *get_message_payload(const Message *msg)
{
    if (!msg)
        return NULL;

    return msg->payload;
}

/* Check if message is an error message
 * Parameters:
 *   msg - Pointer to Message structure
 * Returns: 1 if error message, 0 otherwise
 */
int is_error_message(const Message *msg)
{
    if (!msg)
        return 0;

    return (msg->type == MSG_ERROR);
}

/* Check if message is an ACK message
 * Parameters:
 *   msg - Pointer to Message structure
 * Returns: 1 if ACK message, 0 otherwise
 */
int is_ack_message(const Message *msg)
{
    if (!msg)
        return 0;

    return (msg->type == MSG_ACK);
}

/* Extract error code from error message payload
 * Parameters:
 *   msg - Pointer to Message structure (must be MSG_ERROR type)
 * Returns: ErrorCode value, or ERR_GENERAL if parsing fails
 */
ErrorCode extract_error_code(const Message *msg)
{
    if (!msg || msg->type != MSG_ERROR)
        return ERR_GENERAL;

    int code;
    if (sscanf(msg->payload, "ERROR %d:", &code) == 1)
    {
        return (ErrorCode)code;
    }

    return ERR_GENERAL;
}

/* Format a message for logging or display
 * Parameters:
 *   msg - Pointer to Message structure
 *   buffer - Buffer to store formatted string
 *   buffer_size - Size of the buffer
 * Returns: Pointer to buffer on success, NULL on failure
 */
char *format_message(const Message *msg, char *buffer, size_t buffer_size)
{
    if (!msg || !buffer || buffer_size == 0)
        return NULL;

    const char *type_str;
    switch (msg->type)
    {
    case MSG_HELLO:
        type_str = "HELLO";
        break;
    case MSG_REGISTER_SS:
        type_str = "REGISTER_SS";
        break;
    case MSG_REGISTER_CLIENT:
        type_str = "REGISTER_CLIENT";
        break;
    case MSG_ACK:
        type_str = "ACK";
        break;
    case MSG_ERROR:
        type_str = "ERROR";
        break;
    default:
        type_str = "UNKNOWN";
        break;
    }

    snprintf(buffer, buffer_size, "[%s] %s", type_str, msg->payload);
    return buffer;
}

// LLM GENERATED CODE ENDS HERE