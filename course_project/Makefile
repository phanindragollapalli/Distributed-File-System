
.PHONY: all common client name_server storage_server clean distclean

SUBDIRS := common client name_server storage_server

# BIN_DIR relative to project root. Compute absolute path and pass to sub-makes
BIN_DIR := bin
LOGS_DIR := logs
ABS_BIN_DIR := $(abspath $(BIN_DIR))

# Build common first, then the servers/clients which link against it
all: common client name_server storage_server | $(BIN_DIR) $(LOGS_DIR)

common:
	$(MAKE) -C common

client: common
	$(MAKE) -C client BIN_DIR=$(ABS_BIN_DIR)

name_server: common
	$(MAKE) -C name_server BIN_DIR=$(ABS_BIN_DIR)

storage_server: common
	$(MAKE) -C storage_server BIN_DIR=$(ABS_BIN_DIR)


$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(LOGS_DIR):
	mkdir -p $(LOGS_DIR)

# Clean build artifacts (bin/, *.o)
clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean BIN_DIR=$(ABS_BIN_DIR); done
	rm -rf $(BIN_DIR)

# Clean everything (build artifacts + logs)
distclean: clean
	rm -rf logs/
	rm -rf name_server/logs/ name_server/data/
	rm -rf storage_server/logs/ storage_server/data/
	rm -rf storage_server/storage/files/*

