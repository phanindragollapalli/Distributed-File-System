#ifndef NS_ACL_H
#define NS_ACL_H

// ACL (Access Control List) System - To be fully implemented in Week 2 (Days 10-11)

typedef enum
{
    PERM_NONE = 0,
    PERM_READ = 1,
    PERM_WRITE = 2,
    PERM_READ_WRITE = 3
} Permission;

typedef struct ACLEntry
{
    char username[32];
    Permission perm;
} ACLEntry;

typedef struct FileACL
{
    char filename[256];
    char owner[32];
    ACLEntry entries[32];
    int entry_count;
} FileACL;

// Initialize ACL system
void init_acl_system();

// Add permission for user on file
int add_permission(const char *filename, const char *username, Permission perm);

// Remove all permissions for user on file
int remove_permission(const char *filename, const char *username);

// Check if user has permission on file
int check_permission(const char *filename, const char *username, Permission perm);

// Get file ACL
FileACL *get_file_acl(const char *filename);

#endif
