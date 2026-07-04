#ifndef SS_PERSISTENCE_H
#define SS_PERSISTENCE_H

#include <time.h>

typedef struct FileInfo
{
    char filename[256];
    char owner[32];
    time_t created;
    time_t modified;
    size_t size;
} FileInfo;

// Initialize persistence system
int init_persistence(const char *data_dir);

// Save file metadata
int save_file_info(const char *filename, const char *owner);

// Load file metadata
FileInfo *load_file_info(const char *filename);

// Load all file metadata from disk
int load_all_metadata(const char *storage_dir);

// Update file metadata after modification
int update_file_info(const char *filename, size_t new_size);

// Delete file metadata
int delete_file_info(const char *filename);

#endif
