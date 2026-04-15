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
size_t handle_recv(const int c, const char *buffer, const size_t size);
size_t handle_send(const int c, const char *bytes, const size_t size);
void init_clients(struct client_info *c);
void route_to_scope(struct packet_header header, char *payload,
    struct client_info *clients);
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
    size_t header_read = handle_recv(c, buffer, sizeof(struct packet_header));

    if (header_read == 0) {
        // TODO: handle disconnect
        return;
    }

    struct packet_header header;
    memcpy(&header, buffer, header_read);
    header.payload_len = ntohl(header.payload_len);
    header.scope_id = ntohl(header.scope_id);
    header.sender_id = ntohl(header.sender_id);
    header.expires_at = be64toh(header.expires_at);

    if (header.payload_len > MAX_PAYLOAD) {
        printf("Payload is too large.\n");
        // TODO: handle disconnect
        return;
    }

    char payload[MAX_PAYLOAD];
    size_t payload_read = handle_recv(c, payload, header.payload_len);

    if (payload_read <= 0) {
        // TODO: handle disconnect
        return;
    }

    switch((enum packet_type)header.type) {
        case TYPE_SYS_IDENTIFY:
            strncpy(clients[c].username, payload, MAX_NAME - 1);
            clients[c].username[MAX_NAME - 1] = '\0';
            clients[c].is_identified = 1;
            clients[c].client_id = header.sender_id;
            break;
        case TYPE_SYS_JOIN:
            clients[c].scope_id = header.scope_id;
            break;
        case TYPE_SYS_PING:
            break;
        default:
            if (
                header.expires_at != 0 &&
                header.expires_at < (uint64_t)time(NULL)
            ) {
                printf("Packet's TTL is expired.\n");
                break;
            }
            route_to_scope(header, payload, clients);
    }
}

void route_to_scope(struct packet_header header, char *payload,
    struct client_info *clients)
{
    header.payload_len = htonl(header.payload_len);
    header.scope_id = htonl(header.scope_id);
    header.sender_id = htonl(header.sender_id);
    header.expires_at = htobe64(header.expires_at);

    for (int i = 0; i < FD_SETSIZE; i++) {
        if (
            clients[i].socket_fd == -1 ||
            header.sender_id == clients[i].client_id ||
            clients[i].scope_id != header.scope_id
        )
            continue;

        size_t header_sent = handle_send(i, &header,
            sizeof(struct packet_header));
        if (header_sent <= 0) {
            // TODO: handle disconnection
            return;
        }

        size_t payload_sent = handle_send(i, payload, header.payload_len);
        if (payload_sent <= 0) {
            // TODO: handle disconnection
            return;
        }
    }

    return 0;
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

size_t handle_send(const int c, const char *bytes, const size_t size)
{
    size_t total_sent = 0;
    ssize_t bytes_sent;
    while (total_sent < size) {
        bytes_sent = send(c, bytes + total_sent, size - total_sent, 0);
        if (bytes_sent == -1)
            perror("send");
        total_sent += bytes_sent;
    }
    return total_sent;
}

size_t handle_recv(const int c, const char *buffer, const size_t size)
{
    size_t total = 0;
    while (total < size) {
        ssize_t n = recv(c, buffer + total, size - total, 0);
        if (n == -1) {
            perror("recv");
            break;
        }
        total += n;
    }
    return total;
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