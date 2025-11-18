#ifndef NS_STORAGE_H
#define NS_STORAGE_H

#define ALPHABET_SIZE 256 // ASCII

typedef struct TrieNode
{
    struct TrieNode *children[ALPHABET_SIZE];
    int is_end_of_file;
    char *file_name;
    int storage_server_id; // Index to SS tracking struct, -1 if not assigned
} TrieNode;

TrieNode *trie_create_node();
void trie_insert(TrieNode *root, const char *filename, int ss_id);
int trie_search(TrieNode *root, const char *filename, int *ss_id_out);
void trie_remove(TrieNode *root, const char *filename);

/* ========== DAY 13: PERSISTENCE FUNCTIONS ========== */

// Get all files from Trie
void trie_get_all_files(TrieNode *root, char files[][256], int ss_ids[], int *count, int max_count);

// Save Trie to disk
int save_trie_to_disk(TrieNode *root);

// Load Trie from disk
int load_trie_from_disk(TrieNode *root);

#endif
