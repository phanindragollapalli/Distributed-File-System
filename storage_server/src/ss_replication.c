/* STORAGE SERVER REPLICATION
 * Handles file replication across multiple storage servers
 * Ensures all file operations are propagated to all connected storage servers
 */

#include "../include/ss_replication.h"
#include "../include/file_operations.h"
#include "../../common/include/logger.h"
#include "../../common/include/error_codes.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>

extern Logger *ss_logger;

// Get list of peer storage servers from name server
int get_peer_storage_servers(FileOperationContext *ctx, int **peer_ids, char ***peer_ips, 
                              int **peer_ports, int *peer_count)
{
    if (!ctx || !peer_ids || !peer_ips || !peer_ports || !peer_count)
    {
        return ERR_NULL_POINTER;
    }
    
    // Connect to name server
    int ns_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ns_fd < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create socket for NS connection");
        return ERR_SOCKET_CREATE;
    }
    
    struct sockaddr_in ns_addr = {0};
    ns_addr.sin_family = AF_INET;
    ns_addr.sin_port = htons(ctx->ns_port);
    
    if (inet_pton(AF_INET, ctx->ns_ip, &ns_addr.sin_addr) <= 0)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid NS IP address: %s", ctx->ns_ip);
        close(ns_fd);
        return ERR_INVALID_FORMAT;
    }
    
    if (connect(ns_fd, (struct sockaddr *)&ns_addr, sizeof(ns_addr)) < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to connect to NS for peer list");
        close(ns_fd);
        return ERR_CONNECTION_CLOSED;
    }
    
    // Request list of all storage servers
    char request[256];
    snprintf(request, sizeof(request), "GET_PEER_SS %d\n", ctx->ss_id);
    write(ns_fd, request, strlen(request));
    
    // Read response
    char buffer[4096];
    int bytes_read = read(ns_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to read peer SS list from NS");
        close(ns_fd);
        return ERR_RECV_FAILED;
    }
    buffer[bytes_read] = '\0';
    
    close(ns_fd);
    
    // Parse response: format "PEER_SS <count>\n<id> <ip> <port>\n..."
    int count = 0;
    if (sscanf(buffer, "PEER_SS %d\n", &count) != 1)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid peer SS response format");
        return ERR_INVALID_FORMAT;
    }
    
    if (count == 0)
    {
        *peer_count = 0;
        return SUCCESS;
    }
    
    // Allocate arrays
    *peer_ids = malloc(count * sizeof(int));
    *peer_ips = malloc(count * sizeof(char *));
    *peer_ports = malloc(count * sizeof(int));
    
    if (!*peer_ids || !*peer_ips || !*peer_ports)
    {
        free(*peer_ids);
        free(*peer_ips);
        free(*peer_ports);
        return ERR_MEMORY;
    }
    
    // Parse each peer
    char *line = buffer;
    char *next_line;
    int parsed = 0;
    
    // Skip first line
    next_line = strchr(line, '\n');
    if (next_line)
    {
        line = next_line + 1;
    }
    
    while (parsed < count && line && *line)
    {
        (*peer_ips)[parsed] = malloc(32);
        if (sscanf(line, "%d %31s %d", &(*peer_ids)[parsed], (*peer_ips)[parsed], &(*peer_ports)[parsed]) == 3)
        {
            parsed++;
        }
        
        next_line = strchr(line, '\n');
        if (!next_line)
        {
            break;
        }
        line = next_line + 1;
    }
    
    *peer_count = parsed;
    return SUCCESS;
}

// Send file content to a specific storage server
int send_file_to_storage_server(const char *filepath, const char *ss_ip, int ss_port, const char *filename)
{
    if (!filepath || !ss_ip || !filename)
    {
        return ERR_NULL_POINTER;
    }
    
    // Open file
    FILE *file = fopen(filepath, "r");
    if (!file)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to open file for replication: %s", filepath);
        return ERR_FILE_READ;
    }
    
    // Connect to peer storage server
    int ss_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_fd < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create socket for peer SS connection");
        fclose(file);
        return ERR_SOCKET_CREATE;
    }
    
    struct sockaddr_in ss_addr = {0};
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);
    
    if (inet_pton(AF_INET, ss_ip, &ss_addr.sin_addr) <= 0)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid peer SS IP address: %s", ss_ip);
        close(ss_fd);
        fclose(file);
        return ERR_INVALID_FORMAT;
    }
    
    if (connect(ss_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        log_message(ss_logger, LOG_WARN, "Failed to connect to peer SS %s:%d for replication", ss_ip, ss_port);
        close(ss_fd);
        fclose(file);
        return ERR_CONNECTION_CLOSED;
    }
    
    // Send REPLICATE_CREATE command
    char command[512];
    snprintf(command, sizeof(command), "REPLICATE_CREATE %s\n", filename);
    write(ss_fd, command, strlen(command));
    
    // Send file content
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        if (write(ss_fd, buffer, bytes) < 0)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to send file content to peer SS");
            close(ss_fd);
            fclose(file);
            return ERR_SEND_FAILED;
        }
    }
    
    // Send END marker
    write(ss_fd, "END_FILE\n", 9);
    
    // Read ACK
    char ack[64];
    int ack_bytes = read(ss_fd, ack, sizeof(ack) - 1);
    if (ack_bytes > 0)
    {
        ack[ack_bytes] = '\0';
        if (strncmp(ack, "ACK", 3) != 0)
        {
            log_message(ss_logger, LOG_WARN, "Peer SS did not acknowledge replication");
        }
    }
    
    close(ss_fd);
    fclose(file);
    
    log_message(ss_logger, LOG_INFO, "Replicated file %s to peer SS %s:%d", filename, ss_ip, ss_port);
    return SUCCESS;
}

