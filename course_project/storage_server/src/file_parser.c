/* FILE PARSER
 * Parse file contents into words and sentences for linked list structure
 * Words are separated by spaces
 * Sentences are delimited by punctuation: period (.), exclamation mark (!), question mark (?)
 * Converts raw text file into structured format for concurrent access
 */
#include "../include/file_parser.h"
#include "../../common/include/error_codes.h"
#include "../../common/include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern Logger *ss_logger; // Declared in ss_main.c

// Check if character is a sentence delimiter
int is_sentence_delimiter(char c)
{
    return (c == '.' || c == '!' || c == '?');
}

// Extract next word from string (currently unused)
char *extract_next_word(const char *str, int *pos)
{
    if (!str || !pos)
        return NULL;

    // Skip leading spaces
    while (str[*pos] && isspace(str[*pos]))
    {
        (*pos)++;
    }

    if (str[*pos] == '\0')
    {
        return NULL;
    }

    int start = *pos;

    // Find end of word (space or end of string)
    while (str[*pos] && !isspace(str[*pos]))
    {
        (*pos)++;
    }

    int length = *pos - start;
    char *word = (char *)malloc(length + 1);
    if (!word)
    {
        log_message(ss_logger, LOG_ERROR, "Memory allocation failed for word");
        return NULL;
    }

    strncpy(word, str + start, length);
    word[length] = '\0';

    return word;
}

// Parse line into sentences and words
int parse_line(FileStructure *file, const char *line)
{
    if (!file || !line)
    {
        return ERR_NULL_POINTER;
    }

    int pos = 0;
    SentenceNode *current_sentence = create_sentence_node(file->sentence_count);

    if (!current_sentence)
    {
        return ERR_MEMORY;
    }

    char word_buffer[MAX_WORD_LEN];
    int word_idx = 0;

    while (line[pos] != '\0')
    {
        char c = line[pos];

        // Check if current character is a sentence delimiter
        if (is_sentence_delimiter(c))
        {
            // Add the word before delimiter (if any)
            if (word_idx > 0)
            {
                word_buffer[word_idx] = '\0';
                if (add_word_to_sentence(current_sentence, word_buffer) != SUCCESS)
                {
                    log_message(ss_logger, LOG_WARN, "Failed to add word to sentence");
                }
                word_idx = 0;
            }

            // Close current sentence with this delimiter
            current_sentence->delimiter = c;
            if (add_sentence(file, current_sentence) != SUCCESS)
            {
                destroy_sentence_node(current_sentence);
                return ERR_MEMORY;
            }

            // Start new sentence
            current_sentence = create_sentence_node(file->sentence_count);
            if (!current_sentence)
            {
                return ERR_MEMORY;
            }
        }
        // Check if current character is whitespace
        else if (isspace(c))
        {
            // End of word - add to sentence if buffer not empty
            if (word_idx > 0)
            {
                word_buffer[word_idx] = '\0';
                if (add_word_to_sentence(current_sentence, word_buffer) != SUCCESS)
                {
                    log_message(ss_logger, LOG_WARN, "Failed to add word to sentence");
                }
                word_idx = 0;
            }
        }
        // Regular character - add to word buffer
        else
        {
            if (word_idx < MAX_WORD_LEN - 1)
            {
                word_buffer[word_idx++] = c;
            }
            else
            {
                // Word too long - log warning and skip character
                log_message(ss_logger, LOG_WARN, "Word exceeds MAX_WORD_LEN, truncating");
            }
        }

        pos++;
    }

    // Add last word if exists
    if (word_idx > 0)
    {
        word_buffer[word_idx] = '\0';
        if (add_word_to_sentence(current_sentence, word_buffer) != SUCCESS)
        {
            log_message(ss_logger, LOG_WARN, "Failed to add word to sentence");
        }
    }

    // Add last sentence if it has words (incomplete sentence without delimiter)
    if (current_sentence->word_count > 0)
    {
        // No delimiter at end - use default period
        current_sentence->delimiter = '.';
        if (add_sentence(file, current_sentence) != SUCCESS)
        {
            destroy_sentence_node(current_sentence);
            return ERR_MEMORY;
        }
    }
    else
    {
        // Empty sentence - just destroy it
        destroy_sentence_node(current_sentence);
    }

    return SUCCESS;
}

