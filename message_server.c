#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFF_SIZE 1024

int setup_server(void);

int main(void)
{
    int server_socket = setup_server();
    int client_socket = accept(server_socket, NULL, NULL);
    if (client_socket == -1) {
        perror("Error: Cannot accept incoming client.\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to client...\n");

    char buffer[MAX_BUFF_SIZE];
    ssize_t bytes_received;
    while ((bytes_received = recv(
        client_socket,
        buffer,
        sizeof(buffer) - 1, 0)) > 0
    ) {
       buffer[bytes_received] = '\0';
        printf("Received: %s", buffer);
    }

    if (bytes_received == 0) printf("Socket closed.\n");
    else if (bytes_received == -1) {
        printf("Error: Socket couldn't receieve stream.\n");
    }

    close(server_socket);
    close(client_socket);

    exit(EXIT_SUCCESS);
}

int setup_server(void) {
    int server = socket(PF_INET, SOCK_STREAM, 0);
    if (server == -1) {
        perror("Error: Cannot create socket.\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server,
        (struct sockaddr *) &address,
        sizeof(address)) == -1
    ) {
        perror("Error: Cannot bind socket to address.\n");
        exit(EXIT_FAILURE);
    }

    if (listen(server, 4) == -1) {
        perror("Error: Cannot start listening.\n");
        exit(EXIT_FAILURE);
    }

    return server;
}
