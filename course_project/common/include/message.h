#ifndef MESSAGE_H
#define MESSAGE_H

#include "protocol.h"

#define MAX_MSG_SIZE 1024

typedef struct
{
    MessageType type;
    char payload[MAX_MSG_SIZE];
} Message;

#endif
