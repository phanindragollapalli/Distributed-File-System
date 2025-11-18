/* FILE OPERATIONS
 * Core file operation implementations: CREATE, READ, WRITE, APPEND, DELETE
 * CREATE: Creates new empty file on disk and updates NS metadata
 * READ: Loads entire file into memory, converts to linked list, sends to client
 * WRITE: Accepts word index and content, modifies file with sentence-level locking
 * APPEND: Adds content to end of file
 * DELETE: Removes file from disk and clears metadata
 */
#include "../include/file_operations.h"
#include "../include/file_parser.h"
#include "../include/file_stream.h"
#include "../../common/include/error_codes.h"
#include "../../common/include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <libgen.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>

extern Logger *ss_logger; // Declared in ss_main.c

// Create backup file for atomic operations
int create_backup(const char *original_path, char *backup_path)
{
    if (!original_path || !backup_path)
    {
        return ERR_NULL_POINTER;
    }

    // Generate backup filename with timestamp
    snprintf(backup_path, MAX_PATH_LEN, "%s.backup.%ld", original_path, time(NULL));

    // Copy original file to backup
    FILE *src = fopen(original_path, "r");
    if (!src)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to open original file for backup: %s", original_path);
        return ERR_FILE_READ;
    }

    FILE *dst = fopen(backup_path, "w");
    if (!dst)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create backup file: %s", backup_path);
        fclose(src);
        return ERR_FILE_WRITE;
    }

    // Copy content
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        if (fwrite(buffer, 1, bytes, dst) != bytes)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to write backup file");
            fclose(src);
            fclose(dst);
            remove(backup_path);
            return ERR_FILE_WRITE;
        }
    }

    fclose(src);
    fclose(dst);

    log_message(ss_logger, LOG_INFO, "Created backup: %s", backup_path);
    return SUCCESS;
}

// Restore from backup
int restore_backup(const char *backup_path, const char *original_path)
{
    if (!backup_path || !original_path)
    {
        return ERR_NULL_POINTER;
    }

    if (!file_exists(backup_path))
    {
        log_message(ss_logger, LOG_ERROR, "Backup file not found: %s", backup_path);
        return ERR_FILE_NOT_FOUND;
    }

    // Remove original
    remove(original_path);

    // Rename backup to original
    if (rename(backup_path, original_path) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to restore backup: %s", strerror(errno));
        return ERR_FILE_WRITE;
    }

    log_message(ss_logger, LOG_INFO, "Restored from backup: %s", backup_path);
    return SUCCESS;
}

// Delete backup file
int delete_backup(const char *backup_path)
{
    if (!backup_path)
    {
        return ERR_NULL_POINTER;
    }

    if (file_exists(backup_path))
    {
        if (remove(backup_path) != 0)
        {
            log_message(ss_logger, LOG_WARN, "Failed to delete backup: %s", backup_path);
            return ERR_FILE_DELETE;
        }
        log_message(ss_logger, LOG_DEBUG, "Deleted backup: %s", backup_path);
    }

    return SUCCESS;
}

