#ifndef FILE_PARSER_H
#define FILE_PARSER_H

#include "file_structure.h"

// Parse file content into linked list structure
int parse_file_content(FileStructure *file, const char *content);

// Parse a single line into sentences and words
int parse_line(FileStructure *file, const char *line);

// Check if character is a sentence delimiter
int is_sentence_delimiter(char c);

// Extract next word from string
char *extract_next_word(const char *str, int *pos);

// Convert file structure back to string
char *file_to_string(FileStructure *file);

// Convert sentence to string
char *sentence_to_string(SentenceNode *sentence);

// Load file from disk into file structure
int load_file_from_disk(FileStructure *file, const char *filepath);

// Save file structure to disk
int save_file_to_disk(FileStructure *file, const char *filepath);

// Count words in file
int count_total_words(FileStructure *file);

// Count characters in file
int count_total_chars(FileStructure *file);

#endif // FILE_PARSER_H