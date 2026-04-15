#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include "protocol.h"
#include "utils.h"

#if defined(__APPLE__)
    #include <machine/endian.h>
    #include <libkern/OSByteOrder.h>
    #define htobe64(x) OSSwapHostToBigInt64(x)
    #define be64toh(x) OSSwapBigToHostInt64(x)
#else
    #include <endian.h>
#endif

#define PORT 8080

void handle_client(const int c, const int s, int *maxfd, fd_set *main,
    struct client_info *clients);
void disconnect_client(const int c, const int s, int *maxfd, fd_set *main,
    struct client_info *clients);
void handle_new_socket(const int s, int *maxfd, fd_set *main,
    struct client_info *clients);
void init_clients(struct client_info *c);
ssize_t route_to_scope(struct packet_header header, char *payload,
    struct client_info *clients);
int setup_server(void);
int update_maxfd(const int s, const fd_set *main);

int main(void)
{
    struct client_info clients[MAX_CLIENTS];
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
                handle_new_socket(i, &maxfd, &main, clients);
            else
                handle_client(i, server, &maxfd, &main, clients);
        }
    }

    return EXIT_SUCCESS;
}

void handle_client(const int c, const int s, int *maxfd, fd_set *main,
    struct client_info *clients)
{
    char buffer[sizeof(struct packet_header)];
    ssize_t header_read = handle_recv(c, buffer, sizeof(struct packet_header));

    if (header_read <= 0) {
        if (header_read == -1)
            perror("recv");
        disconnect_client(c, s, maxfd, main, clients);
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
        disconnect_client(c, s, maxfd, main, clients);
        return;
    }

    char payload[MAX_PAYLOAD];
    ssize_t payload_read = handle_recv(c, payload, header.payload_len);

    if (payload_read <= 0) {
        if (payload_read == -1)
            perror("recv");
        disconnect_client(c, s, maxfd, main, clients);
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
        case TYPE_SYS_PING: {
            struct packet_header response;
            memset(&response, 0, sizeof(struct packet_header));
            response.type = TYPE_SYS_PING;

            response.payload_len = htonl(response.payload_len);
            response.scope_id = htonl(response.scope_id);
            response.sender_id = htonl(response.sender_id);
            response.expires_at = htobe64(response.expires_at);

            ssize_t ping_result = handle_send(c, (const char *)&response,
                sizeof(struct packet_header));
            if (ping_result == -1)
                disconnect_client(c, s, maxfd, main, clients);
            break;
        }
        default:
            if (
                header.expires_at != 0 &&
                header.expires_at < (uint64_t)time(NULL)
            ) {
                printf("Packet's TTL is expired.\n");
                break;
            }
            ssize_t sent = route_to_scope(header, payload, clients);
            if (sent == -1) {
                perror("send");
                disconnect_client(c, s, maxfd, main, clients);
            }
    }
}

void disconnect_client(const int c, const int s, int *maxfd, fd_set *main,
    struct client_info *clients)
{
    close(c);
    memset(&clients[c], 0, sizeof(struct client_info));
    clients[c].socket_fd = -1;
    FD_CLR(c, main);
    if (c == *maxfd)
        *maxfd = update_maxfd(s, main);
}

ssize_t route_to_scope(struct packet_header header, char *payload,
    struct client_info *clients)
{
    uint32_t sender_id = header.sender_id;
    uint32_t scope_id = header.scope_id;
    uint32_t payload_len = header.payload_len;

    header.payload_len = htonl(header.payload_len);
    header.scope_id = htonl(header.scope_id);
    header.sender_id = htonl(header.sender_id);
    header.expires_at = htobe64(header.expires_at);

    int routed = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (
            clients[i].socket_fd == -1 ||
            clients[i].client_id == sender_id ||
            clients[i].scope_id != scope_id
        )
            continue;

        ssize_t header_sent = handle_send(i, (const char *)&header,
            sizeof(struct packet_header));
        if (header_sent == -1)
            return header_sent;

        ssize_t payload_sent = handle_send(i, payload, payload_len);
        if (payload_sent == -1)
            return payload_sent;

        routed++;
    }

    return routed;
}

int update_maxfd(const int s, const fd_set *main)
{
    int new_maxfd = s;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (FD_ISSET(i, main) && i > new_maxfd)
            new_maxfd = i;
    }
    return new_maxfd;
}

void handle_new_socket(const int s, int *maxfd, fd_set *main,
    struct client_info *clients)
{
    int client_socket = accept(s, NULL, NULL);
    if (client_socket == -1) {
        perror("accept");
        return;
    }

    FD_SET(client_socket, main);
    clients[client_socket].socket_fd = client_socket;
    printf("Connected to client on socket %d...\n", client_socket);
    if (client_socket > *maxfd)
        *maxfd = client_socket;
}

void init_clients(struct client_info *c)
{
    memset(c, 0, sizeof(struct client_info) * MAX_CLIENTS);
    for (int i = 0; i < MAX_CLIENTS; i++)
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