// Begin write operation
WriteContext *begin_write(FileOperationContext *ctx, const char *filepath, int sentence_index, const char *username, int *error_code)
{
    if (!ctx || !filepath || !username)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for begin_write");
        if (error_code)
            *error_code = ERR_NULL_POINTER;
        return NULL;
    }

    // Validate filepath
    int validation_result = validate_filepath(filepath);
    if (validation_result != SUCCESS)
    {
        if (error_code)
            *error_code = validation_result;
        return NULL;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        if (error_code)
            *error_code = ERR_INVALID_FORMAT;
        return NULL;
    }

    // Check if file exists
    if (!file_exists(full_path))
    {
        log_message(ss_logger, LOG_WARN, "File not found: %s", filepath);
        if (error_code)
            *error_code = ERR_FILE_NOT_FOUND;
        return NULL;
    }

    // Create write context
    WriteContext *wctx = (WriteContext *)malloc(sizeof(WriteContext));
    if (!wctx)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to allocate write context");
        if (error_code)
            *error_code = ERR_MEMORY;
        return NULL;
    }

    // Create backup
    wctx->has_backup = 0;
    if (create_backup(full_path, wctx->backup_path) != SUCCESS)
    {
        log_message(ss_logger, LOG_WARN, "Failed to create backup, continuing without backup");
    }
    else
    {
        wctx->has_backup = 1;
    }

    // Load file into memory
    wctx->file = load_and_parse_file(full_path);
    if (!wctx->file)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to load file: %s", filepath);
        if (wctx->has_backup)
        {
            delete_backup(wctx->backup_path);
        }
        free(wctx);
        if (error_code)
            *error_code = ERR_FILE_READ;
        return NULL;
    }

    // Validate / extend sentence index
    if (sentence_index < 0)
    {
        log_message(ss_logger, LOG_WARN, "Sentence index %d is negative", sentence_index);
        destroy_file_structure(wctx->file);
        if (wctx->has_backup)
        {
            delete_backup(wctx->backup_path);
        }
        free(wctx);
        if (error_code)
            *error_code = ERR_SENTENCE_OUT_OF_RANGE;
        return NULL;
    }

    if (sentence_index > wctx->file->sentence_count)
    {
        log_message(ss_logger, LOG_WARN, "Sentence index %d beyond max append position %d",
                    sentence_index, wctx->file->sentence_count);
        destroy_file_structure(wctx->file);
        if (wctx->has_backup)
        {
            delete_backup(wctx->backup_path);
        }
        free(wctx);
        if (error_code)
            *error_code = ERR_SENTENCE_OUT_OF_RANGE;
        return NULL;
    }

    if (sentence_index == wctx->file->sentence_count)
    {
        // Check if previous sentence has a delimiter (except for first sentence in empty file)
        if (sentence_index > 0)
        {
            SentenceNode *prev_sentence = get_sentence(wctx->file, sentence_index - 1);
            if (!prev_sentence)
            {
                log_message(ss_logger, LOG_ERROR, "Failed to get previous sentence %d", sentence_index - 1);
                destroy_file_structure(wctx->file);
                if (wctx->has_backup)
                {
                    delete_backup(wctx->backup_path);
                }
                free(wctx);
                if (error_code)
                    *error_code = ERR_GENERAL;
                return NULL;
            }

            // Previous sentence must have a delimiter to create a new sentence
            if (prev_sentence->delimiter == '\0')
            {
                log_message(ss_logger, LOG_WARN, "Cannot create sentence %d: previous sentence has no delimiter", sentence_index);
                destroy_file_structure(wctx->file);
                if (wctx->has_backup)
                {
                    delete_backup(wctx->backup_path);
                }
                free(wctx);
                if (error_code)
                    *error_code = ERR_SENTENCE_OUT_OF_RANGE;
                return NULL;
            }
        }

        // Append a new empty sentence to support writing to empty files or new sentences
        SentenceNode *new_sentence = create_sentence_node(sentence_index);
        if (!new_sentence)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to create sentence %d", sentence_index);
            destroy_file_structure(wctx->file);
            if (wctx->has_backup)
            {
                delete_backup(wctx->backup_path);
            }
            free(wctx);
            if (error_code)
                *error_code = ERR_MEMORY;
            return NULL;
        }

        if (add_sentence(wctx->file, new_sentence) != SUCCESS)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to append sentence %d", sentence_index);
            destroy_sentence_node(new_sentence);
            destroy_file_structure(wctx->file);
            if (wctx->has_backup)
            {
                delete_backup(wctx->backup_path);
            }
            free(wctx);
            if (error_code)
                *error_code = ERR_GENERAL;
            return NULL;
        }
    }

    wctx->sentence_index = sentence_index;
    strncpy(wctx->username, username, MAX_USERNAME_LEN - 1);
    wctx->username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(wctx->filepath, filepath, MAX_PATH_LEN - 1);
    wctx->filepath[MAX_PATH_LEN - 1] = '\0';

    // Acquire write lock on the sentence
    SentenceNode *sentence = get_sentence(wctx->file, sentence_index);
    if (!sentence)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to get sentence at index %d", sentence_index);
        destroy_file_structure(wctx->file);
        if (wctx->has_backup)
        {
            delete_backup(wctx->backup_path);
        }
        free(wctx);
        if (error_code)
            *error_code = ERR_GENERAL;
        return NULL;
    }

    int lock_result = acquire_sentence_write_lock(sentence);
    if (lock_result != SUCCESS)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to acquire write lock on sentence %d", sentence_index);
        destroy_file_structure(wctx->file);
        if (wctx->has_backup)
        {
            delete_backup(wctx->backup_path);
        }
        free(wctx);
        if (error_code)
            *error_code = ERR_FILE_LOCKED;
        return NULL;
    }

    // Register in active writes tracking
    pthread_mutex_lock(&ctx->active_writes_lock);
    WriteContext **new_writes = realloc(ctx->active_writes, (ctx->active_write_count + 1) * sizeof(WriteContext *));
    if (new_writes)
    {
        ctx->active_writes = new_writes;
        ctx->active_writes[ctx->active_write_count++] = wctx;
    }
    else
    {
        log_message(ss_logger, LOG_ERROR, "Failed to register active write (memory)");
        // Write continues but won't be tracked - acceptable degradation
    }
    pthread_mutex_unlock(&ctx->active_writes_lock);

    if (error_code)
        *error_code = SUCCESS;
    log_message(ss_logger, LOG_INFO, "Started write operation on %s sentence %d by %s", filepath, sentence_index, username);

    return wctx;
}

