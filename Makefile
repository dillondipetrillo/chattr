CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LFLAGS = -lpthread -lssl -lcrypto -lcurl

ENGINE_SRCS = src/server.c \
	src/utils.c \
	src/logger.c \
	src/auth_hook.c \
	src/config.c

TEST_AUTH = tests/test_auth_hook.c src/auth_hook.c src/logger.c

all: server client

server: $(ENGINE_SRCS)
	$(CC) $(CFLAGS) -o server $(ENGINE_SRCS) $(LFLAGS)

client: src/client.c src/config.c src/utils.c src/logger.c
	$(CC) $(CFLAGS) -o client \
		src/client.c \
		src/config.c \
		src/utils.c \
		src/logger.c

tests/run_auth: $(TEST_AUTH)
	$(CC) $(CFLAGS) -o $@ $(TEST_AUTH) $(LFLAGS)

sanitize: $(ENGINE_SRCS)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o server_san \
		$(ENGINE_SRCS) $(LFLAGS)

clean:
	rm -f server client server_san
	rm -rf tests/run_*
	rm -rf *.dSYM

.PHONY: all clean
