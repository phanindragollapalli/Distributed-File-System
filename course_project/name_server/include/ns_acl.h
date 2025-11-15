#ifndef NS_ACL_H
#define NS_ACL_H

#include <stdbool.h>

#define MAX_USERS 64
#define MAX_FILES 1024
#define USERNAME_LEN 64
#define FILENAME_LEN 128

// Access rights mask
#define ACCESS_READ 1
#define ACCESS_WRITE 2

typedef struct
{
    char username[USERNAME_LEN];
    int rights; // bitmask: READ=1, WRITE=2
} ACL_Entry;

typedef struct
{
    char filename[FILENAME_LEN];
    char owner[USERNAME_LEN];
    int entry_count;
    ACL_Entry entries[MAX_USERS];
} FileACL;

// ========== PUBLIC NS API ==========

// Load ACL DB from disk
void acl_load_database();

// Save ACL DB
void acl_save_database();

// Find ACL record by filename
FileACL *acl_get(const char *filename);

// Create ACL record for new file
void acl_create_file(const char *filename, const char *owner);

// Remove ACL record of deleted file
void acl_delete_file(const char *filename);

// Modify ACL
bool acl_add_read(const char *filename, const char *user);
bool acl_add_write(const char *filename, const char *user);
bool acl_remove_access(const char *filename, const char *user);

// Check permissions
bool acl_can_read(const char *filename, const char *user);
bool acl_can_write(const char *filename, const char *user);

#endif
