CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

SHARED = src/utils.c src/logger.c

all: server client

server: src/server.c $(SHARED)
	$(CC) $(CFLAGS) -o server src/server.c $(SHARED)

client: src/client.c $(SHARED)
	$(CC) $(CFLAGS) -o client src/client.c $(SHARED)

clean:
	rm -f server client
	rm -rf *.dSYM

.PHONY: all clean