// Write word to sentence
int write_word(WriteContext *wctx, int word_index, const char *content)
{
    if (!wctx || !wctx->file || !content)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for write_word");
        return ERR_NULL_POINTER;
    }

    SentenceNode *sentence = get_sentence(wctx->file, wctx->sentence_index);
    if (!sentence)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to get sentence");
        return ERR_GENERAL;
    }

    // Validate word index (0-based indexing)
    // Index 0 = insert before first word (or at start of empty sentence)
    // Index 1 = insert after first word
    // Index N = insert after Nth word (append if N == word_count)
    // Valid range: [0, word_count] inclusive
    if (word_index < 0 || word_index > sentence->word_count)
    {
        log_message(ss_logger, LOG_WARN, "Word index %d out of range (max: %d)",
                    word_index, sentence->word_count);
        return ERR_WORD_OUT_OF_RANGE;
    }

    // Check if content contains sentence delimiters
    int has_delimiter = 0;
    char delimiter = '.';
    int delimiter_pos = -1;

    for (int i = 0; content[i] != '\0'; i++)
    {
        if (is_sentence_delimiter(content[i]))
        {
            has_delimiter = 1;
            delimiter = content[i];
            delimiter_pos = i;
            break;
        }
    }

    if (!has_delimiter)
    {
        // Simple word insertion
        int result = insert_word_at_index(sentence, word_index, content);
        if (result != SUCCESS)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to insert word at index %d", word_index);
            return result;
        }

        log_message(ss_logger, LOG_DEBUG, "Inserted word '%s' at index %d in sentence %d",
                    content, word_index, wctx->sentence_index);
    }
    else
    {
        // Content has delimiter - need to split sentence
        // Insert word before delimiter (if any text before delimiter)
        int actual_insert_pos = word_index;
        if (delimiter_pos > 0)
        {
            if (delimiter_pos >= MAX_WORD_LEN)
            {
                log_message(ss_logger, LOG_ERROR, "Word before delimiter exceeds MAX_WORD_LEN");
                return ERR_WORD_OUT_OF_RANGE;
            }
            char word_before[MAX_WORD_LEN];
            strncpy(word_before, content, delimiter_pos);
            word_before[delimiter_pos] = '\0';

            int result = insert_word_at_index(sentence, word_index, word_before);
            if (result != SUCCESS)
            {
                log_message(ss_logger, LOG_ERROR, "Failed to insert word before delimiter");
                return result;
            }
            actual_insert_pos = word_index + 1; // Move split point after inserted word
        }

        // Check if there are words after the insertion point OR content after delimiter
        WordNode *split_point = sentence->words;
        WordNode *prev = NULL;

        // Navigate to the split point
        for (int i = 0; i < actual_insert_pos && split_point; i++)
        {
            prev = split_point;
            split_point = split_point->next;
        }

        // Check if there's remaining content after delimiter in the input
        const char *remaining = NULL;
        int has_remaining_content = 0;
        if (delimiter_pos + 1 < strlen(content))
        {
            remaining = content + delimiter_pos + 1;
            // Skip leading spaces
            while (*remaining && isspace(*remaining))
            {
                remaining++;
            }
            if (strlen(remaining) > 0)
            {
                has_remaining_content = 1;
            }
        }

        // Only create a new sentence if there are words to move OR content after delimiter
        if (split_point || has_remaining_content)
        {
            // Create new sentence for words after delimiter (and any remaining content)
            SentenceNode *new_sentence = create_sentence_node(wctx->file->sentence_count);
            if (!new_sentence)
            {
                log_message(ss_logger, LOG_ERROR, "Failed to create new sentence");
                return ERR_MEMORY;
            }

            // Move words after split point to new sentence
            if (split_point)
            {
                new_sentence->words = split_point;
                if (prev)
                {
                    prev->next = NULL; // Cut the current sentence
                }
                else
                {
                    sentence->words = NULL; // Current sentence becomes empty
                }

                // Count words in new sentence
                WordNode *counter = new_sentence->words;
                while (counter)
                {
                    new_sentence->word_count++;
                    counter = counter->next;
                }

                // Update current sentence word count
                sentence->word_count = 0;
                counter = sentence->words;
                while (counter)
                {
                    sentence->word_count++;
                    counter = counter->next;
                }
            }

            // Set delimiter for current sentence
            sentence->delimiter = delimiter;

            // Insert new sentence after current one
            new_sentence->next = sentence->next;
            sentence->next = new_sentence;
            wctx->file->sentence_count++;

            // Re-index all sentences from the insertion point
            int new_id = wctx->sentence_index;
            SentenceNode *current = sentence;
            while (current)
            {
                current->sentence_id = new_id++;
                current = current->next;
            }

            log_message(ss_logger, LOG_INFO, "Split sentence at index %d with delimiter '%c'",
                        wctx->sentence_index, delimiter);

            // Process remaining content after delimiter (if any in the input)
            if (has_remaining_content)
            {
                // Recursively process remaining content into the new sentence
                WriteContext recursive_ctx = *wctx;
                recursive_ctx.sentence_index = wctx->sentence_index + 1; // Point to new sentence

                int result = write_word(&recursive_ctx, 0, remaining); // Insert at beginning of new sentence

                // Copy back the file pointer in case it changed
                wctx->file = recursive_ctx.file;

                if (result != SUCCESS)
                {
                    log_message(ss_logger, LOG_ERROR, "Failed to process remaining content recursively");
                    return result;
                }
            }
        }
        else
        {
            // No words after insertion point and no remaining content
            // Just set the delimiter for current sentence
            sentence->delimiter = delimiter;
            log_message(ss_logger, LOG_INFO, "Set delimiter '%c' for sentence %d (no split needed)",
                        delimiter, wctx->sentence_index);
        }
    }

    return SUCCESS;
}

// Commit write operation
int commit_write(FileOperationContext *ctx, WriteContext *wctx, const char *filepath)
{
    if (!ctx || !wctx || !wctx->file || !filepath)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for commit_write");
        return ERR_NULL_POINTER;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return ERR_INVALID_FORMAT;
    }

    // Get undo manager
    UndoManager *undo_mgr = get_undo_manager(ctx, filepath);

    // Save undo state BEFORE committing write
    if (undo_mgr && file_exists(full_path))
    {
        // Load current file state
        FileStructure *current_file = load_and_parse_file(full_path);
        if (current_file)
        {
            save_undo_state(undo_mgr, current_file, wctx->username);
            destroy_file_structure(current_file);
        }
    }

    // Save file to disk BEFORE releasing lock (prevents race condition)
    // This ensures no other thread can load and modify the file while we're saving
    int result = save_file_to_disk(wctx->file, full_path);

    // Deregister from active writes tracking (after save, before lock release)
    pthread_mutex_lock(&ctx->active_writes_lock);
    for (int i = 0; i < ctx->active_write_count; i++)
    {
        if (ctx->active_writes[i] == wctx)
        {
            // Shift remaining elements
            for (int j = i; j < ctx->active_write_count - 1; j++)
            {
                ctx->active_writes[j] = ctx->active_writes[j + 1];
            }
            ctx->active_write_count--;
            break;
        }
    }
    pthread_mutex_unlock(&ctx->active_writes_lock);

    // Now release write lock (after file is safely saved)
    SentenceNode *sentence = get_sentence(wctx->file, wctx->sentence_index);
    if (sentence)
    {
        release_sentence_write_lock(sentence);
    }

    // Check save result after releasing lock
    if (result != SUCCESS)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to save file to disk");

        // Restore from backup if available
        if (wctx->has_backup)
        {
            log_message(ss_logger, LOG_INFO, "Attempting to restore from backup");
            restore_backup(wctx->backup_path, full_path);
        }

        return result;
    }

    // Delete backup on successful commit
    if (wctx->has_backup)
    {
        delete_backup(wctx->backup_path);
    }

    log_message(ss_logger, LOG_INFO, "Committed write operation on %s by %s",
                filepath, wctx->username);

    // Notify name server of metadata update (file size/timestamp changed)
    notify_ns_metadata_update(ctx, filepath);

    return SUCCESS;
}

