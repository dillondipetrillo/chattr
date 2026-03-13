#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFF_SIZE 1024
#define PORT 8080

void setup_server(int *server);
void handle_new_connection(int s, fd_set *main, int *maxfd);
void handle_new_client(int c, int s, int *maxfd, fd_set *main);
void update_maxfd(int s, int *maxfd, fd_set *main);

int main(void)
{
    int server, maxfd;
    fd_set main, readfds;

    setup_server(&server);

    FD_ZERO(&main);
    FD_ZERO(&readfds);
    FD_SET(server, &main);
    maxfd = server;

    while (1) {
        readfds = main;
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("Error: Select encountered an error.\n");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &readfds)) {
                if (i == server)
                    // handle new connection
                    handle_new_connection(i, &main, &maxfd);
                else
                    // handle new client
                    handle_new_client(i, server, &maxfd, &main);
            }
        }
    }

    exit(EXIT_SUCCESS);
}

void handle_new_client(int c, int s, int *maxfd, fd_set *main) {
    char buffer[MAX_BUFF_SIZE];
    ssize_t bytes_received;
    if ((bytes_received = recv(
        c,
        buffer,
        sizeof(buffer) - 1, 0)) > 0
    ) {
       buffer[bytes_received] = '\0';
        printf("Received(socket %d): %s", c, buffer);
        return;
    }

    if (bytes_received == 0) {
        printf("Closed socket %d\n", c);
    } else if (bytes_received == -1) {
        printf("Error: Socket %d couldn't receieve stream.\n", c);
    }

    close(c);
    FD_CLR(c, main);
    if (c == *maxfd)
        update_maxfd(s, maxfd, main);
}

void handle_new_connection(int s, fd_set *main, int *maxfd) {
    int client_socket = accept(s, NULL, NULL);
    if (client_socket == -1) {
        perror("Error: Cannot accept incoming client.\n");
        return;
    }

    FD_SET(client_socket, main);
    if (client_socket > *maxfd)
        *maxfd = client_socket; 
    printf("Connected to client on socket %d...\n", client_socket);
}

void setup_server(int *server) {
    *server = socket(PF_INET, SOCK_STREAM, 0);
    if (*server == -1) {
        perror("Error: Cannot create socket.\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(*server,
        (struct sockaddr *) &address,
        sizeof(address)) == -1
    ) {
        perror("Error: Cannot bind socket to address.\n");
        exit(EXIT_FAILURE);
    }

    if (listen(*server, 4) == -1) {
        perror("Error: Cannot start listening.\n");
        exit(EXIT_FAILURE);
    }
}

void update_maxfd(int s, int *maxfd, fd_set *main) {
    int new_maxfd = s;
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, main) && i > new_maxfd)
            new_maxfd = i;
    }
    *maxfd = new_maxfd;
}