// Parse file content
int parse_file_content(FileStructure *file, const char *content)
{
    if (!file || !content)
    {
        return ERR_NULL_POINTER;
    }

    log_message(ss_logger, LOG_INFO, "Parsing file content for: %s", file->filename);

    char *content_copy = strdup(content);
    if (!content_copy)
    {
        return ERR_MEMORY;
    }

    char *save_ptr = NULL;
    char *line = strtok_r(content_copy, "\n", &save_ptr);
    while (line)
    {
        int result = parse_line(file, line);
        if (result != SUCCESS)
        {
            free(content_copy);
            return result;
        }
        line = strtok_r(NULL, "\n", &save_ptr);
    }

    free(content_copy);
    file->is_loaded = 1;

    log_message(ss_logger, LOG_INFO, "Parsed %d sentences for file: %s",
                file->sentence_count, file->filename);
    return SUCCESS;
}

// Convert sentence to string
char *sentence_to_string(SentenceNode *sentence)
{
    if (!sentence)
        return NULL;

    char buffer[MAX_SENTENCE_LEN];
    int offset = 0;

    WordNode *current = sentence->words;
    while (current)
    {
        int len = strlen(current->word);
        if (offset + len + 2 < MAX_SENTENCE_LEN)
        {
            strcpy(buffer + offset, current->word);
            offset += len;

            if (current->next)
            {
                buffer[offset++] = ' ';
            }
        }
        else
        {
            log_message(ss_logger, LOG_WARN, "Sentence exceeds MAX_SENTENCE_LEN, truncating");
            break;
        }
        current = current->next;
    }

    // Add delimiter
    if (offset < MAX_SENTENCE_LEN - 1)
    {
        buffer[offset++] = sentence->delimiter;
    }

    buffer[offset] = '\0';

    return strdup(buffer);
}

// Convert file structure to string
char *file_to_string(FileStructure *file)
{
    if (!file)
        return NULL;

    size_t total_size = 0;
    SentenceNode *current = file->sentences;

    // Calculate total size needed
    while (current)
    {
        total_size += MAX_SENTENCE_LEN; // Conservative estimate
        current = current->next;
    }

    char *result = (char *)malloc(total_size + 1);
    if (!result)
    {
        log_message(ss_logger, LOG_ERROR, "Memory allocation failed for file string");
        return NULL;
    }

    result[0] = '\0';
    int offset = 0;

    current = file->sentences;
    while (current)
    {
        char *sentence_str = sentence_to_string(current);
        if (sentence_str)
        {
            int len = strlen(sentence_str);
            if (offset + len + 2 < total_size)
            {
                strcpy(result + offset, sentence_str);
                offset += len;

                if (current->next)
                {
                    result[offset++] = ' ';
                }
            }
            free(sentence_str);
        }
        current = current->next;
    }

    result[offset] = '\0';
    return result;
}

// Load file from disk
int load_file_from_disk(FileStructure *file, const char *filepath)
{
    if (!file || !filepath)
    {
        return ERR_NULL_POINTER;
    }

    FILE *fp = fopen(filepath, "r");
    if (!fp)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to open file: %s", filepath);
        return ERR_FILE_OPEN;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(fp);
        file->is_loaded = 1;
        log_message(ss_logger, LOG_INFO, "Empty file loaded: %s", filepath);
        return SUCCESS;
    }

    // Read file content
    char *content = (char *)malloc(file_size + 1);
    if (!content)
    {
        fclose(fp);
        return ERR_MEMORY;
    }

    size_t bytes_read = fread(content, 1, file_size, fp);
    content[bytes_read] = '\0';
    fclose(fp);

    // Parse content
    int result = parse_file_content(file, content);
    free(content);

    if (result == SUCCESS)
    {
        log_message(ss_logger, LOG_INFO, "Loaded file from disk: %s", filepath);
    }

    return result;
}

// Save file to disk
int save_file_to_disk(FileStructure *file, const char *filepath)
{
    if (!file || !filepath)
    {
        return ERR_NULL_POINTER;
    }

    char *content = file_to_string(file);
    if (!content)
    {
        return ERR_MEMORY;
    }

    FILE *fp = fopen(filepath, "w");
    if (!fp)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to open file for writing: %s", filepath);
        free(content);
        return ERR_FILE_WRITE;
    }

    fprintf(fp, "%s", content);
    fclose(fp);
    free(content);

    log_message(ss_logger, LOG_INFO, "Saved file to disk: %s", filepath);
    return SUCCESS;
}

// Count total words
int count_total_words(FileStructure *file)
{
    if (!file)
        return 0;

    int total = 0;
    SentenceNode *current = file->sentences;

    while (current)
    {
        total += current->word_count;
        current = current->next;
    }

    return total;
}

// Count total characters
int count_total_chars(FileStructure *file)
{
    if (!file)
        return 0;

    int total = 0;
    SentenceNode *current = file->sentences;

    while (current)
    {
        WordNode *word = current->words;
        while (word)
        {
            total += strlen(word->word);
            total++; // Space or delimiter
            word = word->next;
        }
        current = current->next;
    }

    return total;
}