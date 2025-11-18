#ifndef NS_EXEC_H
#define NS_EXEC_H

#include <stddef.h>
#include "ns_registration.h"

// Execute file from Storage Server and capture output
int execute_file_from_ss(StorageServerInfo *ss_info, const char *filename,
                         char *output, size_t output_size, int ss_id);

// Legacy function (deprecated)
int execute_file(const char *filename, char *output, size_t output_size);

#endif
