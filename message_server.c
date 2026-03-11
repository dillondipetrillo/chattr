#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFF_SIZE 1024

int main(void)
{
    int server = socket(PF_INET, SOCK_STREAM, 0);
    if (server == -1) {
        printf("Error: Cannot create socket.\n");
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
        printf("Error: Cannot bind socket to address.\n");
        exit(EXIT_FAILURE);
    }

    if (listen(server, 4) == -1) {
        printf("Error: Cannot start listening.\n");
        exit(EXIT_FAILURE);
    }

    int client_socket = accept(server, NULL, NULL);
    if (client_socket == -1) {
        printf("Error: Cannot accept incoming client.\n");
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

    close(server);
    close(client_socket);

    exit(EXIT_SUCCESS);
}
