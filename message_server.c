#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define AUTH "AUTH"         // Command to authenticate users
#define MAX_LINE_SIZE 1024  // Maximum length for client buffer
#define MAX_BUFF_SIZE 4096  // Maximum buffer size
#define PORT 8080           // Default port for socket
#define SEND "SEND"         // Command to send a message
#define USERNAME_MAX_LEN 32 // Max length for usernames

// Enum that determines a client sockets authenticated state.
enum client_state {
    STATE_CONNECTED,
    STATE_AUTHENTICATED
};

struct client {
    char buffer[MAX_BUFF_SIZE];
    int buffer_len;
    enum client_state state;
    char username[USERNAME_MAX_LEN];
};

// Function prototypes
int  can_add_username(char *arg, int *maxfd, int c, struct client *clients);
void handle_client(int c, int s, int *maxfd, fd_set *main,
    struct client *clients);
void handle_close_socket(int bytes_received, int c, struct client *clients,
    fd_set *main, int *maxfd, int s);
void handle_new_socket(int s, fd_set *main, int *maxfd);
void handle_send_message(char *arg, int *maxfd, int c, struct client *clients);
void initialize_clients(struct client *c);
int  is_valid_username(char *arg);
void process_input(int c, int *maxfd, char *read_line, struct client *clients);
void reset_client(int c, struct client *clients);
void send_error(int c, char *err_msg);
void setup_server(int *server);
void split_input(char *buffer, char **cmd, char **arg);
char *trim_arg(char *s);
void update_maxfd(int s, int *maxfd, fd_set *main);
int  validate_input(int c, char **cmd, char **arg);

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

void initialize_clients(struct client *c)
{
    for (int i = 0; i < FD_SETSIZE; i++) {
        c[i].state = STATE_CONNECTED;
        c[i].buffer_len = 0;
        c[i].username[0] = '\0';
    }
}

void handle_client(int c, int s, int *maxfd, fd_set *main,
    struct client *clients)
{
    char temp_buff[MAX_BUFF_SIZE];
    ssize_t bytes_received = recv(c, temp_buff, sizeof(temp_buff) - 1, 0);

    // Socket was either closed or encountered an error
    if (bytes_received <= 0) {
        handle_close_socket(bytes_received, c, clients, main, maxfd, s);
        return;
    }

    // Too many characters entered to fit into client buffer
    if (clients[c].buffer_len + bytes_received > MAX_BUFF_SIZE) {
        send_error(c, "too many characters entered.");
        return;
    }
    
    // Socket receieved input with no error.
    // Copy receieved input into client buffer
    // and process it.
    memcpy(clients[c].buffer + clients[c].buffer_len, temp_buff,
        bytes_received);
    clients[c].buffer_len += bytes_received;
    char *buf = clients[c].buffer;

    for (int i = 0; i < clients[c].buffer_len; i++) {
        if (buf[i] == '\n') {
            if (i >= MAX_LINE_SIZE)
                send_error(c, "User input is too long.");
            else {
                char read_line[MAX_LINE_SIZE];
                memcpy(read_line, buf, i);
                read_line[i] = '\0';

                process_input(c, maxfd, read_line, clients);
            }

            // Move client buffer pointer to position
            // after the \n.
            memmove(buf, buf + i + 1, clients[c].buffer_len - (i + 1));
            clients[c].buffer_len -= (i + 1);
            // Trick, after loop iteration is over, i++
            // runs and sets i back to 0.
            i = -1;
        }
    }
}

void process_input(int c, int *maxfd, char *read_line, struct client *clients)
{
    char *cmd, *arg;
    split_input(read_line, &cmd, &arg);
    if (!validate_input(c, &cmd, &arg))
        return;

    // Valid command and argument, handle command
    if (strcmp(cmd, AUTH) == 0) {
        if (can_add_username(arg, maxfd, c, clients)) {
            clients[c].state = STATE_AUTHENTICATED;
            strncpy(clients[c].username, arg, USERNAME_MAX_LEN - 1);
            clients[c].username[USERNAME_MAX_LEN - 1] = '\0';
            printf("OK: %s is authenticated.\n", clients[c].username);
        }
    } else if (strcmp(cmd, SEND) == 0) {
        if (clients[c].state)
            handle_send_message(arg, maxfd, c, clients);
        else
            send_error(c, "Need to authenticate to send message.");
    } else
        send_error(c, "Error: command not recognized.");
}

