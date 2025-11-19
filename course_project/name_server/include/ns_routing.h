#ifndef NS_ROUTING_H
#define NS_ROUTING_H

#include "ns_storage.h"
#include "ns_registration.h"

// Route a file request to the appropriate storage server
int route_file_request(const char *filename, int *ss_id_out);

// Get storage server info for a file
StorageServerInfo *get_ss_for_file(const char *filename);

// Send storage server info to client
int send_ss_info_to_client(int client_fd, StorageServerInfo *ss_info);

// Send storage server info along with alternate replicas for the same file
int send_ss_info_with_fallbacks(int client_fd, StorageServerInfo *primary_ss, const char *filename);

#endif
