/*LRU CACHE
 * Least Recently Used cache for recent file lookups to optimize Name Server performance
 * Capacity: 16 entries (configurable via LRU_CAPACITY)
 * Implementation: Doubly linked list for O(1) insertion/removal at both ends
 * Cache stores filename to SS_ID mappings to avoid repeated Trie traversals
 * Most recently accessed files move to front, least recently used evicted from tail
 */

#include "../include/ns_cache.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LRU_CAPACITY 16

/* LRU node structure: stores key (filename) and value (SS_ID) with prev/next pointers */
typedef struct LRUNode
{
    char *key;
    int value;
    struct LRUNode *prev, *next;
} LRUNode;

static LRUNode *lru_head = NULL, *lru_tail = NULL;
static int lru_count = 0;

/* Insert or update a cache entry
 * New entries added to front (most recently used position)
 * If cache exceeds capacity, removes least recently used entry from tail
 */
void lru_put(const char *key, int value)
{
    /* Create new node and insert at head (most recently used position) */
    LRUNode *node = (LRUNode *)calloc(1, sizeof(LRUNode));
    node->key = strdup(key);
    node->value = value;
    node->next = lru_head;
    if (lru_head)
        lru_head->prev = node;
    lru_head = node;
    if (!lru_tail)
        lru_tail = node;
    ++lru_count;

    /* Evict least recently used entry if cache is full */
    if (lru_count > LRU_CAPACITY)
    {
        LRUNode *tmp = lru_tail;
        lru_tail = lru_tail->prev;
        lru_tail->next = NULL;
        free(tmp->key);
        free(tmp);
        --lru_count;
    }
}

int lru_get(const char *key, int *value_out)
{
    LRUNode *curr = lru_head;
    while (curr)
    {
        if (strcmp(curr->key, key) == 0)
        {
            if (value_out)
                *value_out = curr->value;
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

void lru_cache_free()
{
    LRUNode *curr = lru_head;
    while (curr)
    {
        LRUNode *next = curr->next;
        free(curr->key);
        free(curr);
        curr = next;
    }
    lru_head = lru_tail = NULL;
}
