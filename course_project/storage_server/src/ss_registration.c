/*
 * STORAGE SERVER REGISTRATION
 * Handles registration of Storage Server with Name Server.
 * Sends server metadata and scans local directory to report available files.
 */

#define _DEFAULT_SOURCE
#include "../include/ss_registration.h"
#include "../../common/include/protocol.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Register Storage Server with Name Server
int register_with_ns(int ns_fd, const char *ss_ip, int ss_nm_port, int ss_client_port)
{
    char msg[512];
    snprintf(msg, sizeof(msg), "REGISTER_SS %s %d %d\n", ss_ip, ss_nm_port, ss_client_port);
    write(ns_fd, msg, strlen(msg));
    // Don't wait for ACK here - NS sends ACK after receiving file list
    return 1;
}

// send_file_list_to_ns: sends all current filenames to NS
int send_file_list_to_ns(int ns_fd, const char *dir)
{
    DIR *dp = opendir(dir);
    if (!dp)
        return -1;
    struct dirent *ep;
    while ((ep = readdir(dp)) != NULL)
    {
        if (ep->d_type == DT_REG)
        {
            char msg[512];
            snprintf(msg, sizeof(msg), "FILE %s\n", ep->d_name);
            write(ns_fd, msg, strlen(msg));
        }
    }
    closedir(dp);
    // End of list indicator
    write(ns_fd, "END_FILE_LIST\n", 14);
    return 0;
}