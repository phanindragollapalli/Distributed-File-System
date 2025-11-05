CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude -I../common/include -g
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = client_app

all: $(BIN)

$(BIN): $(OBJ) ../common/libcommon.a
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJ) $(BIN)
