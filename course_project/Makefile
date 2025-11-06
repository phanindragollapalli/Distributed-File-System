
.PHONY: all common client name_server storage_server clean

SUBDIRS := common client name_server storage_server

# BIN_DIR relative to project root. Compute absolute path and pass to sub-makes
BIN_DIR := bin
ABS_BIN_DIR := $(abspath $(BIN_DIR))

# Build common first, then the servers/clients which link against it
all: common client name_server storage_server | $(BIN_DIR)

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

clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean BIN_DIR=$(ABS_BIN_DIR); done
	rm -rf $(BIN_DIR)

