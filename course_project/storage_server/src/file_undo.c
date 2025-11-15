/* FILE UNDO
 * Undo functionality to revert file to previous state
 * Stores history of file states before each write operation
 * UNDO command reverts file to last saved state
 * Undo is file-specific (not user-specific)
 * Must update file on disk after undo
 */
#include "../include/file_undo.h"
#include "../include/file_parser.h"
#include "../../common/include/error_codes.h"
#include "../../common/include/logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

extern Logger *ss_logger; // Declared in ss_main.c

// Create undo manager
UndoManager *create_undo_manager(const char *filepath)
{
    if (!filepath)
    {
        log_message(ss_logger, LOG_ERROR, "Null filepath provided");
        return NULL;
    }

    UndoManager *manager = (UndoManager *)malloc(sizeof(UndoManager));
    if (!manager)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to allocate undo manager");
        return NULL;
    }

    strncpy(manager->filepath, filepath, MAX_PATH_LEN - 1);
    manager->filepath[MAX_PATH_LEN - 1] = '\0';
    manager->last_state = NULL;
    manager->has_history = 0;

    if (pthread_mutex_init(&manager->undo_lock, NULL) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize undo mutex");
        free(manager);
        return NULL;
    }

    log_message(ss_logger, LOG_INFO, "Created undo manager for: %s", filepath);
    return manager;
}

// Destroy undo manager
void destroy_undo_manager(UndoManager *manager)
{
    if (!manager)
        return;

    pthread_mutex_lock(&manager->undo_lock);

    if (manager->last_state)
    {
        if (manager->last_state->file_content)
        {
            free(manager->last_state->file_content);
        }
        free(manager->last_state);
    }

    pthread_mutex_unlock(&manager->undo_lock);
    pthread_mutex_destroy(&manager->undo_lock);

    log_message(ss_logger, LOG_INFO, "Destroyed undo manager for: %s", manager->filepath);
    free(manager);
}

// Save current state before write
int save_undo_state(UndoManager *manager, FileStructure *file, const char *username)
{
    if (!manager || !file || !username)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for save_undo_state");
        return ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&manager->undo_lock);

    // Clear previous undo state (only 1 level of undo)
    if (manager->last_state)
    {
        if (manager->last_state->file_content)
        {
            free(manager->last_state->file_content);
        }
        free(manager->last_state);
        manager->last_state = NULL;
    }

    // Create new undo entry
    UndoEntry *entry = (UndoEntry *)malloc(sizeof(UndoEntry));
    if (!entry)
    {
        pthread_mutex_unlock(&manager->undo_lock);
        log_message(ss_logger, LOG_ERROR, "Failed to allocate undo entry");
        return ERR_MEMORY;
    }

    // Serialize current file content
    entry->file_content = file_to_string(file);
    if (!entry->file_content)
    {
        free(entry);
        pthread_mutex_unlock(&manager->undo_lock);
        log_message(ss_logger, LOG_ERROR, "Failed to serialize file for undo");
        return ERR_MEMORY;
    }

    entry->timestamp = time(NULL);
    strncpy(entry->username, username, MAX_USERNAME_LEN - 1);
    entry->username[MAX_USERNAME_LEN - 1] = '\0';

    manager->last_state = entry;
    manager->has_history = 1;

    pthread_mutex_unlock(&manager->undo_lock);

    log_message(ss_logger, LOG_INFO, "Saved undo state for %s (modified by %s)",
                manager->filepath, username);

    return SUCCESS;
}

// Perform undo operation
int undo_last_change(UndoManager *manager, const char *filepath, FileStructure **restored_file)
{
    if (!manager || !filepath || !restored_file)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for undo_last_change");
        return ERR_NULL_POINTER;
    }

    pthread_mutex_lock(&manager->undo_lock);

    // Check if undo history exists
    if (!manager->has_history || !manager->last_state)
    {
        pthread_mutex_unlock(&manager->undo_lock);
        log_message(ss_logger, LOG_WARN, "No undo history available for: %s", filepath);
        return ERR_GENERAL;
    }

    // Get stored content
    const char *saved_content = manager->last_state->file_content;
    if (!saved_content)
    {
        pthread_mutex_unlock(&manager->undo_lock);
        log_message(ss_logger, LOG_ERROR, "Undo state corrupted for: %s", filepath);
        return ERR_GENERAL;
    }

    // Create new file structure from saved content
    char *filepath_copy = strdup(filepath);
    char *filename = basename(filepath_copy);

    FileStructure *file = create_file_structure(filename);
    free(filepath_copy);

    if (!file)
    {
        pthread_mutex_unlock(&manager->undo_lock);
        log_message(ss_logger, LOG_ERROR, "Failed to create file structure for undo");
        return ERR_MEMORY;
    }

    // Parse saved content
    int result = parse_file_content(file, saved_content);
    if (result != SUCCESS)
    {
        destroy_file_structure(file);
        pthread_mutex_unlock(&manager->undo_lock);
        log_message(ss_logger, LOG_ERROR, "Failed to parse undo content");
        return result;
    }

    // Save to disk
    result = save_file_to_disk(file, filepath);
    if (result != SUCCESS)
    {
        destroy_file_structure(file);
        pthread_mutex_unlock(&manager->undo_lock);
        log_message(ss_logger, LOG_ERROR, "Failed to save undo content to disk");
        return result;
    }

    log_message(ss_logger, LOG_INFO, "Successfully undone last change to: %s (was modified by %s)",
                filepath, manager->last_state->username);

    // Clear undo history after successful undo
    if (manager->last_state->file_content)
    {
        free(manager->last_state->file_content);
    }
    free(manager->last_state);
    manager->last_state = NULL;
    manager->has_history = 0;

    *restored_file = file;
    pthread_mutex_unlock(&manager->undo_lock);

    return SUCCESS;
}

// Clear undo history
void clear_undo_history(UndoManager *manager)
{
    if (!manager)
        return;

    pthread_mutex_lock(&manager->undo_lock);

    if (manager->last_state)
    {
        if (manager->last_state->file_content)
        {
            free(manager->last_state->file_content);
        }
        free(manager->last_state);
        manager->last_state = NULL;
    }

    manager->has_history = 0;

    pthread_mutex_unlock(&manager->undo_lock);

    log_message(ss_logger, LOG_DEBUG, "Cleared undo history for: %s", manager->filepath);
}

// Check if undo is available
int has_undo_history(UndoManager *manager)
{
    if (!manager)
        return 0;

    pthread_mutex_lock(&manager->undo_lock);
    int has_history = manager->has_history;
    pthread_mutex_unlock(&manager->undo_lock);

    return has_history;
}

// Get undo username
const char *get_undo_username(UndoManager *manager)
{
    if (!manager)
        return NULL;
    pthread_mutex_lock(&manager->undo_lock);
    if (!manager || !manager->last_state)
    {
        pthread_mutex_unlock(&manager->undo_lock);
        return NULL;
    }
    const char *username = manager->last_state->username;
    pthread_mutex_unlock(&manager->undo_lock);
    return username;
}

// Get undo timestamp
time_t get_undo_timestamp(UndoManager *manager)
{
    if (!manager)
        return 0;
    pthread_mutex_lock(&manager->undo_lock);
    if (!manager || !manager->last_state)
    {
        pthread_mutex_unlock(&manager->undo_lock);
        return 0;
    }
    time_t timestamp = manager->last_state->timestamp;
    pthread_mutex_unlock(&manager->undo_lock);
    return timestamp;
}