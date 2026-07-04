#ifndef MESSAGE_H
#define MESSAGE_H

#include <stddef.h>
#include "protocol.h"
#include "error_codes.h"

// LLM GENERATED CODE STARTS HERE

#define MAX_MSG_SIZE 1024

typedef struct
{
    MessageType type;
    char payload[MAX_MSG_SIZE];
} Message;

/* Message creation and manipulation */
Message create_message(MessageType type, const char *payload);
MessageType get_message_type(const Message *msg);
const char *get_message_payload(const Message *msg);
char *format_message(const Message *msg, char *buffer, size_t buffer_size);

/* Message sending and receiving */
int send_message(int sockfd, const Message *msg);
int receive_message(int sockfd, Message *msg);

/* Text-based communication (legacy compatibility) */
int send_text(int sockfd, const char *text);
int receive_text(int sockfd, char *buffer, size_t buffer_size);

/* Convenience functions for common message types */
int send_ack(int sockfd, const char *ack_msg);
int send_error_message(int sockfd, ErrorCode error_code, const char *error_msg);

/* Message type checking */
int is_error_message(const Message *msg);
int is_ack_message(const Message *msg);

/* Error handling */
ErrorCode extract_error_code(const Message *msg);

#endif
// LLM GENERATED CODE ENDS HERE