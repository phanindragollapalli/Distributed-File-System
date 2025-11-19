#ifndef NS_METADATA_H
#define NS_METADATA_H

#include <time.h>

typedef struct FileMetadata
{
    char filename[256];
    char owner[32];
    time_t created;
    time_t modified;
    time_t last_accessed;
    char last_accessed_by[32];
    size_t size;
    int storage_server_id;
    char storage_location[64];
} FileMetadata;

// Initialize metadata storage
void init_metadata_storage();

// Add file metadata
int add_file_metadata(const char *filename, const char *owner, int ss_id);

// Get file metadata
FileMetadata *get_file_metadata(const char *filename);

// Update file metadata
int update_file_metadata(const char *filename, size_t new_size);

// Update access time and user
int update_access_time(const char *filename, const char *username);

// Delete file metadata
int delete_file_metadata(const char *filename);

// List all files
int list_all_files(char files[][256], int max_count);

// Get file count
int get_file_count();

/* ========== PERSISTENCE FUNCTIONS ========== */

// Save all metadata to disk
int save_metadata_to_disk();

// Load metadata from disk on startup
int load_metadata_from_disk();

#endif
