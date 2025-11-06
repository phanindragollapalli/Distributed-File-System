#include "../include/ns_storage.h"
#include <stdlib.h>
#include <string.h>

TrieNode *trie_create_node()
{
    TrieNode *node = (TrieNode *)calloc(1, sizeof(TrieNode));
    node->is_end_of_file = 0;
    node->file_name = NULL;
    node->storage_server_id = -1;
    return node;
}

void trie_insert(TrieNode *root, const char *filename, int ss_id)
{
    TrieNode *p = root;
    for (int i = 0; filename[i]; ++i)
    {
        unsigned char idx = filename[i];
        if (!p->children[idx])
            p->children[idx] = trie_create_node();
        p = p->children[idx];
    }
    p->is_end_of_file = 1;
    p->file_name = strdup(filename);
    p->storage_server_id = ss_id;
}

int trie_search(TrieNode *root, const char *filename, int *ss_id_out)
{
    TrieNode *p = root;
    for (int i = 0; filename[i]; ++i)
    {
        unsigned char idx = filename[i];
        if (!p->children[idx])
            return 0;
        p = p->children[idx];
    }
    if (p && p->is_end_of_file)
    {
        if (ss_id_out)
            *ss_id_out = p->storage_server_id;
        return 1;
    }
    return 0;
}
