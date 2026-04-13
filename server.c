#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include "protocol.h"

#if defined(__APPLE__)
    #include <machine/endian.h>
    #include <libkern/OSByteOrder.h>
    #define htobe64(x) OSSwapHostToBigInt64(x)
    #define be64toh(x) OSSwapBigToHostInt64(x)
#else
    #include <endian.h>
#endif

#define PORT 8080

void handle_client(const int c, struct client_info *clients);
void handle_new_socket(const int s, int *maxfd, fd_set *main);
void init_clients(struct client_info *c);
int setup_server(void);

int main(void)
{
    struct client_info clients[FD_SETSIZE];
    int server, maxfd;
    fd_set main, readfds;

    init_clients(clients);
    server = setup_server();

    FD_ZERO(&main);
    FD_ZERO(&readfds);
    FD_SET(server, &main);
    maxfd = server;

    while (1) {
        readfds = main;
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= maxfd; i++) {
            if (!FD_ISSET(i, &readfds))
                continue;
            if (i == server)
                handle_new_socket(i, &maxfd, &main);
            else
                handle_client(i, clients);
        }
    }

    return EXIT_SUCCESS;
}

void handle_client(const int c, struct client_info *clients)
{
    char buffer[sizeof(struct packet_header)];
    size_t total = 0;

    while (total < sizeof(struct packet_header)) {
        ssize_t n = recv(c, buffer + total,
            sizeof(struct packet_header) - total, 0);
        if (n <= 0) {
            if (n == -1)
                perror("recv");
            /* handle disconnect */
            return;
        }

        total += n;
    }

    struct packet_header header;
    memcpy(&header, buffer, total);
    header.payload_len = ntohl(header.payload_len);
    header.scope_id = ntohl(header.scope_id);
    header.sender_id = ntohl(header.sender_id);
    header.expires_at = be64toh(header.expires_at);

    if (header.payload_len > MAX_PAYLOAD) {
        printf("Payload is too large.\n");
        /* handle disconnect */
    }

    char payload[MAX_PAYLOAD];
    size_t payload_total = 0;

    while (payload_total < sizeof(header.payload_len)) {
        ssize_t p = recv(c, payload + payload_total,
            sizeof(header.payload_len) - payload_total, 0);
        if (p <= 0) {
            if (p == -1)
                perror("recv");
            /* handle disconnect */
            return;
        }

        payload_total += p;
    }
}

void handle_new_socket(const int s, int *maxfd, fd_set *main)
{
    int client_socket = accept(s, NULL, NULL);
    if (client_socket == -1) {
        perror("accept");
        return;
    }

    FD_SET(client_socket, main);
    printf("Connected to client on socket %d...\n", client_socket);
    if (client_socket > *maxfd)
        *maxfd = client_socket;
}

void init_clients(struct client_info *c)
{
    memset(c, 0, sizeof(struct client_info) * FD_SETSIZE);
    for (int i = 0; i < FD_SETSIZE; i++)
        c[i].socket_fd = -1;
}

int setup_server(void)
{
    int server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
            sizeof(opt)) == -1)
        perror("setsockopt");

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 4) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return server_fd;
}