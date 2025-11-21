/*
 * STORAGE SERVER AND CLIENT REGISTRATION
 *
 * Manages registration of Storage Servers and Clients with the Name Server.
 * Maintains lists of connected Storage Servers and Clients with their metadata.
 */

#include "../include/ns_registration.h"
#include <string.h>
#include <stdio.h>

// Storage Server registry (max 16 Storage Servers)
static StorageServerInfo ss_list[16];
static int ss_count = 0;

static ClientInfo client_list[32];
static int client_count = 0;

static int file_matches(const char *a, const char *b)
{
    return a && b && strncmp(a, b, 128) == 0;
}

int register_storage_server(const char *ip, int nm_port, int client_port,
                            const char file_list[][128], int file_count, int ns_fd)
{
    StorageServerInfo *ss = &ss_list[ss_count];
    ss->ss_id = ss_count;
    strncpy(ss->ss_ip, ip, 32);
    ss->ss_nm_port = nm_port;
    ss->ss_client_port = client_port;
    ss->file_count = file_count;
    ss->ns_fd = ns_fd;
    for (int i = 0; i < file_count; ++i)
        strncpy(ss->files[i], file_list[i], 128);
    pthread_mutex_init(&ss->command_lock, NULL);
    ss->is_connected = 1;
    ss->monitor_thread = 0;
    ++ss_count;
    printf("Registered SS: %s:%d (client_port %d); %d files\n", ip, nm_port, client_port, file_count);
    return ss_count - 1; // Return ID
}

int register_storage_server_with_id(int ss_id, const char *ip, int nm_port, int client_port,
                            const char file_list[][128], int file_count, int ns_fd)
{
    // Check if SS with this ID already exists AND was previously initialized
    if (ss_id >= 0 && ss_id < ss_count)
    {
        StorageServerInfo *ss = &ss_list[ss_id];
        
        // Only treat as reconnection if this SS was actually used before
        // Check if it has a valid ss_id set (meaning it was initialized)
        if (ss->ss_id == ss_id && ss->ss_ip[0] != '\0')
        {
            // Reconnecting storage server - update connection info
            strncpy(ss->ss_ip, ip, 32);
            ss->ss_nm_port = nm_port;
            ss->ss_client_port = client_port;
            ss->ns_fd = ns_fd;
            ss->is_connected = 1;
            
            // Merge file lists (add new files, keep existing)
            for (int i = 0; i < file_count; ++i)
            {
                int found = 0;
                for (int j = 0; j < ss->file_count; ++j)
                {
                    if (file_matches(ss->files[j], file_list[i]))
                    {
                        found = 1;
                        break;
                    }
                }
                if (!found && ss->file_count < 128)
                {
                    strncpy(ss->files[ss->file_count], file_list[i], 128);
                    ss->file_count++;
                }
            }
            
            printf("Reconnected SS %d: %s:%d (client_port %d); %d files\n", ss_id, ip, nm_port, client_port, ss->file_count);
            return ss_id;
        }
        // Otherwise, fall through to initialize this slot as a new SS
    }
    
    // New storage server - allocate at the requested ID
    if (ss_id >= 0 && ss_id < 16)
    {
        // Ensure we have space
        if (ss_id >= ss_count)
        {
            ss_count = ss_id + 1;
        }
        
        StorageServerInfo *ss = &ss_list[ss_id];
        ss->ss_id = ss_id;
        strncpy(ss->ss_ip, ip, 32);
        ss->ss_nm_port = nm_port;
        ss->ss_client_port = client_port;
        ss->file_count = file_count;
        ss->ns_fd = ns_fd;
        for (int i = 0; i < file_count; ++i)
            strncpy(ss->files[i], file_list[i], 128);
        pthread_mutex_init(&ss->command_lock, NULL);
        ss->is_connected = 1;
        ss->monitor_thread = 0;
        
        printf("Registered SS %d: %s:%d (client_port %d); %d files\n", ss_id, ip, nm_port, client_port, file_count);
        return ss_id;
    }
    
    // Fallback: use auto-increment ID
    return register_storage_server(ip, nm_port, client_port, file_list, file_count, ns_fd);
}

int register_client(const char *username, const char *ip, int client_port)
{
    ClientInfo *cli = &client_list[client_count];
    strncpy(cli->username, username, 32);
    strncpy(cli->ip, ip, 32);
    cli->client_port = client_port;
    ++client_count;
    printf("Registered client: %s (%s:%d)\n", username, ip, client_port);
    return client_count - 1;
}

// Get storage server by ID
StorageServerInfo *get_ss_by_id(int ss_id)
{
    if (ss_id < 0 || ss_id >= ss_count)
    {
        return NULL;
    }
    
    // Check if this slot was actually initialized (has a valid IP)
    StorageServerInfo *ss = &ss_list[ss_id];
    if (ss->ss_ip[0] == '\0')
    {
        return NULL;  // Slot not initialized
    }
    
    return ss;
}

// Get client by ID
ClientInfo *get_client_by_id(int client_id)
{
    if (client_id < 0 || client_id >= client_count)
    {
        return NULL;
    }
    return &client_list[client_id];
}

// Get storage server count
int get_ss_count()
{
    return ss_count;
}

int get_client_count()
{
    return client_count;
}

void ns_register_add_file(int ss_id, const char *filename)
{
    if (ss_id < 0 || ss_id >= ss_count || !filename)
    {
        return;
    }

    StorageServerInfo *ss = &ss_list[ss_id];

    for (int i = 0; i < ss->file_count; ++i)
    {
        if (file_matches(ss->files[i], filename))
        {
            return;
        }
    }

    if (ss->file_count >= 128)
    {
        return;
    }

    strncpy(ss->files[ss->file_count], filename, 128);
    ss->files[ss->file_count][127] = '\0';
    ss->file_count++;
}

void ns_register_remove_file(int ss_id, const char *filename)
{
    if (ss_id < 0 || ss_id >= ss_count || !filename)
    {
        return;
    }

    StorageServerInfo *ss = &ss_list[ss_id];

    for (int i = 0; i < ss->file_count; ++i)
    {
        if (file_matches(ss->files[i], filename))
        {
            for (int j = i; j < ss->file_count - 1; ++j)
            {
                strncpy(ss->files[j], ss->files[j + 1], 128);
            }
            if (ss->file_count > 0)
            {
                ss->file_count--;
                ss->files[ss->file_count][0] = '\0';
            }
            return;
        }
    }
}

int get_ss_ids_for_file(const char *filename, int *out_ids, int max_ids)
{
    if (!filename || !out_ids || max_ids <= 0)
    {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < ss_count && count < max_ids; ++i)
    {
        for (int j = 0; j < ss_list[i].file_count; ++j)
        {
            if (file_matches(ss_list[i].files[j], filename))
            {
                out_ids[count++] = ss_list[i].ss_id;
                break;
            }
        }
    }

    return count;
}

int list_registered_users(char users[][32], int max_users)
{
    int unique_count = 0;

    // Deduplicate usernames
    for (int i = 0; i < client_count && unique_count < max_users; ++i)
    {
        const char *current_username = client_list[i].username;

        // Check if this username already exists in the output list
        int already_added = 0;
        for (int j = 0; j < unique_count; ++j)
        {
            if (strcmp(users[j], current_username) == 0)
            {
                already_added = 1;
                break;
            }
        }

        // Only add if it's a new unique username
        if (!already_added)
        {
            strncpy(users[unique_count], current_username, 32);
            users[unique_count][31] = '\0';
            unique_count++;
        }
    }

    return unique_count;
}
