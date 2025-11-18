#ifndef NS_CACHE_H
#define NS_CACHE_H

void lru_put(const char *key, int value);
int lru_get(const char *key, int *value_out);
void lru_cache_free();

#endif
