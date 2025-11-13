/* ACCESS CONTROL LISTS (ACL) - Person 1, Day 10-11
 * Manages file permissions: owner, read access, write access
 * Each file has an ACL with owner and list of users with specific rights
 * GRANT command adds permissions (READ or WRITE) to users
 * REVOKE command removes user access
 * Only file owner can GRANT or REVOKE permissions
 * Persisted to disk in name_server/data/acl.dat
 */

#include "ns_acl.h"
#include <stdio.h>
#include <string.h>

static FileACL acl_db[MAX_FILES];
static int acl_count = 0;

#define ACL_FILE "name_server/data/acl.dat"

/* INTERNAL HELPERS - Find ACL entry for a specific file */

static FileACL *find_acl(const char *filename)
{
    for (int i = 0; i < acl_count; i++)
    {
        if (strcmp(acl_db[i].filename, filename) == 0)
            return &acl_db[i];
    }
    return NULL;
}

/* PUBLIC API - Get ACL for a file */
FileACL *acl_get(const char *filename)
{
    return find_acl(filename);
}

/* LOAD AND SAVE ACL DATABASE - Persistence for ACLs across NS restarts */

void acl_load_database()
{
    FILE *f = fopen(ACL_FILE, "rb");
    if (!f)
    {
        printf("NS: No ACL DB found. Starting fresh.\n");
        return;
    }
    fread(&acl_count, sizeof(int), 1, f);
    fread(acl_db, sizeof(FileACL), acl_count, f);
    fclose(f);
}

void acl_save_database()
{
    FILE *f = fopen(ACL_FILE, "wb");
    if (!f)
    {
        perror("Failed to save ACL DB");
        return;
    }
    fwrite(&acl_count, sizeof(int), 1, f);
    fwrite(acl_db, sizeof(FileACL), acl_count, f);
    fclose(f);
}

/* CREATE AND DELETE FILE ACLs - Called when files are created or deleted */

void acl_create_file(const char *filename, const char *owner)
{
    FileACL *fa = &acl_db[acl_count++];
    strcpy(fa->filename, filename);
    strcpy(fa->owner, owner);
    fa->entry_count = 1;

    // Owner always gets RW
    strcpy(fa->entries[0].username, owner);
    fa->entries[0].rights = ACCESS_READ | ACCESS_WRITE;

    acl_save_database();
}

void acl_delete_file(const char *filename)
{
    for (int i = 0; i < acl_count; i++)
    {
        if (strcmp(acl_db[i].filename, filename) == 0)
        {
            // shift left
            for (int j = i; j < acl_count - 1; j++)
                acl_db[j] = acl_db[j + 1];

            acl_count--;
            acl_save_database();
            return;
        }
    }
}

/* PERMISSION CHECKING - Verify if user has read or write access to file */

bool acl_can_read(const char *filename, const char *user)
{
    FileACL *fa = find_acl(filename);
    if (!fa)
        return false;

    for (int i = 0; i < fa->entry_count; i++)
    {
        if (strcmp(fa->entries[i].username, user) == 0)
            return (fa->entries[i].rights & ACCESS_READ) != 0;
    }
    return false;
}

bool acl_can_write(const char *filename, const char *user)
{
    FileACL *fa = find_acl(filename);
    if (!fa)
        return false;

    for (int i = 0; i < fa->entry_count; i++)
    {
        if (strcmp(fa->entries[i].username, user) == 0)
            return (fa->entries[i].rights & ACCESS_WRITE) != 0;
    }
    return false;
}

/* GRANT PERMISSIONS - Add read or write access for users (GRANT command) */

bool acl_add_read(const char *filename, const char *user)
{
    FileACL *fa = find_acl(filename);
    if (!fa)
        return false;

    // If already present
    for (int i = 0; i < fa->entry_count; i++)
    {
        if (strcmp(fa->entries[i].username, user) == 0)
        {
            fa->entries[i].rights |= ACCESS_READ;
            acl_save_database();
            return true;
        }
    }

    // Add new entry
    strcpy(fa->entries[fa->entry_count].username, user);
    fa->entries[fa->entry_count].rights = ACCESS_READ;
    fa->entry_count++;

    acl_save_database();
    return true;
}

bool acl_add_write(const char *filename, const char *user)
{
    FileACL *fa = find_acl(filename);
    if (!fa)
        return false;

    // If exists
    for (int i = 0; i < fa->entry_count; i++)
    {
        if (strcmp(fa->entries[i].username, user) == 0)
        {
            fa->entries[i].rights |= (ACCESS_READ | ACCESS_WRITE);
            acl_save_database();
            return true;
        }
    }

    // New entry
    strcpy(fa->entries[fa->entry_count].username, user);
    fa->entries[fa->entry_count].rights = ACCESS_READ | ACCESS_WRITE;
    fa->entry_count++;

    acl_save_database();
    return true;
}

/* REVOKE PERMISSIONS - Remove user access (REVOKE command)
 * Owner cannot be removed from ACL
 */
bool acl_remove_access(const char *filename, const char *user)
{
    FileACL *fa = find_acl(filename);
    if (!fa)
        return false;

    // owner cannot be removed
    if (strcmp(fa->owner, user) == 0)
        return false;

    for (int i = 0; i < fa->entry_count; i++)
    {
        if (strcmp(fa->entries[i].username, user) == 0)
        {
            for (int j = i; j < fa->entry_count - 1; j++)
                fa->entries[j] = fa->entries[j + 1];

            fa->entry_count--;
            acl_save_database();
            return true;
        }
    }
    return false;
}
