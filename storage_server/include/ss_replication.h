#ifndef SS_REPLICATION_H
#define SS_REPLICATION_H

#include "file_operations.h"

// Replicate file creation to all other storage servers
int replicate_create_to_peers(FileOperationContext *ctx, const char *filepath);

// Replicate file write to all other storage servers
int replicate_write_to_peers(FileOperationContext *ctx, const char *filepath);

// Replicate file deletion to all other storage servers
int replicate_delete_to_peers(FileOperationContext *ctx, const char *filepath);

// Get list of peer storage server information from name server
int get_peer_storage_servers(FileOperationContext *ctx, int **peer_ids, char ***peer_ips, 
                              int **peer_ports, int *peer_count);

// Send file content to a specific storage server
int send_file_to_storage_server(const char *filepath, const char *ss_ip, int ss_port, const char *filename);

// Copy file from one storage server to another
int copy_file_from_peer(const char *filepath, const char *ss_ip, int ss_port, const char *filename, int source_ss_id);

#endif