// Rollback write operation
void rollback_write(FileOperationContext *ctx, WriteContext *wctx, const char *filepath)
{
    if (!wctx)
        return;

    log_message(ss_logger, LOG_WARN, "Rolling back write operation");

    // Deregister from active writes tracking
    if (ctx)
    {
        pthread_mutex_lock(&ctx->active_writes_lock);
        for (int i = 0; i < ctx->active_write_count; i++)
        {
            if (ctx->active_writes[i] == wctx)
            {
                // Shift remaining elements
                for (int j = i; j < ctx->active_write_count - 1; j++)
                {
                    ctx->active_writes[j] = ctx->active_writes[j + 1];
                }
                ctx->active_write_count--;
                break;
            }
        }
        pthread_mutex_unlock(&ctx->active_writes_lock);
    }

    // Release lock if held
    if (wctx->file)
    {
        SentenceNode *sentence = get_sentence(wctx->file, wctx->sentence_index);
        if (sentence)
        {
            release_sentence_write_lock(sentence);
        }
    }

    const char *requested_path = filepath;
    if ((!requested_path || requested_path[0] == '\0') && wctx->filepath[0] != '\0')
    {
        requested_path = wctx->filepath;
    }

    const char *restore_target = requested_path;
    char full_path[MAX_PATH_LEN];
    if (ctx && requested_path && requested_path[0] != '\0')
    {
        size_t storage_len = strlen(ctx->storage_path);
        if (strncmp(requested_path, ctx->storage_path, storage_len) != 0)
        {
            if (get_full_path(ctx, requested_path, full_path) == SUCCESS)
            {
                restore_target = full_path;
            }
        }
    }

    if (wctx->has_backup && restore_target && restore_target[0] != '\0')
    {
        restore_backup(wctx->backup_path, restore_target);
    }
}

// Cleanup write context
void cleanup_write_context(WriteContext *wctx)
{
    if (!wctx)
        return;

    if (wctx->file)
    {
        destroy_file_structure(wctx->file);
    }

    free(wctx);
    log_message(ss_logger, LOG_DEBUG, "Cleaned up write context");
}

// DELETE operation
int delete_file(FileOperationContext *ctx, const char *filepath, const char *username)
{
    if (!ctx || !filepath || !username)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for delete_file");
        return ERR_NULL_POINTER;
    }

    // Validate filepath
    int validation_result = validate_filepath(filepath);
    if (validation_result != SUCCESS)
    {
        return validation_result;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return ERR_INVALID_FORMAT;
    }

    // Check if file exists
    if (!file_exists(full_path))
    {
        log_message(ss_logger, LOG_WARN, "File not found: %s", filepath);
        return ERR_FILE_NOT_FOUND;
    }

    log_message(ss_logger, LOG_INFO, "Attempting to delete file: %s by user %s",
                filepath, username);

    // Step 1: Clear undo history for this file
    UndoManager *undo_mgr = get_undo_manager(ctx, filepath);
    if (undo_mgr)
    {
        clear_undo_history(undo_mgr);
        log_message(ss_logger, LOG_DEBUG, "Cleared undo history for: %s", filepath);
    }

    // Step 2: Remove any backup files that might exist
    char dir_path[MAX_PATH_LEN];
    strncpy(dir_path, full_path, MAX_PATH_LEN - 1);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash)
    {
        *last_slash = '\0'; // Terminate to get directory path
        const char *filename = last_slash + 1;

        DIR *dir = opendir(dir_path);
        if (dir)
        {
            struct dirent *entry;
            while ((entry = readdir(dir)))
            {
                // Check if entry matches: <filename>.backup.<timestamp>
                if (strncmp(entry->d_name, filename, strlen(filename)) == 0 &&
                    strstr(entry->d_name, ".backup."))
                {
                    // Validate combined path length before constructing
                    size_t needed = strlen(dir_path) + 1 + strlen(entry->d_name) + 1;
                    if (needed > MAX_PATH_LEN)
                    {
                        log_message(ss_logger, LOG_WARN,
                                    "Backup path too long, skipping: %s (needs %zu bytes)",
                                    entry->d_name, needed);
                        continue;
                    }

                    char backup_file[MAX_PATH_LEN];
                    // Safe: Length validated above, suppress false positive warning
                    snprintf(backup_file, sizeof(backup_file), "%s/%s",
                             dir_path, entry->d_name);

                    if (remove(backup_file) == 0)
                    {
                        log_message(ss_logger, LOG_DEBUG,
                                    "Removed backup file: %s", entry->d_name);
                    }
                    else
                    {
                        log_message(ss_logger, LOG_WARN,
                                    "Failed to remove backulog_messagep: %s (errno: %d)",
                                    entry->d_name, errno);
                    }
                }
            }
            closedir(dir);
        }
        else
        {
            log_message(ss_logger, LOG_WARN,
                        "Failed to open directory for backup cleanup: %s", dir_path);
        }
    }

    // Step 3: Delete the actual file
    if (remove(full_path) != 0)
    {
        int err = errno;
        log_message(ss_logger, LOG_ERROR, "Failed to delete file: %s - %s (errno: %d)",
                    filepath, strerror(err), err);

        // Check specific error conditions
        if (err == EACCES || err == EPERM)
        {
            return ERR_PERMISSION_DENIED;
        }
        else if (err == EBUSY)
        {
            return ERR_FILE_LOCKED;
        }
        else
        {
            return ERR_FILE_DELETE;
        }
    }

    // Step 4: Notify Name Server to update metadata
    int ns_result = notify_ns_file_deleted(ctx, filepath, username);
    if (ns_result != SUCCESS)
    {
        log_message(ss_logger, LOG_WARN,
                    "File deleted locally but failed to notify NS for: %s", filepath);
        // Don't fail the operation - file is already deleted
    }

    // Step 5: Remove undo manager from context
    cleanup_undo_manager_for_file(ctx, filepath);

    log_message(ss_logger, LOG_INFO, "Successfully deleted file: %s by user %s",
                filepath, username);

    return SUCCESS;
}

