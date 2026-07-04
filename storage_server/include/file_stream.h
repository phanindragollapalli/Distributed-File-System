#ifndef FILE_STREAM_H
#define FILE_STREAM_H

#include "file_structure.h"
#include "../../common/include/constants.h"

// Stream context
typedef struct StreamContext
{
    FileStructure *file;
    char **words;           // Array of all words in file
    int total_words;        // Total word count
    int current_word_index; // Current position in stream
    int delay_ms;           // Delay between words in milliseconds
    int is_active;          // Stream is active
    pthread_mutex_t stream_lock;
} StreamContext;

// Create stream context
StreamContext *create_stream_context(FileStructure *file, int delay_ms);
void destroy_stream_context(StreamContext *ctx);

// Stream operations
int start_stream(StreamContext *ctx);
char *get_next_word(StreamContext *ctx);
int has_more_words(StreamContext *ctx);
void stop_stream(StreamContext *ctx);

// Full file streaming (for testing without network)
int stream_file_to_output(FileStructure *file, void (*output_fn)(const char *word), int delay_ms);

// Helper: Extract all words from file
int extract_all_words(FileStructure *file, char ***words_out, int *count_out);

#endif // FILE_STREAM_H