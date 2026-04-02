#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define AUTH "AUTH"         // Command to authenticate users
#define ERROR "ERROR"       // Macro for sending error messages
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

enum client_cmd {
    CMD_AUTH,
    CMD_SEND,
    CMD_UNKNOWN
};

enum error_res {
    ERR_NONE = 0, // No error
    ERR_ALREADY_AUTHENTICATED,
    ERR_BUFFER_OVERFLOW,
    ERR_INVALID_USERNAME,
    ERR_LINE_TOO_LONG,
    ERR_MISSING_ARGUMENT,
    ERR_MISSING_COMMAND,
    ERR_UNKNOWN_COMMAND,
    ERR_USERNAME_TAKEN,
    ERR_USERNAME_TOO_LONG
};

struct client {
    char buffer[MAX_BUFF_SIZE];
    int buffer_len;
    enum client_state state;
    char username[USERNAME_MAX_LEN];
};

// Function prototypes
enum error_res can_add_username(const char *arg, const int *maxfd, const int c,
    const struct client *clients);
enum error_res handle_auth(const char *arg, const int *maxfd, const int c,
    struct client *clients);
void handle_client(const int c, const int s, int *maxfd, fd_set *main,
    struct client *clients);
void handle_close_socket(const int c, const int s, struct client *clients,
    fd_set *main, int *maxfd);
void handle_new_socket(const int s, int *maxfd, fd_set *main);
void handle_send_message(char *arg, const int *maxfd, const int c,
    const struct client *clients);
void initialize_clients(struct client *c);
int  is_valid_username(const char *arg);
enum client_cmd parse_command(char *cmd_str);
void process_input(const int c, const int *maxfd, char *read_line,
    struct client *clients);
void reset_client(const int c, struct client *clients);
int send_all(const int c, const char *buffer, const size_t length);
void send_error_code(const int c, const enum client_cmd cmd,
    const enum error_res err);
int setup_server(void);
void split_input(char *buffer, char **cmd, char **arg);
char *trim_arg(char *s);
int update_maxfd(const int s, const fd_set *main);
enum error_res  validate_input(const int c, char *read_line, char **cmd,
    char **arg);

