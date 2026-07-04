#ifndef NS_STORAGE_H
#define NS_STORAGE_H

#define ALPHABET_SIZE 256 // ASCII
#define MAX_SS_PER_FILE 16 // Maximum number of storage servers that can store a file

typedef struct TrieNode
{
    struct TrieNode *children[ALPHABET_SIZE];
    int is_end_of_file;
    char *file_name;
    int storage_server_ids[MAX_SS_PER_FILE]; // List of SS IDs that store this file
    int ss_count; // Number of storage servers storing this file
} TrieNode;

TrieNode *trie_create_node();
void trie_insert(TrieNode *root, const char *filename, int ss_id);
int trie_search(TrieNode *root, const char *filename, int *ss_id_out);
int trie_search_all(TrieNode *root, const char *filename, int *ss_ids_out, int max_ids);
void trie_remove(TrieNode *root, const char *filename);
void trie_remove_ss(TrieNode *root, const char *filename, int ss_id);

// persistence functions

// Get all files from Trie
void trie_get_all_files(TrieNode *root, char files[][256], int ss_ids[], int *count, int max_count);

// Save Trie to disk
int save_trie_to_disk(TrieNode *root);

// Load Trie from disk
int load_trie_from_disk(TrieNode *root);

#endif
