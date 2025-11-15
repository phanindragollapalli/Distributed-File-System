#ifndef FILE_STRUCTURE_H
#define FILE_STRUCTURE_H

#include <pthread.h>
#include <time.h>
#include "../../common/include/constants.h"

// Word node in a sentence
typedef struct WordNode
{
    char *word;            // Dynamically allocated word
    struct WordNode *next; // Next word in sentence
} WordNode;

// Sentence node with locking
typedef struct SentenceNode
{
    WordNode *words;           // Linked list of words
    int word_count;            // Number of words in sentence
    char delimiter;            // Sentence delimiter: '.', '!', '?'
    pthread_rwlock_t lock;     // Read-write lock for this sentence
    struct SentenceNode *next; // Next sentence
    int sentence_id;           // Sentence index
} SentenceNode;

// File structure in memory
typedef struct FileStructure
{
    char filename[MAX_FILENAME_LEN];
    SentenceNode *sentences;   // Linked list of sentences
    int sentence_count;        // Total number of sentences
    pthread_mutex_t file_lock; // Lock for modifying sentence list
    int is_loaded;             // Flag: file loaded in memory
} FileStructure;

// Function prototypes
FileStructure *create_file_structure(const char *filename);
void destroy_file_structure(FileStructure *file);

SentenceNode *create_sentence_node(int sentence_id);
void destroy_sentence_node(SentenceNode *sentence);

WordNode *create_word_node(const char *word);
void destroy_word_node(WordNode *word_node);

// Sentence operations
int add_sentence(FileStructure *file, SentenceNode *sentence);
SentenceNode *get_sentence(FileStructure *file, int sentence_index);
// int remove_sentence(FileStructure* file, int sentence_index);
int get_sentence_count(FileStructure *file);

// Word operations
int add_word_to_sentence(SentenceNode *sentence, const char *word);
int insert_word_at_index(SentenceNode *sentence, int index, const char *word);
// int remove_word_from_sentence(SentenceNode* sentence, int index);
int get_word_count(SentenceNode *sentence);
char *get_word_at_index(SentenceNode *sentence, int index);

// Locking operations
int acquire_sentence_read_lock(SentenceNode *sentence);
int release_sentence_read_lock(SentenceNode *sentence);
int acquire_sentence_write_lock(SentenceNode *sentence);
int release_sentence_write_lock(SentenceNode *sentence);

#endif // FILE_STRUCTURE_H