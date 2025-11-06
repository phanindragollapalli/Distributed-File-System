#ifndef SS_REGISTRATION_H
#define SS_REGISTRATION_H

int register_with_ns(int ns_fd, const char *ss_ip, int ss_nm_port, int ss_client_port);
int send_file_list_to_ns(int ns_fd, const char *dir);

#endif
