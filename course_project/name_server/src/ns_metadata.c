#include "../include/ns_metadata.h"
#include "../../common/include/logger.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern Logger *global_logger;

static FileMetadata metadata_list[512];
static int metadata_count = 0;

// Initialize metadata storage
void init_metadata_storage()
{
    metadata_count = 0;
    memset(metadata_list, 0, sizeof(metadata_list));
    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "Initialized metadata storage");
    }
}

// Add file metadata
int add_file_metadata(const char *filename, const char *owner, int ss_id)
{
    if (!filename || metadata_count >= 512)
    {
        return -1;
    }

    // Check if file already exists
    for (int i = 0; i < metadata_count; i++)
    {
        if (strcmp(metadata_list[i].filename, filename) == 0)
        {
            // Update existing metadata
            metadata_list[i].storage_server_id = ss_id;
            metadata_list[i].modified = time(NULL);
            return i;
        }
    }

    // Add new metadata
    FileMetadata *meta = &metadata_list[metadata_count];
    strncpy(meta->filename, filename, sizeof(meta->filename) - 1);

    if (owner)
    {
        strncpy(meta->owner, owner, sizeof(meta->owner) - 1);
    }
    else
    {
        strcpy(meta->owner, "system");
    }

    meta->created = time(NULL);
    meta->modified = meta->created;
    meta->last_accessed = meta->created;
    meta->size = 0;
    meta->storage_server_id = ss_id;

    metadata_count++;

    if (global_logger)
    {
        log_message(global_logger, LOG_DEBUG, "Added metadata for file '%s' (owner: %s, SS_ID: %d)",
                    filename, meta->owner, ss_id);
    }

    return metadata_count - 1;
}

// Get file metadata
FileMetadata *get_file_metadata(const char *filename)
{
    if (!filename)
        return NULL;

    for (int i = 0; i < metadata_count; i++)
    {
        if (strcmp(metadata_list[i].filename, filename) == 0)
        {
            metadata_list[i].last_accessed = time(NULL);
            return &metadata_list[i];
        }
    }

    return NULL;
}

// Update file metadata
int update_file_metadata(const char *filename, size_t new_size)
{
    FileMetadata *meta = get_file_metadata(filename);
    if (!meta)
        return -1;

    meta->size = new_size;
    meta->modified = time(NULL);

    if (global_logger)
    {
        log_message(global_logger, LOG_DEBUG, "Updated metadata for file '%s' (size: %zu)",
                    filename, new_size);
    }

    return 0;
}

// Delete file metadata
int delete_file_metadata(const char *filename)
{
    if (!filename)
        return -1;

    for (int i = 0; i < metadata_count; i++)
    {
        if (strcmp(metadata_list[i].filename, filename) == 0)
        {
            // Shift remaining entries
            for (int j = i; j < metadata_count - 1; j++)
            {
                metadata_list[j] = metadata_list[j + 1];
            }
            metadata_count--;

            if (global_logger)
            {
                log_message(global_logger, LOG_INFO, "Deleted metadata for file '%s'", filename);
            }

            return 0;
        }
    }

    return -1;
}

// List all files
int list_all_files(char files[][256], int max_count)
{
    int count = (metadata_count < max_count) ? metadata_count : max_count;

    for (int i = 0; i < count; i++)
    {
        strncpy(files[i], metadata_list[i].filename, 256);
    }

    return count;
}

// Get file count
int get_file_count()
{
    return metadata_count;
}