// Notify Name Server about file deletion
int notify_ns_file_deleted(FileOperationContext *ctx, const char *filepath, const char *username)
{
    // Placeholder - will be implemented when Person 1 completes NS communication

    log_message(ss_logger, LOG_INFO,
                "Would notify NS at %s:%d about file deletion: %s by user %s",
                ctx->ns_ip, ctx->ns_port, filepath, username);

    // TODO: Implement actual network communication with NS
    // - Connect to NS
    // - Send DELETE_FILE_METADATA message
    // - Include: filepath, ss_id, username, timestamp
    // - Wait for ACK
    // - Clear ACL entries at NS

    return SUCCESS;
}

// Remove undo manager for a specific file
void cleanup_undo_manager_for_file(FileOperationContext *ctx, const char *filepath)
{
    if (!ctx || !filepath)
        return;

    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return;
    }

    pthread_mutex_lock(&ctx->undo_managers_lock);

    // Find and remove the undo manager
    for (int i = 0; i < ctx->undo_manager_count; i++)
    {
        if (strcmp(ctx->undo_managers[i]->filepath, full_path) == 0)
        {
            // Destroy this manager
            destroy_undo_manager(ctx->undo_managers[i]);

            // Shift remaining managers
            for (int j = i; j < ctx->undo_manager_count - 1; j++)
            {
                ctx->undo_managers[j] = ctx->undo_managers[j + 1];
            }

            ctx->undo_manager_count--;

            log_message(ss_logger, LOG_DEBUG,
                        "Removed undo manager for deleted file: %s", filepath);
            break;
        }
    }

    pthread_mutex_unlock(&ctx->undo_managers_lock);
}

// Check if file is currently locked by any operation
int is_file_locked(FileOperationContext *ctx, const char *filepath)
{
    if (!ctx || !filepath)
        return 0;

    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return 0;
    }

    // Check active write operations (tracks actual in-memory locks)
    pthread_mutex_lock(&ctx->active_writes_lock);

    int is_locked = 0;
    for (int i = 0; i < ctx->active_write_count; i++)
    {
        WriteContext *wctx = ctx->active_writes[i];

        // Get the full path of the active write
        char wctx_full_path[MAX_PATH_LEN];
        if (get_full_path(ctx, wctx->filepath, wctx_full_path) != SUCCESS)
        {
            continue;
        }

        if (strcmp(wctx_full_path, full_path) == 0)
        {
            is_locked = 1;
            break;
        }
    }

    pthread_mutex_unlock(&ctx->active_writes_lock);
    return is_locked;
}

// Initialize file operations context
FileOperationContext *init_file_operations(const char *storage_path, int ss_id,
                                           const char *ns_ip, int ns_port)
{
    if (!storage_path || !ns_ip)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for file operations init");
        return NULL;
    }

    FileOperationContext *ctx = (FileOperationContext *)malloc(sizeof(FileOperationContext));
    if (!ctx)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to allocate file operations context");
        return NULL;
    }

    strncpy(ctx->storage_path, storage_path, MAX_PATH_LEN - 1);
    ctx->storage_path[MAX_PATH_LEN - 1] = '\0';
    ctx->ss_id = ss_id;
    strncpy(ctx->ns_ip, ns_ip, 63);
    ctx->ns_ip[63] = '\0';
    ctx->ns_port = ns_port;

    // Ensure storage directory exists
    if (ensure_directory_exists(storage_path) != SUCCESS)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create storage directory: %s", storage_path);
        free(ctx);
        return NULL;
    }

    ctx->undo_managers = NULL;
    ctx->undo_manager_count = 0;

    if (pthread_mutex_init(&ctx->undo_managers_lock, NULL) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize undo managers lock");
        free(ctx);
        return NULL;
    }

    // Initialize active writes tracking
    ctx->active_writes = NULL;
    ctx->active_write_count = 0;

    if (pthread_mutex_init(&ctx->active_writes_lock, NULL) != 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to initialize active writes lock");
        pthread_mutex_destroy(&ctx->undo_managers_lock);
        free(ctx);
        return NULL;
    }

    log_message(ss_logger, LOG_INFO, "File operations initialized with storage path: %s", storage_path);
    return ctx;
}

// Cleanup file operations
void cleanup_file_operations(FileOperationContext *ctx)
{
    if (!ctx)
        return;

    // Clean up undo managers
    pthread_mutex_lock(&ctx->undo_managers_lock);
    for (int i = 0; i < ctx->undo_manager_count; i++)
    {
        destroy_undo_manager(ctx->undo_managers[i]);
    }
    if (ctx->undo_managers)
    {
        free(ctx->undo_managers);
    }
    pthread_mutex_unlock(&ctx->undo_managers_lock);
    pthread_mutex_destroy(&ctx->undo_managers_lock);

    // Clean up active writes tracking
    pthread_mutex_lock(&ctx->active_writes_lock);
    if (ctx->active_writes)
    {
        free(ctx->active_writes);
    }
    pthread_mutex_unlock(&ctx->active_writes_lock);
    pthread_mutex_destroy(&ctx->active_writes_lock);

    log_message(ss_logger, LOG_INFO, "Cleaning up file operations context");
    free(ctx);
}

