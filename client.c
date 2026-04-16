#include <arpa/inet.h>
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

int main(void)
{
    int socketfd = socket(PF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        perror("socket");
        return(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (connect(socketfd, (struct sockaddr *)&address,
            sizeof(address)) == -1
    ) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server...\n");

    const char *username = "Dillon";
    struct packet_header identify;
    memset(&identify, 0, sizeof(identify));
    identify.type = (uint8_t)TYPE_SYS_IDENTIFY;
    identify.payload_len = strlen(username);
    identify.sender_id = 1;

    identify.payload_len = htonl(identify.payload_len);
    identify.scope_id = htonl(identify.scope_id);
    identify.sender_id = htonl(identify.sender_id);
    identify.expires_at = htobe64(identify.expires_at);

    handle_send(socketfd, (const char *)&identify,
        sizeof(struct packet_header));
    handle_send(socketfd, username, strlen(username));

    uint32_t scope_id = 1;
    struct packet_header join;
    memset(&join, 0, sizeof(join));
    join.type = (uint8_t)TYPE_SYS_JOIN;
    join.payload_len = sizeof(uint32_t);
    join.scope_id = scope_id;
    join.sender_id = 1;

    join.payload_len = htonl(join.payload_len);
    join.scope_id = htonl(join.scope_id);
    join.sender_id = htonl(join.sender_id);
    join.expires_at = htobe64(join.expires_at);

    uint32_t scope_payload = htonl(scope_id);
    handle_send(socketfd, (const char *)&join, sizeof(struct packet_header));
    handle_send(socketfd, (const char *)&scope_payload, sizeof(uint32_t));
    printf("Handshake complete.\n");

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(socketfd, &readfds);

        int maxfd = socketfd;

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char input[MAX_PAYLOAD];
            if (fgets(input, MAX_PAYLOAD, stdin) == NULL)
                break;
            struct packet_header send_header;
            memset(&send_header, 0, sizeof(send_header));
            send_header.type = (uint8_t)TYPE_APP_REALTIME;
            send_header.sender_id = 1;
            send_header.scope_id = scope_id;
            send_header.payload_len = strlen(input);
            send_header.expires_at = (uint64_t)time(NULL) + 300;

            send_header.payload_len = htonl(send_header.payload_len);
            send_header.scope_id = htonl(send_header.scope_id);
            send_header.sender_id = htonl(send_header.sender_id);
            send_header.expires_at = htobe64(send_header.expires_at);

            handle_send(socketfd, (const char *)&send_header,
                sizeof(struct packet_header));
            handle_send(socketfd, input, strlen(input));
        }

        if (FD_ISSET(socketfd, &readfds)) {
            char r_buffer[sizeof(struct packet_header)];
            ssize_t recv_header = handle_recv(socketfd, r_buffer,
                sizeof(struct packet_header));

            if (recv_header == 0) {
                printf("Server disconnected.\n");
                exit(EXIT_SUCCESS);
            }
            
            if (recv_header == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }

            struct packet_header r_header;
            memcpy(&r_header, r_buffer, sizeof(recv_header));
            r_header.payload_len = ntohl(r_header.payload_len);
            r_header.scope_id    = ntohl(r_header.scope_id);
            r_header.sender_id   = ntohl(r_header.sender_id);
            r_header.expires_at  = be64toh(r_header.expires_at);

            if (r_header.payload_len > MAX_PAYLOAD) {
                printf("Payload is too large to receieve.\n");
                exit(EXIT_FAILURE);
            }

            char r_payload[MAX_PAYLOAD + 1];
            ssize_t recv_payload = handle_recv(socketfd, r_payload,
                r_header.payload_len);

            if (recv_payload == 0) {
                printf("Server disconnected.\n");
                exit(EXIT_SUCCESS);
            }
                
            if (recv_payload == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }
                
            r_payload[r_header.payload_len] = '\0';
            printf("Message: %s\n", r_payload);
        }
    }

    exit(EXIT_SUCCESS);
}