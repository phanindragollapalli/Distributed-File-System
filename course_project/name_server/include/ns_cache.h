/*
 * LRU CACHE HEADER
 * Person 1, Days 3-4
 *
 * Defines LRU cache for frequently accessed file paths.
 * Capacity: 16 entries, O(1) access time.
 */

#ifndef NS_CACHE_H
#define NS_CACHE_H

void lru_put(const char *key, int value);
int lru_get(const char *key, int *value_out);
void lru_cache_free();

#endif
