#include "../include/ns_acl.h"
#include "../../common/include/logger.h"
#include <string.h>
#include <stdlib.h>

extern Logger *global_logger;

static FileACL acl_list[512];
static int acl_count = 0;

// Initialize ACL system
void init_acl_system()
{
    acl_count = 0;
    memset(acl_list, 0, sizeof(acl_list));
    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "Initialized ACL system");
    }
}

// Add permission for user on file
int add_permission(const char *filename, const char *username, Permission perm)
{
    // To be fully implemented in Week 2 (Days 10-11)
    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "ACL: add_permission called (stub) - %s for %s",
                    filename, username);
    }
    return 0;
}

// Remove all permissions for user on file
int remove_permission(const char *filename, const char *username)
{
    // To be fully implemented in Week 2 (Days 10-11)
    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "ACL: remove_permission called (stub) - %s for %s",
                    filename, username);
    }
    return 0;
}

// Check if user has permission on file
int check_permission(const char *filename, const char *username, Permission perm)
{
    // To be fully implemented in Week 2 (Days 10-11)
    // For now, allow all operations
    return 1;
}

// Get file ACL
FileACL *get_file_acl(const char *filename)
{
    // To be fully implemented in Week 2 (Days 10-11)
    return NULL;
}
