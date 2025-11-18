#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "file_structure.h"
#include "file_undo.h"
#include "file_stream.h"
#include "../../common/include/constants.h"

// Forward declaration
typedef struct WriteContext WriteContext;

// File operations context
typedef struct FileOperationContext
{
    char storage_path[MAX_PATH_LEN];
    int ss_id;
    char ns_ip[64];
    int ns_port;
    UndoManager **undo_managers; // Array of undo managers for open files
    int undo_manager_count;
    pthread_mutex_t undo_managers_lock;
    WriteContext **active_writes; // Track active write operations
    int active_write_count;
    pthread_mutex_t active_writes_lock;
} FileOperationContext;

// Write operation context
struct WriteContext
{
    FileStructure *file;
    int sentence_index;
    char username[MAX_USERNAME_LEN];
    char filepath[MAX_PATH_LEN]; // Store filepath for tracking
    char backup_path[MAX_PATH_LEN];
    int has_backup;
};

// Initialize file operations
FileOperationContext *init_file_operations(const char *storage_path, int ss_id,
                                           const char *ns_ip, int ns_port);
void cleanup_file_operations(FileOperationContext *ctx);

// CREATE operation
int create_file(FileOperationContext *ctx, const char *filepath, const char *username);
int create_empty_file_on_disk(const char *full_path);
int notify_ns_file_created(FileOperationContext *ctx, const char *filepath, const char *username);

// Notify NS about metadata updates (size, timestamps)
int notify_ns_metadata_update(FileOperationContext *ctx, const char *filepath);

// READ operation
char *read_file(FileOperationContext *ctx, const char *filepath, const char *username, int *error_code);
FileStructure *load_and_parse_file(const char *filepath);
char *serialize_file_content(FileStructure *file);

// WRITE operation
WriteContext *begin_write(FileOperationContext *ctx, const char *filepath,
                          int sentence_index, const char *username, int *error_code);
int write_word(WriteContext *wctx, int word_index, const char *content);
int commit_write(FileOperationContext *ctx, WriteContext *wctx, const char *filepath);
void rollback_write(FileOperationContext *ctx, WriteContext *wctx, const char *filepath);
void cleanup_write_context(WriteContext *wctx);

// DELETE operation (for later)
int delete_file(FileOperationContext *ctx, const char *filepath, const char *username);
int notify_ns_file_deleted(FileOperationContext *ctx, const char *filepath, const char *username);
void cleanup_undo_manager_for_file(FileOperationContext *ctx, const char *filepath);

// Check if file is locked/in use
int is_file_locked(FileOperationContext *ctx, const char *filepath);

// UNDO operation
int undo_file(FileOperationContext *ctx, const char *filepath, const char *username);
UndoManager *get_undo_manager(FileOperationContext *ctx, const char *filepath);

// STREAM operation
StreamContext *begin_stream(FileOperationContext *ctx, const char *filepath, const char *username, int *error_code);
char *stream_next_word(StreamContext *stream_ctx);
int end_stream(StreamContext *stream_ctx);

// Simplified streaming (for testing)
int stream_file(FileOperationContext *ctx, const char *filepath, const char *username, void (*output_fn)(const char *word));

// Helper functions
int file_exists(const char *filepath);
int validate_filepath(const char *filepath);
int get_full_path(FileOperationContext *ctx, const char *filepath, char *full_path);
int ensure_directory_exists(const char *dirpath);

// Backup functions for atomic writes
int create_backup(const char *original_path, char *backup_path);
int restore_backup(const char *backup_path, const char *original_path);
int delete_backup(const char *backup_path);

#endif // FILE_OPERATIONS_H