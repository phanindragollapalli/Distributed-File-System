#include "../include/ns_cache.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LRU_CAPACITY 16

typedef struct LRUNode
{
    char *key;
    int value;
    struct LRUNode *prev, *next;
} LRUNode;

static LRUNode *lru_head = NULL, *lru_tail = NULL;
static int lru_count = 0;

void lru_put(const char *key, int value)
{
    // For demo: just insert at front, remove from tail if full.
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
