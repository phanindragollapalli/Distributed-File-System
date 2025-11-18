/*
 * STORAGE SERVER AND CLIENT REGISTRATION
 *
 * Manages registration of Storage Servers and Clients with the Name Server.
 * Maintains lists of connected Storage Servers and Clients with their metadata.
 */

#include "ns_registration.h"
#include <string.h>
#include <stdio.h>

// Storage Server registry (max 16 Storage Servers)
static StorageServerInfo ss_list[16];
static int ss_count = 0;

static ClientInfo client_list[32];
static int client_count = 0;

int register_storage_server(const char *ip, int nm_port, int client_port,
                            const char file_list[][128], int file_count, int ns_fd)
{
    StorageServerInfo *ss = &ss_list[ss_count];
    strncpy(ss->ss_ip, ip, 32);
    ss->ss_nm_port = nm_port;
    ss->ss_client_port = client_port;
    ss->file_count = file_count;
    ss->ns_fd = ns_fd;
    for (int i = 0; i < file_count; ++i)
        strncpy(ss->files[i], file_list[i], 128);
    pthread_mutex_init(&ss->command_lock, NULL);
    ++ss_count;
    printf("Registered SS: %s:%d (client_port %d); %d files\n", ip, nm_port, client_port, file_count);
    return ss_count - 1; // Return ID
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
    return &ss_list[ss_id];
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

int list_registered_users(char users[][32], int max_users)
{
    int count = (client_count < max_users) ? client_count : max_users;
    for (int i = 0; i < count; ++i)
    {
        strncpy(users[i], client_list[i].username, 32);
        users[i][31] = '\0';
    }
    return count;
}