// Ensure directory exists (create if needed)
int ensure_directory_exists(const char *dirpath)
{
    struct stat st = {0};

    if (stat(dirpath, &st) == -1)
    {
        // Directory doesn't exist, create it
        char temp[MAX_PATH_LEN];
        char *path_copy = strdup(dirpath);
        char *token;
        char *rest = path_copy;

        temp[0] = '\0';

        // while ((token = strtok_r(rest, "/", &rest))) {
        //     if (strlen(temp) > 0) {
        //         strcat(temp, "/");
        //     }
        //     strcat(temp, token);
        int offset = 0;
        while ((token = strtok_r(rest, "/", &rest)))
        {
            int needed = strlen(token) + 2; // token + "/" + null
            if (offset + needed >= MAX_PATH_LEN)
            {
                log_message(ss_logger, LOG_ERROR, "Path too long");
                return ERR_INVALID_FORMAT;
            }
            if (offset > 0)
            {
                temp[offset++] = '/';
            }
            strcpy(temp + offset, token);
            offset += strlen(token);

            if (mkdir(temp, 0755) == -1 && errno != EEXIST)
            {
                log_message(ss_logger, LOG_ERROR, "Failed to create directory: %s - %s",
                            temp, strerror(errno));
                free(path_copy);
                return ERR_GENERAL;
            }
        }

        free(path_copy);
        log_message(ss_logger, LOG_INFO, "Created directory: %s", dirpath);
    }

    return SUCCESS;
}

// Get full path from relative filepath
int get_full_path(FileOperationContext *ctx, const char *filepath, char *full_path)
{
    int needed = strlen(ctx->storage_path) + 1 + strlen(filepath) + 1;

    if (needed > MAX_PATH_LEN)
    {
        log_message(ss_logger, LOG_ERROR, "Combined path exceeds MAX_PATH_LEN");
        return ERR_INVALID_FORMAT;
    }
    // Safe: Length validated above, suppress false positive warning
    snprintf(full_path, MAX_PATH_LEN, "%s/%s", ctx->storage_path, filepath);
    return SUCCESS;
}

// Validate filepath
int validate_filepath(const char *filepath)
{
    if (!filepath || strlen(filepath) == 0)
    {
        return ERR_INVALID_ARGS;
    }

    // Check for invalid characters
    if (strstr(filepath, "..") != NULL)
    {
        log_message(ss_logger, LOG_WARN, "Invalid filepath with '..': %s", filepath);
        return ERR_INVALID_FORMAT;
    }

    return SUCCESS;
}

// Check if file exists
int file_exists(const char *filepath)
{
    struct stat st;
    return (stat(filepath, &st) == 0);
}

// Create empty file on disk
int create_empty_file_on_disk(const char *full_path)
{
    // Extract directory path
    char *path_copy = strdup(full_path);
    char *dir = dirname(path_copy);

    // Ensure parent directory exists
    if (ensure_directory_exists(dir) != SUCCESS)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create parent directory for: %s", full_path);
        free(path_copy);
        return ERR_FILE_WRITE;
    }
    free(path_copy);

    // Create empty file
    FILE *fp = fopen(full_path, "w");
    if (!fp)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create file: %s - %s",
                    full_path, strerror(errno));
        return ERR_FILE_WRITE;
    }

    fclose(fp);
    log_message(ss_logger, LOG_INFO, "Created empty file: %s", full_path);
    return SUCCESS;
}

// Notify Name Server about file creation
int notify_ns_file_created(FileOperationContext *ctx, const char *filepath, const char *username)
{
    // This is a placeholder - will be implemented when Person 1 completes NS communication
    // For now, we'll just log the notification

    log_message(ss_logger, LOG_INFO,
                "Would notify NS at %s:%d about file creation: %s by user %s",
                ctx->ns_ip, ctx->ns_port, filepath, username);

    // TODO: Implement actual network communication with NS
    // - Connect to NS
    // - Send CREATE_FILE_METADATA message
    // - Include: filepath, ss_id, username, timestamp
    // - Wait for ACK

    return SUCCESS;
}

// CREATE operation - main function
int create_file(FileOperationContext *ctx, const char *filepath, const char *username)
{
    if (!ctx || !filepath || !username)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for create_file");
        return ERR_NULL_POINTER;
    }

    // Validate filepath
    int validation_result = validate_filepath(filepath);
    if (validation_result != SUCCESS)
    {
        return validation_result;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return ERR_INVALID_FORMAT;
    }

    // Check if file already exists
    if (file_exists(full_path))
    {
        log_message(ss_logger, LOG_WARN, "File already exists: %s", filepath);
        return ERR_FILE_EXISTS;
    }

    // Create empty file on disk
    int result = create_empty_file_on_disk(full_path);
    if (result != SUCCESS)
    {
        return result;
    }

    // Notify Name Server
    result = notify_ns_file_created(ctx, filepath, username);
    if (result != SUCCESS)
    {
        log_message(ss_logger, LOG_WARN, "Failed to notify NS, but file created locally");
        // Don't fail the operation - file is created
    }

    log_message(ss_logger, LOG_INFO, "Successfully created file: %s by user %s",
                filepath, username);
    return SUCCESS;
}

