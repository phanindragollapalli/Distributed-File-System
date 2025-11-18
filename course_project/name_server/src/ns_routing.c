/*
 * FILE REQUEST ROUTING
 *
 * Routes file requests to appropriate Storage Servers using Trie lookups.
 * Implements load balancing and failover for multiple Storage Servers.
 */

#include "../include/ns_routing.h"
#include "../include/ns_storage.h"
#include "../include/ns_registration.h"
#include "../../common/include/logger.h"
#include "../include/ns_acl.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

extern Logger *global_logger;
extern TrieNode *global_file_trie;
extern StorageServerInfo *get_ss_by_id(int ss_id);

// Route a file request to the appropriate Storage Server
int route_file_request(const char *filename, int *ss_id_out)
{
    if (!filename || !global_file_trie)
    {
        return -1;
    }

    int ss_id;
    if (trie_search(global_file_trie, filename, &ss_id))
    {
        if (ss_id_out)
        {
            *ss_id_out = ss_id;
        }
        log_message(global_logger, LOG_DEBUG, "Routed file '%s' to SS_ID=%d", filename, ss_id);
        return 0;
    }

    log_message(global_logger, LOG_WARN, "File '%s' not found in routing table", filename);
    return -1;
}

// Get storage server info for a file
StorageServerInfo *get_ss_for_file(const char *filename)
{
    int ss_id;
    if (route_file_request(filename, &ss_id) == 0)
    {
        return get_ss_by_id(ss_id);
    }
    return NULL;
}

// Send storage server info to client
int send_ss_info_to_client(int client_fd, StorageServerInfo *ss_info)
{
    if (!ss_info)
    {
        const char *error = "ERROR: Storage server not found\n";
        write(client_fd, error, strlen(error));
        return -1;
    }

    char response[256];
    snprintf(response, sizeof(response), "SS_INFO %s %d\n",
             ss_info->ss_ip, ss_info->ss_client_port);

    if (write(client_fd, response, strlen(response)) < 0)
    {
        log_message(global_logger, LOG_ERROR, "Failed to send SS info to client");
        return -1;
    }

    log_message(global_logger, LOG_INFO, "Sent SS info to client: %s:%d",
                ss_info->ss_ip, ss_info->ss_client_port);
    return 0;
}

void handle_access_request(int client_sock, char *msg, const char *client_user)
{
    char op[32], flag[8], filename[128], target_user[64];

    int parts = sscanf(msg, "%[^:]:%[^:]:%[^:]:%s", op, flag, filename, target_user);
    if (parts < 3)
    {
        send(client_sock, "ERR:INVALID_FORMAT", 18, 0);
        return;
    }

    // Permission check
    FileACL *fa = acl_get(filename);
    if (!fa)
    {
        send(client_sock, "ERR:FILE_NOT_FOUND", 18, 0);
        return;
    }

    if (strcmp(fa->owner, client_user) != 0)
    {
        send(client_sock, "ERR:NOT_OWNER", 13, 0);
        return;
    }

    // ---------------------
    // ADDACCESS -R / -W
    // ---------------------
    if (strcmp(op, "ADDACCESS") == 0)
    {
        if (strcmp(flag, "-R") == 0)
        {
            acl_add_read(filename, target_user);
            send(client_sock, "OK:READ_GRANTED", 15, 0);
            return;
        }
        if (strcmp(flag, "-W") == 0)
        {
            acl_add_write(filename, target_user);
            send(client_sock, "OK:WRITE_GRANTED", 16, 0);
            return;
        }
        send(client_sock, "ERR:INVALID_FLAG", 16, 0);
        return;
    }

    // ---------------------
    // REMOVE ACCESS
    // ---------------------
    if (strcmp(op, "REMACCESS") == 0)
    {
        if (!acl_remove_access(filename, target_user))
        {
            send(client_sock, "ERR:REMOVE_FAILED", 17, 0);
            return;
        }
        send(client_sock, "OK:ACCESS_REMOVED", 17, 0);
        return;
    }

    send(client_sock, "ERR:UNKNOWN_ACCESS_OP", 21, 0);
}