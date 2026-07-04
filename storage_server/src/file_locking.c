/* FILE LOCKING
 * Sentence-level read-write locks for concurrent file access
 * Uses pthread read-write locks (pthread_rwlock_t) for each sentence
 * Multiple readers can access same sentence simultaneously
 * Only one writer can access a sentence at a time
 * Writers block all readers and other writers on that sentence
 * Enables concurrent writes to different sentences in same file
 */
#include "../include/file_locking.h"
#include "../../common/include/error_codes.h"
#include "../../common/include/logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern Logger *ss_logger; // Declared in ss_main.c
// Create lock manager
LockManager *create_lock_manager()
{
    LockManager *manager = (LockManager *)malloc(sizeof(LockManager));
    if (!manager)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to allocate lock manager");
        return NULL;
    }

    manager->locks = NULL;

    if (pthread_mutex_init(&manager->manager_lock, NULL) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize manager mutex");
        free(manager);
        return NULL;
    }

    log_message(ss_logger, LOG_INFO, "Lock manager created");
    return manager;
}

// Destroy lock manager
void destroy_lock_manager(LockManager *manager)
{
    if (!manager)
        return;

    pthread_mutex_lock(&manager->manager_lock);

    LockInfo *current = manager->locks;
    while (current)
    {
        LockInfo *next = current->next;
        free(current);
        current = next;
    }

    pthread_mutex_unlock(&manager->manager_lock);
    pthread_mutex_destroy(&manager->manager_lock);

    free(manager);
    log_message(ss_logger, LOG_INFO, "Lock manager destroyed");
}

// Add lock information
int add_lock_info(LockManager *manager, int sentence_id, const char *username, int is_write)
{
    if (!manager || !username)
    {
        return ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&manager->manager_lock);

    LockInfo *lock_info = (LockInfo *)malloc(sizeof(LockInfo));
    if (!lock_info)
    {
        pthread_mutex_unlock(&manager->manager_lock);
        return ERR_MEMORY;
    }

    lock_info->sentence_id = sentence_id;
    strncpy(lock_info->username, username, MAX_USERNAME_LEN - 1);
    lock_info->username[MAX_USERNAME_LEN - 1] = '\0';
    lock_info->is_write_lock = is_write;
    lock_info->lock_time = time(NULL);
    lock_info->next = manager->locks;

    manager->locks = lock_info;

    pthread_mutex_unlock(&manager->manager_lock);

    log_message(ss_logger, LOG_INFO, "Added %s lock on sentence %d for user %s",
                is_write ? "write" : "read", sentence_id, username);
    return SUCCESS;
}

// Remove lock information
int remove_lock_info(LockManager *manager, int sentence_id, const char *username)
{
    if (!manager || !username)
    {
        return ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&manager->manager_lock);

    LockInfo *current = manager->locks;
    LockInfo *prev = NULL;

    while (current)
    {
        if (current->sentence_id == sentence_id &&
            strcmp(current->username, username) == 0)
        {

            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                manager->locks = current->next;
            }

            free(current);
            pthread_mutex_unlock(&manager->manager_lock);

            log_message(ss_logger, LOG_INFO, "Removed lock on sentence %d for user %s",
                        sentence_id, username);
            return SUCCESS;
        }

        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&manager->manager_lock);
    return ERR_GENERAL;
}

// Check if sentence is locked by other users
int is_sentence_locked_by_others(LockManager *manager, int sentence_id, const char *username)
{
    if (!manager || !username)
    {
        return 0;
    }

    pthread_mutex_lock(&manager->manager_lock);

    LockInfo *current = manager->locks;
    while (current)
    {
        if (current->sentence_id == sentence_id)
        {
            // If it's a write lock by someone else, sentence is locked
            if (current->is_write_lock && strcmp(current->username, username) != 0)
            {
                pthread_mutex_unlock(&manager->manager_lock);
                return 1;
            }
        }
        current = current->next;
    }

    pthread_mutex_unlock(&manager->manager_lock);
    return 0;
}

// Get lock information
LockInfo *get_lock_info(LockManager *manager, int sentence_id)
{
    if (!manager)
    {
        return NULL;
    }

    pthread_mutex_lock(&manager->manager_lock);

    LockInfo *current = manager->locks;
    while (current)
    {
        if (current->sentence_id == sentence_id)
        {
            pthread_mutex_unlock(&manager->manager_lock);
            return current;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&manager->manager_lock);
    return NULL;
}