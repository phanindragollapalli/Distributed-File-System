# ================================================
# Docs++ Project - Top-Level Makefile
# ================================================

# Project directories
COMMON_DIR = common
NM_DIR = name_server
SS_DIR = storage_server
CLIENT_DIR = client
TEST_DIR = tests
SCRIPT_DIR = scripts

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

# Executable outputs
NM_BIN = $(NM_DIR)/nm_server
SS_BIN = $(SS_DIR)/ss_server
CLIENT_BIN = $(CLIENT_DIR)/client_app

# Build targets
.PHONY: all common name_server storage_server client clean run start test

# ----------------------------------------
# Default: build everything
# ----------------------------------------
all: common name_server storage_server client
	@echo "✅ Build complete for Docs++!"

# ----------------------------------------
# Common (shared code)
# ----------------------------------------
common:
	@echo "🔧 Building common utilities..."
	@$(MAKE) -C $(COMMON_DIR)

# ----------------------------------------
# Name Server
# ----------------------------------------
name_server:
	@echo "🏗️  Building Name Server..."
	@$(MAKE) -C $(NM_DIR)

# ----------------------------------------
# Storage Server
# ----------------------------------------
storage_server:
	@echo "🧱 Building Storage Server..."
	@$(MAKE) -C $(SS_DIR)

# ----------------------------------------
# Client
# ----------------------------------------
client:
	@echo "💻 Building Client..."
	@$(MAKE) -C $(CLIENT_DIR)

# ----------------------------------------
# Run the system (example setup)
# ----------------------------------------
run: all
	@echo "🚀 Starting Docs++ System..."
	@bash $(SCRIPT_DIR)/start_system.sh

# ----------------------------------------
# Clean everything
# ----------------------------------------
clean:
	@echo "🧹 Cleaning all build files..."
	@$(MAKE) -C $(COMMON_DIR) clean
	@$(MAKE) -C $(NM_DIR) clean
	@$(MAKE) -C $(SS_DIR) clean
	@$(MAKE) -C $(CLIENT_DIR) clean
	@rm -f $(NM_BIN) $(SS_BIN) $(CLIENT_BIN)
	@echo "🧼 Clean complete!"

# ----------------------------------------
# Run all tests
# ----------------------------------------
test: all
	@echo "🧪 Running tests..."
	@bash $(TEST_DIR)/test_basic.sh
	@bash $(TEST_DIR)/test_concurrency.sh
	@bash $(TEST_DIR)/test_acl.sh
	@bash $(TEST_DIR)/test_persistence.sh
	@bash $(TEST_DIR)/test_stream.sh
	@echo "✅ All tests executed."
