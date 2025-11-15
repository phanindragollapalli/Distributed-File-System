/* FILE STRUCTURE
 * Linked list representation of files with sentence nodes
 * Each sentence can be locked independently for concurrent access
 * Words are separated by spaces, sentences by punctuation (. ! ?)
 *
 */
#include "../include/file_structure.h"
#include "../../common/include/error_codes.h"
#include "../../common/include/logger.h"
#include <stdlib.h>
#include <string.h>

extern Logger *ss_logger; // Declared in ss_main.c

// Create file structure
FileStructure *create_file_structure(const char *filename)
{
    if (!filename)
    {
        log_message(ss_logger, LOG_ERROR, "Null filename provided");
        return NULL;
    }

    FileStructure *file = (FileStructure *)malloc(sizeof(FileStructure));
    if (!file)
    {
        log_message(ss_logger, LOG_ERROR, "Memory allocation failed for file structure");
        return NULL;
    }

    strncpy(file->filename, filename, MAX_FILENAME_LEN - 1);
    file->filename[MAX_FILENAME_LEN - 1] = '\0';
    file->sentences = NULL;
    file->sentence_count = 0;
    file->is_loaded = 0;

    if (pthread_mutex_init(&file->file_lock, NULL) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize file mutex");
        free(file);
        return NULL;
    }

    log_message(ss_logger, LOG_INFO, "Created file structure for: %s", filename);
    return file;
}

// Destroy file structure
void destroy_file_structure(FileStructure *file)
{
    if (!file)
        return;

    pthread_mutex_lock(&file->file_lock);

    SentenceNode *current = file->sentences;
    while (current)
    {
        SentenceNode *next = current->next;
        destroy_sentence_node(current);
        current = next;
    }

    pthread_mutex_unlock(&file->file_lock);
    pthread_mutex_destroy(&file->file_lock);

    log_message(ss_logger, LOG_INFO, "Destroyed file structure: %s", file->filename);
    free(file);
}

// Create sentence node
SentenceNode *create_sentence_node(int sentence_id)
{
    SentenceNode *sentence = (SentenceNode *)malloc(sizeof(SentenceNode));
    if (!sentence)
    {
        log_message(ss_logger, LOG_ERROR, "Memory allocation failed for sentence");
        return NULL;
    }

    sentence->words = NULL;
    sentence->word_count = 0;
    sentence->delimiter = '.'; // Default delimiter
    sentence->next = NULL;
    sentence->sentence_id = sentence_id;

    if (pthread_rwlock_init(&sentence->lock, NULL) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize sentence rwlock");
        free(sentence);
        return NULL;
    }

    return sentence;
}

// Destroy sentence node
void destroy_sentence_node(SentenceNode *sentence)
{
    if (!sentence)
        return;

    WordNode *current = sentence->words;
    while (current)
    {
        WordNode *next = current->next;
        destroy_word_node(current);
        current = next;
    }

    pthread_rwlock_destroy(&sentence->lock);
    free(sentence);
}

// Create word node
WordNode *create_word_node(const char *word)
{
    if (!word)
    {
        log_message(ss_logger, LOG_ERROR, "Null word provided");
        return NULL;
    }

    WordNode *word_node = (WordNode *)malloc(sizeof(WordNode));
    if (!word_node)
    {
        log_message(ss_logger, LOG_ERROR, "Memory allocation failed for word node");
        return NULL;
    }

    word_node->word = strdup(word);
    if (!word_node->word)
    {
        log_message(ss_logger, LOG_ERROR, "Memory allocation failed for word string");
        free(word_node);
        return NULL;
    }

    word_node->next = NULL;
    return word_node;
}

// Destroy word node
void destroy_word_node(WordNode *word_node)
{
    if (!word_node)
        return;

    if (word_node->word)
    {
        free(word_node->word);
    }
    free(word_node);
}

// Add sentence to file
int add_sentence(FileStructure *file, SentenceNode *sentence)
{
    if (!file || !sentence)
    {
        return ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&file->file_lock);

    if (!file->sentences)
    {
        file->sentences = sentence;
    }
    else
    {
        SentenceNode *current = file->sentences;
        while (current->next)
        {
            current = current->next;
        }
        current->next = sentence;
    }

    sentence->sentence_id = file->sentence_count;
    file->sentence_count++;

    pthread_mutex_unlock(&file->file_lock);

    log_message(ss_logger, LOG_DEBUG, "Added sentence %d to file %s",
                sentence->sentence_id, file->filename);
    return SUCCESS;
}