// Load and parse file into structure
FileStructure *load_and_parse_file(const char *filepath)
{
    if (!filepath)
    {
        log_message(ss_logger, LOG_ERROR, "Null filepath provided to load_and_parse_file");
        return NULL;
    }

    // Check if file exists
    if (!file_exists(filepath))
    {
        log_message(ss_logger, LOG_ERROR, "File does not exist: %s", filepath);
        return NULL;
    }

    // Extract filename from path
    char *path_copy = strdup(filepath);
    char *filename = basename(path_copy);

    // Create file structure
    FileStructure *file = create_file_structure(filename);
    if (!file)
    {
        free(path_copy);
        return NULL;
    }
    free(path_copy);

    // Load from disk
    int result = load_file_from_disk(file, filepath);
    if (result != SUCCESS)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to load file from disk: %s", filepath);
        destroy_file_structure(file);
        return NULL;
    }

    log_message(ss_logger, LOG_INFO, "Loaded and parsed file: %s (%d sentences, %d words)",
                filepath, file->sentence_count, count_total_words(file));

    return file;
}

// Serialize file content to string
char *serialize_file_content(FileStructure *file)
{
    if (!file)
    {
        log_message(ss_logger, LOG_ERROR, "Null file structure provided to serialize");
        return NULL;
    }

    char *content = file_to_string(file);
    if (!content)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to serialize file: %s", file->filename);
        return NULL;
    }

    log_message(ss_logger, LOG_DEBUG, "Serialized file %s to string (%zu bytes)",
                file->filename, strlen(content));

    return content;
}

// READ operation - main function
char *read_file(FileOperationContext *ctx, const char *filepath, const char *username, int *error_code)
{
    if (!ctx || !filepath || !username)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for read_file");
        if (error_code)
            *error_code = ERR_NULL_POINTER;
        return NULL;
    }

    // Validate filepath
    int validation_result = validate_filepath(filepath);
    if (validation_result != SUCCESS)
    {
        if (error_code)
            *error_code = validation_result;
        return NULL;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        if (error_code)
            *error_code = ERR_INVALID_FORMAT;
        return NULL;
    }

    // Check if file exists
    if (!file_exists(full_path))
    {
        log_message(ss_logger, LOG_WARN, "File not found: %s", filepath);
        if (error_code)
            *error_code = ERR_FILE_NOT_FOUND;
        return NULL;
    }

    // Load and parse file
    FileStructure *file = load_and_parse_file(full_path);
    if (!file)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to load file: %s", filepath);
        if (error_code)
            *error_code = ERR_FILE_READ;
        return NULL;
    }

    // Serialize to string
    char *content = serialize_file_content(file);
    if (!content)
    {
        destroy_file_structure(file);
        if (error_code)
            *error_code = ERR_MEMORY;
        return NULL;
    }

    // Clean up file structure (content is now in string)
    destroy_file_structure(file);

    if (error_code)
        *error_code = SUCCESS;
    log_message(ss_logger, LOG_INFO, "Successfully read file: %s by user %s",
                filepath, username);

    return content;
}

UndoManager *get_undo_manager(FileOperationContext *ctx, const char *filepath)
{
    if (!ctx || !filepath)
        return NULL;

    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return NULL;
    }

    pthread_mutex_lock(&ctx->undo_managers_lock);

    // Search for existing undo manager
    for (int i = 0; i < ctx->undo_manager_count; i++)
    {
        if (strcmp(ctx->undo_managers[i]->filepath, full_path) == 0)
        {
            pthread_mutex_unlock(&ctx->undo_managers_lock);
            return ctx->undo_managers[i];
        }
    }

    // Create new undo manager
    UndoManager *manager = create_undo_manager(full_path);
    if (!manager)
    {
        pthread_mutex_unlock(&ctx->undo_managers_lock);
        return NULL;
    }

    // Add to array
    UndoManager **new_array = (UndoManager **)realloc(
        ctx->undo_managers,
        (ctx->undo_manager_count + 1) * sizeof(UndoManager *));

    if (!new_array)
    {
        destroy_undo_manager(manager);
        pthread_mutex_unlock(&ctx->undo_managers_lock);
        return NULL;
    }

    ctx->undo_managers = new_array;
    ctx->undo_managers[ctx->undo_manager_count] = manager;
    ctx->undo_manager_count++;

    pthread_mutex_unlock(&ctx->undo_managers_lock);

    return manager;
}

// UNDO operation
int undo_file(FileOperationContext *ctx, const char *filepath, const char *username)
{
    if (!ctx || !filepath || !username)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for undo_file");
        return ERR_NULL_POINTER;
    }

    // Validate filepath
    int validation_result = validate_filepath(filepath);
    if (validation_result != SUCCESS)
    {
        return validation_result;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return ERR_INVALID_FORMAT;
    }

    // Check if file exists
    if (!file_exists(full_path))
    {
        log_message(ss_logger, LOG_WARN, "File not found: %s", filepath);
        return ERR_FILE_NOT_FOUND;
    }

    // Get undo manager
    UndoManager *undo_mgr = get_undo_manager(ctx, filepath);
    if (!undo_mgr)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to get undo manager");
        return ERR_GENERAL;
    }

    // Check if undo history exists
    if (!has_undo_history(undo_mgr))
    {
        log_message(ss_logger, LOG_WARN, "No undo history for: %s", filepath);
        return ERR_GENERAL;
    }

    // Perform undo
    FileStructure *restored_file = NULL;
    int result = undo_last_change(undo_mgr, full_path, &restored_file);

    if (result != SUCCESS)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to undo changes to: %s", filepath);
        return result;
    }

    // Clean up restored file structure
    if (restored_file)
    {
        destroy_file_structure(restored_file);
    }

    log_message(ss_logger, LOG_INFO, "Successfully undone changes to: %s by user %s",
                filepath, username);

    return SUCCESS;
}

