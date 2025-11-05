CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude -g
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
LIB = libcommon.a

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^

clean:
	rm -f $(OBJ) $(LIB)