// Replicate file creation to all other storage servers
int replicate_create_to_peers(FileOperationContext *ctx, const char *filepath)
{
    if (!ctx || !filepath)
    {
        return ERR_NULL_POINTER;
    }
    
    // Get full path
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", ctx->storage_path, filepath);
    
    // Get peer storage servers
    int *peer_ids = NULL;
    char **peer_ips = NULL;
    int *peer_ports = NULL;
    int peer_count = 0;
    
    int result = get_peer_storage_servers(ctx, &peer_ids, &peer_ips, &peer_ports, &peer_count);
    if (result != SUCCESS || peer_count == 0)
    {
        // No peers or error, but not critical
        return SUCCESS;
    }
    
    // Replicate to each peer
    for (int i = 0; i < peer_count; i++)
    {
        send_file_to_storage_server(full_path, peer_ips[i], peer_ports[i], filepath);
        // Continue even if some replications fail
    }
    
    // Cleanup
    for (int i = 0; i < peer_count; i++)
    {
        free(peer_ips[i]);
    }
    free(peer_ids);
    free(peer_ips);
    free(peer_ports);
    
    return SUCCESS;
}

// Replicate file write to all other storage servers
int replicate_write_to_peers(FileOperationContext *ctx, const char *filepath)
{
    // Similar to replicate_create_to_peers but sends full updated file content
    return replicate_create_to_peers(ctx, filepath);
}

// Replicate file deletion to all other storage servers
int replicate_delete_to_peers(FileOperationContext *ctx, const char *filepath)
{
    if (!ctx || !filepath)
    {
        return ERR_NULL_POINTER;
    }
    
    // Get peer storage servers
    int *peer_ids = NULL;
    char **peer_ips = NULL;
    int *peer_ports = NULL;
    int peer_count = 0;
    
    int result = get_peer_storage_servers(ctx, &peer_ids, &peer_ips, &peer_ports, &peer_count);
    if (result != SUCCESS || peer_count == 0)
    {
        return SUCCESS;
    }
    
    // Send delete command to each peer
    for (int i = 0; i < peer_count; i++)
    {
        int ss_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (ss_fd < 0)
        {
            continue;
        }
        
        struct sockaddr_in ss_addr = {0};
        ss_addr.sin_family = AF_INET;
        ss_addr.sin_port = htons(peer_ports[i]);
        inet_pton(AF_INET, peer_ips[i], &ss_addr.sin_addr);
        
        if (connect(ss_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
        {
            close(ss_fd);
            continue;
        }
        
        // Send REPLICATE_DELETE command
        char command[512];
        snprintf(command, sizeof(command), "REPLICATE_DELETE %s\n", filepath);
        write(ss_fd, command, strlen(command));
        
        close(ss_fd);
    }
    
    // Cleanup
    for (int i = 0; i < peer_count; i++)
    {
        free(peer_ips[i]);
    }
    free(peer_ids);
    free(peer_ips);
    free(peer_ports);
    
    return SUCCESS;
}

// Copy file from a peer storage server
int copy_file_from_peer(const char *filepath, const char *ss_ip, int ss_port, const char *filename, int source_ss_id)
{
    if (!filepath || !ss_ip || !filename)
    {
        return ERR_NULL_POINTER;
    }
    
    (void)source_ss_id; // Unused for now
    
    // Connect to peer storage server
    int ss_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_fd < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create socket for peer connection");
        return ERR_SOCKET_CREATE;
    }
    
    struct sockaddr_in ss_addr = {0};
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);
    
    if (inet_pton(AF_INET, ss_ip, &ss_addr.sin_addr) <= 0)
    {
        log_message(ss_logger, LOG_ERROR, "Invalid peer SS IP: %s", ss_ip);
        close(ss_fd);
        return ERR_INVALID_FORMAT;
    }
    
    if (connect(ss_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to connect to peer SS %s:%d", ss_ip, ss_port);
        close(ss_fd);
        return ERR_CONNECTION_CLOSED;
    }
    
    // Request file content
    char request[512];
    snprintf(request, sizeof(request), "READ %s\n", filename);
    write(ss_fd, request, strlen(request));
    
    // Create destination file
    FILE *dest_file = fopen(filepath, "w");
    if (!dest_file)
    {
        log_message(ss_logger, LOG_ERROR, "Failed to create file: %s", filepath);
        close(ss_fd);
        return ERR_FILE_WRITE;
    }
    
    // Read and write content
    char buffer[4096];
    int bytes;
    while ((bytes = read(ss_fd, buffer, sizeof(buffer))) > 0)
    {
        if (fwrite(buffer, 1, bytes, dest_file) != (size_t)bytes)
        {
            log_message(ss_logger, LOG_ERROR, "Failed to write to file: %s", filepath);
            fclose(dest_file);
            close(ss_fd);
            return ERR_FILE_WRITE;
        }
    }
    
    fclose(dest_file);
    close(ss_fd);
    
    log_message(ss_logger, LOG_INFO, "Copied file %s from peer SS %s:%d", filename, ss_ip, ss_port);
    return SUCCESS;
}
