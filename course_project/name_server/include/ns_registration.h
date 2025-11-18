#include <stdio.h>
#include <pthread.h>
#ifndef NS_REGISTRATION_H
#define NS_REGISTRATION_H

typedef struct
{
    char ss_ip[32];
    int ss_nm_port;
    int ss_client_port;
    char files[128][128];
    int file_count;
    int ns_fd;
    pthread_mutex_t command_lock;
} StorageServerInfo;

typedef struct
{
    char username[32];
    char ip[32];
    int client_port;
} ClientInfo;

int register_storage_server(const char *ip, int nm_port, int client_port,
                            const char file_list[][128], int file_count, int ns_fd);
int register_client(const char *username, const char *ip, int client_port);

// Get storage server by ID
StorageServerInfo *get_ss_by_id(int ss_id);

// Get client by ID
ClientInfo *get_client_by_id(int client_id);

// Get storage server count
int get_ss_count();

// Get client count
int get_client_count();

// List all registered clients (returns count)
int list_registered_users(char users[][32], int max_users);

#endif
