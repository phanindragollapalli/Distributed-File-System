CC := gcc
CFLAGS := -g -Wall -Iinclude -I../common/include
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))
BIN := client
BIN_DIR ?= .
BIN_PATH := $(BIN_DIR)/$(BIN)
LIBS := ../common/libcommon.a

.PHONY: all clean

all: $(BIN_PATH)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

$(BIN_PATH): $(OBJS) $(LIBS) | $(BIN_DIR)
	$(CC) -o $@ $(OBJS) $(LIBS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf build $(BIN_PATH)

