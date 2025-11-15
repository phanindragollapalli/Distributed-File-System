#ifndef CONSTANTS_H
#define CONSTANTS_H

// Port configurations
#define NS_PORT 8080
#define SS_BASE_PORT 9000
#define CLIENT_BASE_PORT 10000

// Network constants
#define MAX_BUFFER_SIZE 8192
#define MAX_CLIENTS 100
#define MAX_STORAGE_SERVERS 50
#define MAX_FILENAME_LEN 256
#define MAX_USERNAME_LEN 64
#define MAX_PATH_LEN 512

// Timing constants
#define STREAM_DELAY_MS 100
#define CONNECTION_TIMEOUT 30

// File operation constants
#define MAX_SENTENCE_LEN 4096
#define MAX_WORD_LEN 256
#define MAX_UNDO_HISTORY 1

#endif // CONSTANTS_H