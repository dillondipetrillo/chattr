CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LFLAGS = -lpthread -lssl -lcrypto -lcurl

ENGINE_SRCS = src/server.c \
	src/utils.c \
	src/logger.c \
	src/auth_hook.c \
	src/config.c

all: server client

server: $(ENGINE_SRCS)
	$(CC) $(CFLAGS) -o server $(ENGINE_SRCS) $(LFLAGS)

client: src/client.c src/config.c src/utils.c src/logger.c
	$(CC) $(CFLAGS) -o client \
		src/client.c \
		src/config.c \
		src/utils.c \
		src/logger.c

sanitize: $(ENGINE_SRCS)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o server_san \
		$(ENGINE_SRCS) $(LFLAGS)

clean:
	rm -f server client server_san
	rm -rf *.dSYM

.PHONY: all clean
