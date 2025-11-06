#include "../include/ss_persistence.h"
#include "../../common/include/logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

extern Logger *ss_logger;

static char metadata_dir[512] = "";

// Initialize persistence system
int init_persistence(const char *data_dir)
{
    snprintf(metadata_dir, sizeof(metadata_dir), "%s", data_dir);

    // Create metadata directory if it doesn't exist
    struct stat st = {0};
    if (stat(metadata_dir, &st) == -1)
    {
        if (mkdir(metadata_dir, 0755) == -1)
        {
            if (ss_logger)
            {
                log_message(ss_logger, LOG_ERROR, "Failed to create metadata directory: %s",
                            strerror(errno));
            }
            return -1;
        }
    }

    if (ss_logger)
    {
        log_message(ss_logger, LOG_INFO, "Initialized persistence system at %s", metadata_dir);
    }

    return 0;
}

// Save file metadata
int save_file_info(const char *filename, const char *owner)
{
    if (!filename || strlen(metadata_dir) == 0)
    {
        return -1;
    }

    char meta_path[768];
    snprintf(meta_path, sizeof(meta_path), "%s/.%s.meta", metadata_dir, filename);

    FILE *f = fopen(meta_path, "w");
    if (!f)
    {
        if (ss_logger)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to save metadata for %s: %s",
                        filename, strerror(errno));
        }
        return -1;
    }

    FileInfo info;
    strncpy(info.filename, filename, sizeof(info.filename) - 1);
    strncpy(info.owner, owner ? owner : "system", sizeof(info.owner) - 1);
    info.created = time(NULL);
    info.modified = info.created;
    info.size = 0;

    fprintf(f, "filename=%s\n", info.filename);
    fprintf(f, "owner=%s\n", info.owner);
    fprintf(f, "created=%ld\n", (long)info.created);
    fprintf(f, "modified=%ld\n", (long)info.modified);
    fprintf(f, "size=%zu\n", info.size);

    fclose(f);

    if (ss_logger)
    {
        log_message(ss_logger, LOG_DEBUG, "Saved metadata for file '%s'", filename);
    }

    return 0;
}

// Load file metadata
FileInfo *load_file_info(const char *filename)
{
    if (!filename || strlen(metadata_dir) == 0)
    {
        return NULL;
    }

    char meta_path[768];
    snprintf(meta_path, sizeof(meta_path), "%s/.%s.meta", metadata_dir, filename);

    FILE *f = fopen(meta_path, "r");
    if (!f)
    {
        return NULL;
    }

    static FileInfo info;
    memset(&info, 0, sizeof(info));

    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "filename=%255s", info.filename) == 1)
            continue;
        if (sscanf(line, "owner=%31s", info.owner) == 1)
            continue;
        long val;
        if (sscanf(line, "created=%ld", &val) == 1)
        {
            info.created = (time_t)val;
            continue;
        }
        if (sscanf(line, "modified=%ld", &val) == 1)
        {
            info.modified = (time_t)val;
            continue;
        }
        size_t size_val;
        if (sscanf(line, "size=%zu", &size_val) == 1)
        {
            info.size = size_val;
            continue;
        }
    }

    fclose(f);
    return &info;
}

// Load all file metadata from disk
int load_all_metadata(const char *storage_dir)
{
    if (ss_logger)
    {
        log_message(ss_logger, LOG_INFO, "Loading metadata from %s", storage_dir);
    }

    // In a full implementation, this would scan the storage directory
    // and load all .meta files

    return 0;
}

// Update file metadata after modification
int update_file_info(const char *filename, size_t new_size)
{
    FileInfo *info = load_file_info(filename);
    if (!info)
    {
        return -1;
    }

    char meta_path[768];
    snprintf(meta_path, sizeof(meta_path), "%s/.%s.meta", metadata_dir, filename);

    FILE *f = fopen(meta_path, "w");
    if (!f)
    {
        return -1;
    }

    info->size = new_size;
    info->modified = time(NULL);

    fprintf(f, "filename=%s\n", info->filename);
    fprintf(f, "owner=%s\n", info->owner);
    fprintf(f, "created=%ld\n", (long)info->created);
    fprintf(f, "modified=%ld\n", (long)info->modified);
    fprintf(f, "size=%zu\n", info->size);

    fclose(f);

    if (ss_logger)
    {
        log_message(ss_logger, LOG_DEBUG, "Updated metadata for file '%s' (size=%zu)",
                    filename, new_size);
    }

    return 0;
}

// Delete file metadata
int delete_file_info(const char *filename)
{
    if (!filename || strlen(metadata_dir) == 0)
    {
        return -1;
    }

    char meta_path[768];
    snprintf(meta_path, sizeof(meta_path), "%s/.%s.meta", metadata_dir, filename);

    if (unlink(meta_path) == -1)
    {
        if (ss_logger)
        {
            log_message(ss_logger, LOG_WARN, "Failed to delete metadata for %s: %s",
                        filename, strerror(errno));
        }
        return -1;
    }

    if (ss_logger)
    {
        log_message(ss_logger, LOG_INFO, "Deleted metadata for file '%s'", filename);
    }

    return 0;
}