// Get sentence by index
SentenceNode *get_sentence(FileStructure *file, int sentence_index)
{
    if (!file)
    {
        return NULL;
    }

    if (sentence_index < 0 || sentence_index >= file->sentence_count)
    {
        log_message(ss_logger, LOG_WARN, "Sentence index %d out of range", sentence_index);
        return NULL;
    }

    pthread_mutex_lock(&file->file_lock);

    SentenceNode *current = file->sentences;
    int index = 0;

    while (current && index < sentence_index)
    {
        current = current->next;
        index++;
    }

    pthread_mutex_unlock(&file->file_lock);

    return current;
}

// Get sentence count
int get_sentence_count(FileStructure *file)
{
    if (!file)
        return 0;
    return file->sentence_count;
}

// Add word to sentence (at end)
int add_word_to_sentence(SentenceNode *sentence, const char *word)
{
    if (!sentence || !word)
    {
        return ERR_NULL_POINTER;
    }

    WordNode *word_node = create_word_node(word);
    if (!word_node)
    {
        return ERR_MEMORY;
    }

    if (!sentence->words)
    {
        sentence->words = word_node;
    }
    else
    {
        WordNode *current = sentence->words;
        while (current->next)
        {
            current = current->next;
        }
        current->next = word_node;
    }

    sentence->word_count++;
    return SUCCESS;
}

// Insert word at specific index
int insert_word_at_index(SentenceNode *sentence, int index, const char *word)
{
    if (!sentence || !word)
    {
        return ERR_NULL_POINTER;
    }

    if (index < 0 || index > sentence->word_count)
    {
        log_message(ss_logger, LOG_WARN, "Word index %d out of range", index);
        return ERR_WORD_OUT_OF_RANGE;
    }

    WordNode *word_node = create_word_node(word);
    if (!word_node)
    {
        return ERR_MEMORY;
    }

    // Insert at beginning
    if (index == 0)
    {
        word_node->next = sentence->words;
        sentence->words = word_node;
    }
    else
    {
        WordNode *current = sentence->words;
        for (int i = 0; i < index - 1 && current; i++)
        {
            current = current->next;
        }

        if (current)
        {
            word_node->next = current->next;
            current->next = word_node;
        }
    }

    sentence->word_count++;
    log_message(ss_logger, LOG_DEBUG, "Inserted word at index %d", index);
    return SUCCESS;
}

// Get word count
int get_word_count(SentenceNode *sentence)
{
    if (!sentence)
        return 0;
    return sentence->word_count;
}

// Get word at index
char *get_word_at_index(SentenceNode *sentence, int index)
{
    if (!sentence || index < 0 || index >= sentence->word_count)
    {
        return NULL;
    }

    WordNode *current = sentence->words;
    for (int i = 0; i < index && current; i++)
    {
        current = current->next;
    }

    return current ? current->word : NULL;
}

// Locking operations
int acquire_sentence_read_lock(SentenceNode *sentence)
{
    if (!sentence)
        return ERR_NULL_POINTER;

    int result = pthread_rwlock_rdlock(&sentence->lock);
    if (result == 0)
    {
        log_message(ss_logger, LOG_DEBUG, "Acquired read lock on sentence %d",
                    sentence->sentence_id);
        return SUCCESS;
    }

    log_message(ss_logger, LOG_ERROR, "Failed to acquire read lock");
    return ERR_FILE_LOCKED;
}

int release_sentence_read_lock(SentenceNode *sentence)
{
    if (!sentence)
        return ERR_NULL_POINTER;

    int result = pthread_rwlock_unlock(&sentence->lock);
    if (result == 0)
    {
        log_message(ss_logger, LOG_DEBUG, "Released read lock on sentence %d",
                    sentence->sentence_id);
        return SUCCESS;
    }

    log_message(ss_logger, LOG_ERROR, "Failed to release read lock");
    return ERR_GENERAL;
}

int acquire_sentence_write_lock(SentenceNode *sentence)
{
    if (!sentence)
        return ERR_NULL_POINTER;

    int result = pthread_rwlock_wrlock(&sentence->lock);
    if (result == 0)
    {
        log_message(ss_logger, LOG_DEBUG, "Acquired write lock on sentence %d",
                    sentence->sentence_id);
        return SUCCESS;
    }

    log_message(ss_logger, LOG_ERROR, "Failed to acquire write lock");
    return ERR_FILE_LOCKED;
}

int release_sentence_write_lock(SentenceNode *sentence)
{
    if (!sentence)
        return ERR_NULL_POINTER;

    int result = pthread_rwlock_unlock(&sentence->lock);
    if (result == 0)
    {
        log_message(ss_logger, LOG_DEBUG, "Released write lock on sentence %d",
                    sentence->sentence_id);
        return SUCCESS;
    }

    log_message(ss_logger, LOG_ERROR, "Failed to release write lock");
    return ERR_GENERAL;
}