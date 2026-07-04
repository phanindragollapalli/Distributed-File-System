CC := gcc
CFLAGS := -g -Wall -Iinclude
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))
LIB := libcommon.a

.PHONY: all clean

all: $(LIB)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

clean:
	rm -rf build $(LIB)

