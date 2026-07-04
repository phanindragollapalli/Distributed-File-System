#ifndef FILE_UNDO_H
#define FILE_UNDO_H

#include "file_structure.h"
#include "../../common/include/constants.h"

// Undo history entry
typedef struct UndoEntry
{
    char *file_content;              // Complete file content before last write
    time_t timestamp;                // When this state was saved
    char username[MAX_USERNAME_LEN]; // User who made the change we're undoing
} UndoEntry;

// Undo manager for a file
typedef struct UndoManager
{
    char filepath[MAX_PATH_LEN];
    UndoEntry *last_state;     // Only one undo level
    pthread_mutex_t undo_lock; // Lock for undo operations
    int has_history;           // Flag: history available
} UndoManager;

// Create undo manager
UndoManager *create_undo_manager(const char *filepath);
void destroy_undo_manager(UndoManager *manager);

// Save current state before write
int save_undo_state(UndoManager *manager, FileStructure *file, const char *username);

// Perform undo operation
int undo_last_change(UndoManager *manager, const char *filepath, FileStructure **restored_file);

// Clear undo history
void clear_undo_history(UndoManager *manager);

// Check if undo is available
int has_undo_history(UndoManager *manager);

// Get undo info
const char *get_undo_username(UndoManager *manager);
time_t get_undo_timestamp(UndoManager *manager);

#endif // FILE_UNDO_H