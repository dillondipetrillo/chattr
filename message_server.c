#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define AUTH "AUTH"
#define MAX_BUFF_SIZE 1024
#define PORT 8080
#define USERNAME_MAX_LEN 32

struct client {
    char username[USERNAME_MAX_LEN];
    int authenticated;
};

int  can_add_username(
    char *arg, int *maxfd, int c, struct client *clients
);
void handle_client(
    int c, int s, int *maxfd, fd_set *main, struct client *clients
);
void handle_new_socket(int s, fd_set *main, int *maxfd);
void initialize_clients(struct client *c);
int  is_valid_username(char *arg);
void parse_command(char *buffer, char **cmd, char **arg);
void reset_client(int c, struct client *clients);
void setup_server(int *server);
char *trim_arg(char *s);
void update_maxfd(int s, int *maxfd, fd_set *main);
int  validate_command(char *cmd, char *arg);

int main(void)
{
    struct client clients[FD_SETSIZE];
    int server, maxfd;
    fd_set main, readfds;

    initialize_clients(clients);
    setup_server(&server);

    FD_ZERO(&main);
    FD_ZERO(&readfds);
    FD_SET(server, &main);
    maxfd = server;

    while (1) {
        readfds = main;
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1) {
            printf("Error: Select encountered an error.\n");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &readfds)) {
                if (i == server)
                    handle_new_socket(i, &main, &maxfd);
                else
                    handle_client(i, server, &maxfd, &main, clients);
            }
        }
    }

    exit(EXIT_SUCCESS);
}

void initialize_clients(struct client *c) {
    for (int i = 0; i < FD_SETSIZE; i++) {
        c[i].authenticated = 0;
        c[i].username[0] = '\0';
    }
}

void handle_client(
    int c, int s, int *maxfd, fd_set *main, struct client *clients
) {
    char buffer[MAX_BUFF_SIZE];
    char *cmd, *arg;
    ssize_t bytes_received = recv(
        c,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    // Socket is either closed or encountered an error
    if (bytes_received <= 0) {
        char *msg = (bytes_received == 0) ?
            "Closed socket" :
            "Error: Couldn't receieve stream on socket";
        printf("%s %d\n", msg, c);
        close(c);
        reset_client(c, clients);
        FD_CLR(c, main);
        if (c == *maxfd)
            update_maxfd(s, maxfd, main);
        return;
    }
    
    // Socket receieved input with no error
    buffer[bytes_received] = '\0';
    parse_command(buffer, &cmd, &arg);
    if (validate_command(cmd, arg)) {
        // Valid command and argument, handle command
        if (strcmp(cmd, AUTH) == 0 &&
            can_add_username(arg, maxfd, c, clients)
        ) {
            clients[c].authenticated = 1;
            strncpy(clients[c].username, arg, USERNAME_MAX_LEN - 1);
            clients[c].username[USERNAME_MAX_LEN - 1] = '\0';
            printf("OK: %s is authenticated.\n", clients[c].username);
            printf("Connected clients\n");
            for (int i = 0; i <= *maxfd; i++) {
                printf("%s ", clients[i].username);
            }
            printf("\n");
        }
    }
}

void reset_client(int c, struct client *clients) {
    clients[c].authenticated = 0;
    clients[c].username[0] = '\0';
}

int can_add_username(
    char *arg, int *maxfd, int c, struct client *clients
) {
    if (strlen(arg) >= USERNAME_MAX_LEN) {
        printf("Error: username is too long.\n");
        return 0;
    }
    if (!is_valid_username(arg)) {
        printf("Error: not a valid username format.\n");
        return 0;
    }
    if (clients[c].authenticated) {
        printf("Error: client already has a username.\n");
        return 0;
    }
    for (int i = 0; i <= *maxfd; i++) {
        if (strcmp(arg, clients[i].username) == 0) {
            printf("Error: username is already taken.\n");
            return 0;
        }
    }
    return 1;
}

int is_valid_username(char *arg) {
    while (*arg) {
        if (!isalnum((unsigned char)*arg) && *arg != '_') {
            return 0;
        }
        arg++;
    }
    return 1;
}

void parse_command(char *buffer, char **cmd, char **arg) {
    *cmd = buffer;
    *arg = NULL;
    for (int i = 0; buffer[i]; i++) {
        if (buffer[i] == ' ') {
            buffer[i] = '\0';
            *arg = &buffer[i+1];
            break;
        }
    }
}

int validate_command(char *cmd, char *arg) {
    cmd = trim_arg(cmd);
    arg = trim_arg(arg);
    if (cmd == NULL || *cmd == '\0') {
        printf("Error: command not recognized.\n");
        return 0;
    }
    if (arg == NULL || *arg == '\0') {
        printf("Error: argument not provided.\n");
        return 0;
    }
    return 1;
}

char *trim_arg(char *s) {
    if (s == NULL) return NULL;

    // Skip leading spaces
    while (*s == ' ') s++;

    // Trim trailing spaces/newlines
    size_t len = strlen(s);
    while (len > 0 &&
        (s[len-1] == ' ' || s[len-1] == '\n')
    ) {
        s[len-1] = '\0';
        len--;
    }

    return s;
}

void handle_new_socket(int s, fd_set *main, int *maxfd) {
    int client_socket = accept(s, NULL, NULL);
    if (client_socket == -1) {
        printf("Error: Cannot accept incoming client.\n");
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
        printf("Error: Cannot create socket.\n");
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
        printf("Error: Cannot bind socket to address.\n");
        exit(EXIT_FAILURE);
    }

    if (listen(*server, 4) == -1) {
        printf("Error: Cannot start listening.\n");
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
