/* FILE STREAM - Person 2, Days 11-12 - NOT Person 1's Responsibility
 * Stream file contents word-by-word to client with 0.1 second delay
 * Client connects directly to Storage Server for streaming (not through NS)
 * Loads file, parses into words, sends one word at a time
 * Uses usleep(100000) for 0.1 second delay between words
 * Must handle graceful disconnection if Storage Server goes down mid-stream
 *
 * This module is implemented by Person 2 as part of their file operations work
 * Person 1 is NOT responsible for this implementation
 */
#include "../include/file_stream.h"
#include "../include/file_parser.h"
#include "../../common/include/error_codes.h"
#include "../../common/include/logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

extern Logger *ss_logger; // Declared in ss_main.c

// Extract all words from file into an array
int extract_all_words(FileStructure *file, char ***words_out, int *count_out)
{
    if (!file || !words_out || !count_out)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for extract_all_words");
        return ERR_NULL_POINTER;
    }

    // Count total words
    int total_words = count_total_words(file);
    if (total_words == 0)
    {
        *words_out = NULL;
        *count_out = 0;
        return SUCCESS;
    }

    // Allocate word array
    char **words = (char **)malloc(total_words * sizeof(char *));
    if (!words)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to allocate words array");
        return ERR_MEMORY;
    }

    int word_idx = 0;
    SentenceNode *sentence = file->sentences;

    while (sentence)
    {
        WordNode *word = sentence->words;
        while (word)
        {
            words[word_idx] = strdup(word->word);
            if (!words[word_idx])
            {
                // Cleanup on error
                for (int i = 0; i < word_idx; i++)
                {
                    free(words[i]);
                }
                free(words);
                return ERR_MEMORY;
            }
            word_idx++;
            word = word->next;
        }
        sentence = sentence->next;
    }

    *words_out = words;
    *count_out = total_words;

    log_message(ss_logger, LOG_DEBUG, "Extracted %d words from file", total_words);
    return SUCCESS;
}

// Create stream context
StreamContext *create_stream_context(FileStructure *file, int delay_ms)
{
    if (!file)
    {
        log_message(ss_logger, LOG_ERROR, "Null file provided to create_stream_context");
        return NULL;
    }

    StreamContext *ctx = (StreamContext *)malloc(sizeof(StreamContext));
    if (!ctx)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to allocate stream context");
        return NULL;
    }

    ctx->file = file;
    ctx->words = NULL;
    ctx->total_words = 0;
    ctx->current_word_index = 0;
    ctx->delay_ms = delay_ms > 0 ? delay_ms : STREAM_DELAY_MS;
    ctx->is_active = 0;

    if (pthread_mutex_init(&ctx->stream_lock, NULL) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize stream mutex");
        free(ctx);
        return NULL;
    }

    // Extract all words
    int result = extract_all_words(file, &ctx->words, &ctx->total_words);
    if (result != SUCCESS)
    {
        pthread_mutex_destroy(&ctx->stream_lock);
        free(ctx);
        return NULL;
    }

    log_message(ss_logger, LOG_INFO, "Created stream context with %d words (delay: %dms)",
                ctx->total_words, ctx->delay_ms);

    return ctx;
}

// Destroy stream context
void destroy_stream_context(StreamContext *ctx)
{
    if (!ctx)
        return;

    pthread_mutex_lock(&ctx->stream_lock);

    // Free words array
    if (ctx->words)
    {
        for (int i = 0; i < ctx->total_words; i++)
        {
            if (ctx->words[i])
            {
                free(ctx->words[i]);
            }
        }
        free(ctx->words);
    }

    pthread_mutex_unlock(&ctx->stream_lock);
    pthread_mutex_destroy(&ctx->stream_lock);

    log_message(ss_logger, LOG_DEBUG, "Destroyed stream context");
    free(ctx->file);
    free(ctx);
}

// Start streaming
int start_stream(StreamContext *ctx)
{
    if (!ctx)
    {
        return ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&ctx->stream_lock);

    ctx->current_word_index = 0;
    ctx->is_active = 1;

    pthread_mutex_unlock(&ctx->stream_lock);

    log_message(ss_logger, LOG_INFO, "Started streaming (%d words)", ctx->total_words);
    return SUCCESS;
}

// Get next word
char *get_next_word(StreamContext *ctx)
{
    if (!ctx)
    {
        return NULL;
    }

    pthread_mutex_lock(&ctx->stream_lock);

    // Check if streaming was interrupted (e.g., external stop signal)
    if (!ctx->is_active || ctx->current_word_index >= ctx->total_words)
    {
        pthread_mutex_unlock(&ctx->stream_lock);
        return NULL;
    }

    char *word = strdup(ctx->words[ctx->current_word_index]);
    ctx->current_word_index++;

    pthread_mutex_unlock(&ctx->stream_lock);

    // Simulate streaming delay
    usleep(ctx->delay_ms * 1000);

    return word;
}

// Check if more words available
int has_more_words(StreamContext *ctx)
{
    if (!ctx)
        return 0;

    pthread_mutex_lock(&ctx->stream_lock);
    int has_more = ctx->is_active && (ctx->current_word_index < ctx->total_words);
    pthread_mutex_unlock(&ctx->stream_lock);

    return has_more;
}

// Stop streaming
void stop_stream(StreamContext *ctx)
{
    if (!ctx)
        return;

    pthread_mutex_lock(&ctx->stream_lock);
    ctx->is_active = 0;
    pthread_mutex_unlock(&ctx->stream_lock);

    log_message(ss_logger, LOG_INFO, "Stopped streaming at word %d/%d",
                ctx->current_word_index, ctx->total_words);
}

// Stream entire file with custom output function
int stream_file_to_output(FileStructure *file, void (*output_fn)(const char *word), int delay_ms)
{
    if (!file || !output_fn)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for stream_file_to_output");
        return ERR_NULL_POINTER;
    }

    StreamContext *ctx = create_stream_context(file, delay_ms);
    if (!ctx)
    {
        return ERR_MEMORY;
    }

    start_stream(ctx);

    int words_streamed = 0;
    while (has_more_words(ctx))
    {
        char *word = get_next_word(ctx);
        if (word)
        {
            output_fn(word);
            free(word);
            words_streamed++;
        }
    }

    stop_stream(ctx);
    destroy_stream_context(ctx);

    log_message(ss_logger, LOG_INFO, "Streamed %d words", words_streamed);
    return SUCCESS;
}