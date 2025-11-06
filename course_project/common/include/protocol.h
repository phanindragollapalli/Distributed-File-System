#ifndef PROTOCOL_H
#define PROTOCOL_H

#define NS_PORT 9000
#define SS_PORT 9001
#define CLIENT_PORT 9002

// Define message types
typedef enum
{
    MSG_HELLO,
    MSG_REGISTER_SS,
    MSG_REGISTER_CLIENT,
    MSG_ACK,
    MSG_ERROR
} MessageType;

#endif
