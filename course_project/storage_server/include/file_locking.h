#ifndef FILE_LOCKING_H
#define FILE_LOCKING_H

#include "file_structure.h"

// Lock manager for tracking active locks
typedef struct LockInfo
{
    int sentence_id;
    char username[MAX_USERNAME_LEN];
    int is_write_lock;
    time_t lock_time;
    struct LockInfo *next;
} LockInfo;

typedef struct LockManager
{
    LockInfo *locks;
    pthread_mutex_t manager_lock;
} LockManager;

// Initialize lock manager
LockManager *create_lock_manager();
void destroy_lock_manager(LockManager *manager);

// Track locks
int add_lock_info(LockManager *manager, int sentence_id, const char *username, int is_write);
int remove_lock_info(LockManager *manager, int sentence_id, const char *username);
int is_sentence_locked_by_others(LockManager *manager, int sentence_id, const char *username);

// Get lock information
LockInfo *get_lock_info(LockManager *manager, int sentence_id);

#endif // FILE_LOCKING_H