// Begin streaming operation
StreamContext *begin_stream(FileOperationContext *ctx, const char *filepath,
                            const char *username, int *error_code)
{
    if (!ctx || !filepath || !username)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for begin_stream");
        if (error_code)
            *error_code = ERR_NULL_POINTER;
        return NULL;
    }

    // Validate filepath
    int validation_result = validate_filepath(filepath);
    if (validation_result != SUCCESS)
    {
        if (error_code)
            *error_code = validation_result;
        return NULL;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        if (error_code)
            *error_code = ERR_INVALID_FORMAT;
        return NULL;
    }

    // Check if file exists
    if (!file_exists(full_path))
    {
        log_message(ss_logger, LOG_WARN, "File not found for streaming: %s", filepath);
        if (error_code)
            *error_code = ERR_FILE_NOT_FOUND;
        return NULL;
    }

    // Load file
    FileStructure *file = load_and_parse_file(full_path);
    if (!file)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to load file for streaming: %s", filepath);
        if (error_code)
            *error_code = ERR_FILE_READ;
        return NULL;
    }

    // Create stream context
    StreamContext *stream_ctx = create_stream_context(file, STREAM_DELAY_MS);
    if (!stream_ctx)
    {
        destroy_file_structure(file);
        if (error_code)
            *error_code = ERR_MEMORY;
        return NULL;
    }

    // Start streaming
    start_stream(stream_ctx);

    if (error_code)
        *error_code = SUCCESS;
    log_message(ss_logger, LOG_INFO, "Started streaming %s to user %s", filepath, username);

    return stream_ctx;
}

// Get next word in stream
char *stream_next_word(StreamContext *stream_ctx)
{
    if (!stream_ctx)
    {
        return NULL;
    }

    return get_next_word(stream_ctx);
}

// End streaming
int end_stream(StreamContext *stream_ctx)
{
    if (!stream_ctx)
    {
        return ERR_NULL_POINTER;
    }

    stop_stream(stream_ctx);

    // Note: Don't destroy file structure here, it's managed by StreamContext
    destroy_stream_context(stream_ctx);

    log_message(ss_logger, LOG_INFO, "Ended streaming session");
    return SUCCESS;
}

// Simplified streaming for testing
int stream_file(FileOperationContext *ctx, const char *filepath,
                const char *username, void (*output_fn)(const char *word))
{
    if (!ctx || !filepath || !username || !output_fn)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for stream_file");
        return ERR_NULL_POINTER;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return ERR_INVALID_FORMAT;
    }

    // Check if file exists
    if (!file_exists(full_path))
    {
        log_message(ss_logger, LOG_WARN, "File not found: %s", filepath);
        return ERR_FILE_NOT_FOUND;
    }

    // Load file
    FileStructure *file = load_and_parse_file(full_path);
    if (!file)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to load file: %s", filepath);
        return ERR_FILE_READ;
    }

    // Stream it
    int result = stream_file_to_output(file, output_fn, STREAM_DELAY_MS);

    // Cleanup
    destroy_file_structure(file);

    if (result == SUCCESS)
    {
        log_message(ss_logger, LOG_INFO, "Successfully streamed file: %s to user %s",
                    filepath, username);
    }

    return result;
}

// Notify name server of metadata update (file size, timestamps)
int notify_ns_metadata_update(FileOperationContext *ctx, const char *filepath)
{
    if (!ctx || !filepath)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid parameters for notify_ns_metadata_update");
        return ERR_NULL_POINTER;
    }

    // Get full path
    char full_path[MAX_PATH_LEN];
    if (get_full_path(ctx, filepath, full_path) != SUCCESS)
    {
        return ERR_INVALID_FORMAT;
    }

    // Get file size
    struct stat st;
    if (stat(full_path, &st) != 0)
    {
        log_message(ss_logger, LOG_WARN, "Failed to stat file for metadata update: %s", filepath);
        return ERR_FILE_NOT_FOUND;
    }

    size_t file_size = st.st_size;

    // Connect to name server
    int ns_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ns_sock < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create socket for NS metadata update");
        return ERR_GENERAL;
    }

    struct sockaddr_in ns_addr;
    memset(&ns_addr, 0, sizeof(ns_addr));
    ns_addr.sin_family = AF_INET;
    ns_addr.sin_port = htons(ctx->ns_port);
    if (inet_pton(AF_INET, ctx->ns_ip, &ns_addr.sin_addr) <= 0)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid NS IP address: %s", ctx->ns_ip);
        close(ns_sock);
        return ERR_GENERAL;
    }

    if (connect(ns_sock, (struct sockaddr *)&ns_addr, sizeof(ns_addr)) < 0)
    {
        log_message(ss_logger, LOG_WARN, "Failed to connect to NS for metadata update");
        close(ns_sock);
        return ERR_GENERAL;
    }

    // Send metadata update command: "UPDATE_METADATA <filename> <size>\n"
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "UPDATE_METADATA %s %zu\n", filepath, file_size);
    if (write(ns_sock, cmd, strlen(cmd)) < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to send metadata update to NS");
        close(ns_sock);
        return ERR_GENERAL;
    }

    // Wait for acknowledgment (optional but good practice)
    char response[256];
    int n = read(ns_sock, response, sizeof(response) - 1);
    if (n > 0)
    {
        response[n] = '\0';
        if (strncmp(response, "ACK", 3) == 0)
        {
            log_message(ss_logger, LOG_DEBUG, "NS metadata update ACK for file: %s", filepath);
        }
        else
        {
            log_message(ss_logger, LOG_WARN, "NS metadata update unexpected response: %s", response);
        }
    }

    close(ns_sock);
    return SUCCESS;
}