int main(void)
{
    struct client clients[FD_SETSIZE];
    int server, maxfd;
    fd_set main, readfds;

    initialize_clients(clients);
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
                handle_client(i, server, &maxfd, &main, clients);
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

void handle_client(const int c, const int s, int *maxfd, fd_set *main,
    struct client *clients)
{
    char temp_buff[MAX_BUFF_SIZE];
    ssize_t bytes_received = recv(c, temp_buff, sizeof(temp_buff) - 1, 0);

    // Socket encountered an error on recv, connection lost.
    if (bytes_received == -1) {
        perror("recv");
        handle_close_socket(c, s, clients, main, maxfd);
        return;
    }

    // Socket was closed manually.
    if (bytes_received == 0) {
        if (clients[c].state)
            printf("Client '%s' disconnected (socket %d)\n",
                clients[c].username, c);
        else
            printf("Unauthenticated client disconnected (socket %d)\n", c);
        handle_close_socket(c, s, clients, main, maxfd);
        return;
    }

    // Too many characters entered to fit into client buffer
    if (clients[c].buffer_len + bytes_received > MAX_BUFF_SIZE) {
        send_error_code(c, CMD_UNKNOWN, ERR_BUFFER_OVERFLOW);
        handle_close_socket(c, s, clients, main, maxfd);
        return;
    }
    
    // Socket receieved input with no error.
    // Copy receieved input into client buffer and process it.
    memcpy(clients[c].buffer + clients[c].buffer_len, temp_buff,
        bytes_received);
    clients[c].buffer_len += bytes_received;
    char *buf = clients[c].buffer;

    for (int i = 0; i < clients[c].buffer_len; i++) {
        if (buf[i] == '\n') {
            if (i >= MAX_LINE_SIZE) {
                send_error_code(c, CMD_UNKNOWN, ERR_LINE_TOO_LONG);
                handle_close_socket(c, s, clients, main, maxfd);
                return;
            }
            char read_line[MAX_LINE_SIZE];
            memcpy(read_line, buf, i);
            read_line[i] = '\0';

            process_input(c, maxfd, read_line, clients);

            // Move client buffer pointer to position after the \n.
            memmove(buf, buf + i + 1, clients[c].buffer_len - (i + 1));
            clients[c].buffer_len -= (i + 1);
            // After loop iteration is over, i++ runs and sets i back to 0.
            i = -1;
        }
    }
}

enum client_cmd parse_command(char *cmd_str)
{
    if (strcmp(cmd_str, AUTH) == 0) return CMD_AUTH;
    if (strcmp(cmd_str, SEND) == 0) return CMD_SEND;
    return CMD_UNKNOWN;
}

void process_input(const int c, const int *maxfd, char *read_line,
    struct client *clients)
{
    char *cmd_str, *arg_str;
    enum error_res err_input = validate_input(c, read_line, &cmd_str,
        &arg_str);
    if (err_input != ERR_NONE) {
        send_error_code(c, CMD_UNKNOWN, err_input);
        return;
    }

    enum client_cmd cmd = parse_command(cmd_str);
    enum error_res err_cmd;

    switch(cmd) {
        case CMD_AUTH:
            err_cmd = handle_auth(arg_str, maxfd, c, clients);
            if (err_cmd != ERR_NONE) {
                send_error_code(c, CMD_AUTH, err_cmd);
                return;
            }
            // send_ok(c, CMD_AUTH);
            break;

        case CMD_SEND:
            err_cmd = handle_send();
            if (err_cmd != ERR_NONE) {
                send_error_code(c, CMD_SEND, err_cmd);
                return;
            }
            /*
                if (clients[c].state)
                    handle_send_message(arg_str, maxfd, c, clients);
                else
                    send_error(c, "Need to authenticate to send message.");
            */
            // send_ok(c, CMD_SEND);
            break;

        default:
            send_error_code(c, CMD_UNKNOWN, ERR_UNKNOWN_COMMAND);
    }
}

enum error_res handle_auth(const char *arg, const int *maxfd, const int c,
    struct client *clients)
{
    enum error_res is_valid_username = can_add_username(arg, maxfd, c,
        clients);
    if (is_valid_username != ERR_NONE)
        return is_valid_username;

    // Is a valid username, add user
    clients[c].state = STATE_AUTHENTICATED;
    strncpy(clients[c].username, arg, USERNAME_MAX_LEN - 1);
    clients[c].username[USERNAME_MAX_LEN - 1] = '\0';
    printf("OK: %s is authenticated.\n", clients[c].username);
    return ERR_NONE;
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

int send_all(const int c, const char *buffer, const size_t length)
{
    size_t total_sent = 0;
    ssize_t bytes_sent;
    while (total_sent < length) {
        bytes_sent = send(c, buffer + total_sent, length - total_sent, 0);
        if (bytes_sent == -1)
            perror("send");
            return -1; // failure;
        total_sent += bytes_sent;
    }
    return 0; // success
}

void send_error_code(const int c, const enum client_cmd cmd,
    const enum error_res err)
{
    char buffer[MAX_LINE_SIZE];
    snprintf(buffer, sizeof(buffer), "%s %s %s", ERROR, command_to_str(cmd),
        error_to_str(err));
    send_all(c, buffer, strlen(buffer));
}

void handle_send_message(char *arg, const int *maxfd, const int c,
    const struct client *clients)
{
    char *receiver, *message;
    if (!validate_input(c, arg, &receiver, &message))
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

void handle_close_socket(const int c, const int s, struct client *clients,
    fd_set *main, int *maxfd)
{
    close(c);
    reset_client(c, clients);
    FD_CLR(c, main);
    if (c == *maxfd)
        *maxfd = update_maxfd(s, main);
}

void reset_client(const int c, struct client *clients)
{
    clients[c].state = STATE_CONNECTED;
    clients[c].buffer_len = 0;
    clients[c].username[0] = '\0';
}

enum error_res can_add_username(const char *arg, const int *maxfd, const int c,
    const struct client *clients)
{
    if (strlen(arg) >= USERNAME_MAX_LEN) return ERR_USERNAME_TOO_LONG;
    if (!is_valid_username(arg)) return ERR_INVALID_USERNAME;
    if (clients[c].state) return ERR_ALREADY_AUTHENTICATED;
    for (int i = 0; i <= *maxfd; i++) {
        if (strcmp(arg, clients[i].username) == 0)
            return ERR_USERNAME_TAKEN;
    }
    return ERR_NONE;
}

int is_valid_username(const char *arg)
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

enum error_res validate_input(const int c, char *read_line, char **cmd,
    char **arg)
{
    split_input(read_line, cmd, arg);
    *cmd = trim_arg(*cmd);
    *arg = trim_arg(*arg);
    if (*cmd == NULL || **cmd == '\0') {
        return ERR_MISSING_COMMAND;
    }
    if (*arg == NULL || **arg == '\0') {
        return ERR_MISSING_ARGUMENT;
    }
    return ERR_NONE;
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

int setup_server(void)
{
    int server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

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

int update_maxfd(const int s, const fd_set *main)
{
    int new_maxfd = s;
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, main) && i > new_maxfd)
            new_maxfd = i;
    }
    return new_maxfd;
}
