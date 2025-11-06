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

#endif