void send_error(int c, char *err_msg)
{
    char buffer[MAX_LINE_SIZE];
    char *ptr = buffer;
    snprintf(buffer, sizeof(buffer), "ERROR: %s\n", err_msg);
    int err_msg_len = strlen(buffer);
    ssize_t err_bytes_sent;
    while (err_msg_len > 0) {
        err_bytes_sent = send(c, ptr, err_msg_len, 0);
        if (err_bytes_sent < 0)
            printf("Error sending error message.\n");
        ptr += err_bytes_sent;
        err_msg_len -= err_bytes_sent;
    }
}

void handle_send_message(char *arg, int *maxfd, int c, struct client *clients)
{
    char *receiver, *message;
    split_input(arg, &receiver, &message);
    if (!validate_input(c, &receiver, &message))
        return;
    
    // Refuse sending message to self
    if (strcmp(clients[c].username, receiver) == 0) {
        send_error(c, "can't send message to self.");
        return;
    }

    char full_message[MAX_BUFF_SIZE];
    snprintf(full_message, sizeof(full_message), "[%s] %s\n",
        clients[c].username, message);
    char *ptr = full_message;

    ssize_t bytes_sent;
    int found = 0;
    int message_len = strlen(full_message);
    for (int i = 0; i <= *maxfd; i++) {
        if (clients[i].state && strcmp(clients[i].username, receiver) == 0) {
            found = 1;
            while (message_len > 0) {
                bytes_sent = send(i, ptr, message_len, 0);
                if (bytes_sent < 0) {
                    send_error(c, "couldn't send message.\n");
                }
                ptr += bytes_sent;
                message_len -= bytes_sent;
            }
            break;
        }
    }

    if (!found)
        send_error(c, "no user found.");
}

void handle_close_socket(int bytes_received, int c, struct client *clients,
    fd_set *main, int *maxfd, int s)
{
    if (bytes_received == 0) {
        if (clients[c].state)
            printf("Client '%s' disconnected (socket %d)\n",
                clients[c].username, c);
        else
            printf("Unauthenticated client disconnected (socket %d)\n", c);
    } else
        printf("Error: Couldn't receieve stream on socket %d\n", c);

    close(c);
    reset_client(c, clients);
    FD_CLR(c, main);
    if (c == *maxfd)
        update_maxfd(s, maxfd, main);
}

void reset_client(int c, struct client *clients)
{
    clients[c].state = STATE_CONNECTED;
    clients[c].buffer_len = 0;
    clients[c].username[0] = '\0';
}

int can_add_username(char *arg, int *maxfd, int c, struct client *clients)
{
    if (strlen(arg) >= USERNAME_MAX_LEN) {
        send_error(c, "username is too long.");
        return 0;
    }
    if (!is_valid_username(arg)) {
        send_error(c, "not a valid username format.");
        return 0;
    }
    if (clients[c].state) {
        send_error(c, "client already has a username.");
        return 0;
    }
    for (int i = 0; i <= *maxfd; i++) {
        if (strcmp(arg, clients[i].username) == 0) {
            send_error(c, "username is already taken.");
            return 0;
        }
    }
    return 1;
}

int is_valid_username(char *arg)
{
    while (*arg) {
        if (!isalnum((unsigned char)*arg) && *arg != '_') {
            return 0;
        }
        arg++;
    }
    return 1;
}

void split_input(char *buffer, char **cmd, char **arg)
{
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

int validate_input(int c, char **cmd, char **arg)
{
    *cmd = trim_arg(*cmd);
    *arg = trim_arg(*arg);
    if (*cmd == NULL || **cmd == '\0') {
        send_error(c, "command not provided.");
        return 0;
    }
    if (*arg == NULL || **arg == '\0') {
        send_error(c, "argument not provided.");
        return 0;
    }
    return 1;
}

char *trim_arg(char *s)
{
    if (s == NULL) return NULL;

    // Skip leading spaces
    while (*s == ' ') s++;

    // Trim trailing spaces/newlines
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\n')) {
        s[len-1] = '\0';
        len--;
    }

    return s;
}

void handle_new_socket(int s, fd_set *main, int *maxfd)
{
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

void setup_server(int *server)
{
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

    if (bind(*server, (struct sockaddr *) &address, sizeof(address)) == -1) {
        printf("Error: Cannot bind socket to address.\n");
        exit(EXIT_FAILURE);
    }

    if (listen(*server, 4) == -1) {
        printf("Error: Cannot start listening.\n");
        exit(EXIT_FAILURE);
    }
}

void update_maxfd(int s, int *maxfd, fd_set *main)
{
    int new_maxfd = s;
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, main) && i > new_maxfd)
            new_maxfd = i;
    }
    *maxfd = new_maxfd;
}
