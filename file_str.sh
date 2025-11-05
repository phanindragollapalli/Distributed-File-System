#!/bin/bash
# setup_project.sh
# Creates the directory and file structure for the Docs++ project

# Base directory
BASE="docs-plus-plus"
mkdir -p "$BASE"

# Top-level files
touch "$BASE/README.md" "$BASE/Makefile" "$BASE/.gitignore"

##########################
# COMMON MODULE
##########################
mkdir -p "$BASE/common/include" "$BASE/common/src"
touch "$BASE/common/include/protocol.h" \
      "$BASE/common/include/error_codes.h" \
      "$BASE/common/include/message.h" \
      "$BASE/common/include/constants.h" \
      "$BASE/common/include/logger.h" \
      "$BASE/common/src/protocol.c" \
      "$BASE/common/src/error_codes.c" \
      "$BASE/common/src/message.c" \
      "$BASE/common/src/logger.c" \
      "$BASE/common/Makefile"

##########################
# NAME SERVER
##########################
mkdir -p "$BASE/name_server/include" "$BASE/name_server/src" "$BASE/name_server/data" "$BASE/name_server/logs"
touch "$BASE/name_server/include/ns_main.h" \
      "$BASE/name_server/include/ns_network.h" \
      "$BASE/name_server/include/ns_storage.h" \
      "$BASE/name_server/include/ns_cache.h" \
      "$BASE/name_server/include/ns_registration.h" \
      "$BASE/name_server/include/ns_routing.h" \
      "$BASE/name_server/include/ns_metadata.h" \
      "$BASE/name_server/include/ns_acl.h" \
      "$BASE/name_server/include/ns_exec.h" \
      "$BASE/name_server/src/ns_main.c" \
      "$BASE/name_server/src/ns_network.c" \
      "$BASE/name_server/src/ns_storage.c" \
      "$BASE/name_server/src/ns_cache.c" \
      "$BASE/name_server/src/ns_registration.c" \
      "$BASE/name_server/src/ns_routing.c" \
      "$BASE/name_server/src/ns_metadata.c" \
      "$BASE/name_server/src/ns_acl.c" \
      "$BASE/name_server/src/ns_exec.c" \
      "$BASE/name_server/data/metadata.dat" \
      "$BASE/name_server/data/acl.dat" \
      "$BASE/name_server/data/users.dat" \
      "$BASE/name_server/logs/ns.log" \
      "$BASE/name_server/Makefile"

##########################
# STORAGE SERVER
##########################
mkdir -p "$BASE/storage_server/include" "$BASE/storage_server/src" \
         "$BASE/storage_server/storage/files" "$BASE/storage_server/data" "$BASE/storage_server/logs"

touch "$BASE/storage_server/include/ss_main.h" \
      "$BASE/storage_server/include/ss_network.h" \
      "$BASE/storage_server/include/ss_registration.h" \
      "$BASE/storage_server/include/ss_commands.h" \
      "$BASE/storage_server/include/ss_persistence.h" \
      "$BASE/storage_server/include/file_structure.h" \
      "$BASE/storage_server/include/file_operations.h" \
      "$BASE/storage_server/include/file_parser.h" \
      "$BASE/storage_server/include/file_locking.h" \
      "$BASE/storage_server/include/file_undo.h" \
      "$BASE/storage_server/include/file_stream.h" \
      "$BASE/storage_server/src/ss_main.c" \
      "$BASE/storage_server/src/ss_network.c" \
      "$BASE/storage_server/src/ss_registration.c" \
      "$BASE/storage_server/src/ss_commands.c" \
      "$BASE/storage_server/src/ss_persistence.c" \
      "$BASE/storage_server/src/file_structure.c" \
      "$BASE/storage_server/src/file_operations.c" \
      "$BASE/storage_server/src/file_parser.c" \
      "$BASE/storage_server/src/file_locking.c" \
      "$BASE/storage_server/src/file_undo.c" \
      "$BASE/storage_server/src/file_stream.c" \
      "$BASE/storage_server/data/metadata.dat" \
      "$BASE/storage_server/data/undo_history.dat" \
      "$BASE/storage_server/logs/ss.log" \
      "$BASE/storage_server/Makefile"

##########################
# CLIENT
##########################
mkdir -p "$BASE/client/include" "$BASE/client/src"
touch "$BASE/client/include/client_main.h" \
      "$BASE/client/include/client_network.h" \
      "$BASE/client/include/client_commands.h" \
      "$BASE/client/include/client_view.h" \
      "$BASE/client/include/client_info.h" \
      "$BASE/client/include/client_list.h" \
      "$BASE/client/include/client_exec.h" \
      "$BASE/client/include/client_access.h" \
      "$BASE/client/include/client_file_ops.h" \
      "$BASE/client/include/client_stream.h" \
      "$BASE/client/src/client_main.c" \
      "$BASE/client/src/client_network.c" \
      "$BASE/client/src/client_commands.c" \
      "$BASE/client/src/client_view.c" \
      "$BASE/client/src/client_info.c" \
      "$BASE/client/src/client_list.c" \
      "$BASE/client/src/client_exec.c" \
      "$BASE/client/src/client_access.c" \
      "$BASE/client/src/client_file_ops.c" \
      "$BASE/client/src/client_stream.c" \
      "$BASE/client/Makefile"

##########################
# TESTS
##########################
mkdir -p "$BASE/tests/unit_tests" "$BASE/tests/integration_tests" "$BASE/tests/test_data"
touch "$BASE/tests/unit_tests/test_protocol.c" \
      "$BASE/tests/unit_tests/test_file_structure.c" \
      "$BASE/tests/unit_tests/test_locking.c" \
      "$BASE/tests/unit_tests/test_parser.c" \
      "$BASE/tests/unit_tests/test_cache.c" \
      "$BASE/tests/integration_tests/test_ns_ss.c" \
      "$BASE/tests/integration_tests/test_client_ns.c" \
      "$BASE/tests/integration_tests/test_end_to_end.c" \
      "$BASE/tests/test_data/sample1.txt" \
      "$BASE/tests/test_data/sample2.txt" \
      "$BASE/tests/test_data/test_commands.txt" \
      "$BASE/tests/Makefile"

##########################
# SCRIPTS
##########################
mkdir -p "$BASE/scripts"
touch "$BASE/scripts/start_ns.sh" \
      "$BASE/scripts/start_ss.sh" \
      "$BASE/scripts/start_client.sh" \
      "$BASE/scripts/cleanup.sh" \
      "$BASE/scripts/run_tests.sh"

##########################
# DOCS & BIN
##########################
mkdir -p "$BASE/docs" "$BASE/bin"
touch "$BASE/docs/ARCHITECTURE.md" \
      "$BASE/docs/PROTOCOL.md" \
      "$BASE/docs/API.md" \
      "$BASE/docs/TESTING.md" \
      "$BASE/bin/name_server" \
      "$BASE/bin/storage_server" \
      "$BASE/bin/client"

echo "✅ Docs++ project structure created successfully!"
