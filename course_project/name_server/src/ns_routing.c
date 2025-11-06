#include "../include/ns_routing.h"
#include "../include/ns_storage.h"
#include "../include/ns_registration.h"
#include "../../common/include/logger.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern Logger *global_logger;
extern TrieNode *global_file_trie;
extern StorageServerInfo *get_ss_by_id(int ss_id);

// Route a file request to the appropriate storage server
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
