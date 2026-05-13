CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

ENGINE_SRCS = src/server.c \
	src/utils.c \
	src/logger.c \
	src/auth_hook.c \
	src/config.c

all: server client

server: $(ENGINE_SRCS)
	$(CC) $(CFLAGS) -o server $(ENGINE_SRCS) -lpthread

client: src/client.c src/config.c src/utils.c
	$(CC) $(CFLAGS) -o client src/client.c src/config.c src/utils.c

clean:
	rm -f server client server_san
	rm -rf *.dSYM

.PHONY: all clean
