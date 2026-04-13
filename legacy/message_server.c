#include <ctype.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

// Macros for program structure
#define MAX_LINE_SIZE 1024
#define MAX_BUFF_SIZE 4096
#define PORT 8080
#define USERNAME_MAX_LEN 32

// Macros for ok/error response commands
#define AUTH "AUTH"
#define ERROR "ERROR"
#define MSG_USER "MSG_USER"
#define OK "OK"
#define SEND_USER "SEND_USER"
#define UNKNOWN_CMD "UNKNOWN"

#define RESULT_ERR(code, detail) (struct error){code, detail}
#define ERR_OK (struct error){ERR_NONE, detail_none}

// Enum that determines a client sockets authenticated state.
enum client_state {
    STATE_AUTHENTICATED,
    STATE_CONNECTED
};

// Commands for clients to use
enum client_cmd {
    CMD_AUTH,
    CMD_SEND_USER,
    CMD_UNKNOWN // Used for error responses
};

// Error codes
enum error_res {
    ERR_NONE = 0, // No error
    ERR_ALREADY_AUTHENTICATED,
    ERR_ALREADY_EXISTS,
    ERR_INPUT_TOO_LARGE,
    ERR_INTERNAL_SERVER_ERROR,
    ERR_INVALID_ARGUMENT,
    ERR_INVALID_COMMAND,
    ERR_MISSING_ARGUMENT,
    ERR_NOT_AUTHENTICATED,
    ERR_NOT_FOUND
};

// Deatiled description for server response
enum detail_res {
    detail_already_authenticated,
    detail_authentication_required,
    detail_exceeds_max_length,
    detail_input_too_large,
    detail_invalid_format,
    detail_line_too_long,
    detail_message_required,
    detail_message_send_error,
    detail_message_too_long,
    detail_none,
    detail_success,
    detail_unknown_command,
    detail_username_required,
    detail_username_taken,
    detail_user_not_found
};

struct error {
    enum error_res code;
    enum detail_res detail;
};

struct client {
    char buffer[MAX_BUFF_SIZE];
    int buffer_len;
    enum client_state state;
    char username[USERNAME_MAX_LEN];
};

char *err_code_lookup[] = {
    "ERROR_NONE",
    "ALREADY_AUTHENTICATED",
    "ALREADY_EXISTS",
    "INPUT_TOO_LARGE",
    "INTERNAL_SERVER_ERROR",
    "INVALID_ARGUMENT",
    "INVALID_COMMAND",
    "MISSING_ARGUMENT",
    "NOT_AUTHENTICATED",
    "NOT_FOUND"
};

char *detail_lookup[] = {
    "already_authenticated",
    "authentication_required",
    "exceeds_max_length",
    "input_too_large",
    "invalid_format",
    "line_too_long",
    "message_required",
    "message_send_error",
    "message_too_long",
    "none",
    "success",
    "unknown_command",
    "username_required",
    "username_taken",
    "user_not_found"
};

// Function prototypes
struct error can_add_username(const char *arg, const int *maxfd,
    const int c, const struct client *clients);
struct error handle_auth(const char *arg, const int *maxfd, const int c,
    struct client *clients);
void handle_client(const int c, const int s, int *maxfd, fd_set *main,
    struct client *clients);
void handle_close_socket(const int c, const int s, struct client *clients,
    fd_set *main, int *maxfd);
void handle_new_socket(const int s, int *maxfd, fd_set *main);
struct error handle_send(char *arg, const int *maxfd, const int c,
    const struct client *clients);
void initialize_clients(struct client *c);
int  is_valid_username(const char *arg);
enum client_cmd parse_command(const char *cmd_str);
void process_input(const int c, const int *maxfd, char *read_line,
    struct client *clients);
void reset_client(const int c, struct client *clients);
ssize_t send_all(const int c, const char *buffer, const size_t length);
void send_error(const int c, const enum client_cmd cmd,
    const struct error err);
void send_ok(const int c, const enum client_cmd cmd);
int setup_server(void);
void split_input(char *buffer, char **cmd, char **arg);
char *trim_arg(char *s);
int update_maxfd(const int s, const fd_set *main);

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
        if (clients[c].state == STATE_AUTHENTICATED)
            printf("Client '%s' disconnected (socket %d)\n",
                clients[c].username, c);
        else
            printf("Unauthenticated client disconnected (socket %d)\n", c);
        handle_close_socket(c, s, clients, main, maxfd);
        return;
    }

    // Too many characters entered to fit into client buffer
    if (clients[c].buffer_len + bytes_received > MAX_BUFF_SIZE) {
        send_error(c, CMD_UNKNOWN, RESULT_ERR(ERR_INPUT_TOO_LARGE,
            detail_input_too_large));
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
                send_error(c, CMD_UNKNOWN, RESULT_ERR(ERR_INPUT_TOO_LARGE,
                    detail_line_too_long));
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

enum client_cmd parse_command(const char *cmd_str)
{
    if (strcmp(cmd_str, AUTH) == 0) return CMD_AUTH;
    if (strcmp(cmd_str, SEND_USER) == 0) return CMD_SEND_USER;
    return CMD_UNKNOWN;
}

void process_input(const int c, const int *maxfd, char *read_line,
    struct client *clients)
{
    char *cmd_str, *arg_str;
    split_input(read_line, &cmd_str, &arg_str);
    cmd_str = trim_arg(cmd_str);
    arg_str = trim_arg(arg_str);

    enum client_cmd cmd = parse_command(cmd_str);
    struct error err_cmd;

    switch(cmd) {
        case CMD_AUTH:
            err_cmd = handle_auth(arg_str, maxfd, c, clients);
            if (err_cmd.code != ERR_NONE) {
                send_error(c, CMD_AUTH, err_cmd);
                return;
            }
            send_ok(c, CMD_AUTH);
            break;

        case CMD_SEND_USER:
            if (clients[c].state != STATE_AUTHENTICATED) {
                send_error(c, CMD_SEND_USER, RESULT_ERR(ERR_NOT_AUTHENTICATED,
                    detail_authentication_required));
                return;
            }

            err_cmd = handle_send(arg_str, maxfd, c, clients);
            if (err_cmd.code != ERR_NONE) {
                send_error(c, CMD_SEND_USER, err_cmd);
                return;
            }
            send_ok(c, CMD_SEND_USER);
            break;

        default:
            send_error(c, CMD_UNKNOWN, RESULT_ERR(ERR_INVALID_COMMAND,
                detail_unknown_command));
            break;
    }
}

struct error handle_auth(const char *arg, const int *maxfd, const int c,
    struct client *clients)
{
    struct error is_valid_username = can_add_username(arg, maxfd, c,
        clients);
    if (is_valid_username.code != ERR_NONE)
        return is_valid_username;

    // Is a valid username, add user
    clients[c].state = STATE_AUTHENTICATED;
    strncpy(clients[c].username, arg, USERNAME_MAX_LEN - 1);
    clients[c].username[USERNAME_MAX_LEN - 1] = '\0';
    return ERR_OK;
}

ssize_t send_all(const int c, const char *buffer, const size_t length)
{
    size_t total_sent = 0;
    ssize_t bytes_sent;
    while (total_sent < length) {
        bytes_sent = send(c, buffer + total_sent, length - total_sent, 0);
        if (bytes_sent == -1) {
            perror("send");
            return -1; // failure;
        }
        total_sent += bytes_sent;
    }
    return 0; // success
}

char *command_to_str(const enum client_cmd cmd)
{
    switch(cmd) {
        case CMD_AUTH: return AUTH;
        case CMD_SEND_USER: return SEND_USER;
        default: return UNKNOWN_CMD;
    }
}

void send_ok(const int c, const enum client_cmd cmd)
{
    char buffer[MAX_LINE_SIZE];
    snprintf(buffer, sizeof(buffer), "%s %s %s\n", OK, command_to_str(cmd),
        detail_lookup[detail_success]);
    send_all(c, buffer, strlen(buffer));
}

void send_error(const int c, const enum client_cmd cmd,
    const struct error err)
{
    char buffer[MAX_LINE_SIZE];
    snprintf(buffer, sizeof(buffer), "%s %s %s %s\n", ERROR,
        command_to_str(cmd), err_code_lookup[err.code],
        detail_lookup[err.detail]);
    send_all(c, buffer, strlen(buffer));
}

struct error handle_send(char *arg, const int *maxfd, const int c,
    const struct client *clients)
{
    if (arg == NULL || *arg == '\0')
        return RESULT_ERR(ERR_MISSING_ARGUMENT, detail_username_required);
    char *receiver, *message;
    split_input(arg, &receiver, &message);
    receiver = trim_arg(receiver);
    message = trim_arg(message);

    if (message == NULL || *message == '\0')
        return RESULT_ERR(ERR_MISSING_ARGUMENT, detail_message_required);

    char full_message[MAX_BUFF_SIZE];
    snprintf(full_message, sizeof(full_message), "%s %s %s\n",
        MSG_USER, clients[c].username, message);
    char *ptr = full_message;

    ssize_t bytes_sent;
    int found = 0;
    int message_len = strlen(full_message);
    for (int i = 0; i <= *maxfd; i++) {
        if (clients[i].state == STATE_AUTHENTICATED &&
            strcmp(clients[i].username, receiver) == 0)
        {
            found = 1;
            bytes_sent = send_all(i, ptr, message_len);
            if (bytes_sent == -1)
                return RESULT_ERR(ERR_INTERNAL_SERVER_ERROR,
                    detail_message_send_error);
            break;
        }
    }

    if (!found)
        return RESULT_ERR(ERR_NOT_FOUND, detail_user_not_found);
    return ERR_OK;
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

struct error can_add_username(const char *arg, const int *maxfd,
    const int c, const struct client *clients)
{
    if (clients[c].state == STATE_AUTHENTICATED)
        return RESULT_ERR(ERR_ALREADY_AUTHENTICATED,
            detail_already_authenticated);
    if (arg == NULL || *arg == '\0')
        return RESULT_ERR(ERR_MISSING_ARGUMENT, detail_username_required);
    if (!is_valid_username(arg))
        return RESULT_ERR(ERR_INVALID_ARGUMENT, detail_invalid_format);
    if (strlen(arg) >= USERNAME_MAX_LEN)
        return RESULT_ERR(ERR_INVALID_ARGUMENT, detail_exceeds_max_length);
    for (int i = 0; i <= *maxfd; i++) {
        if (strcmp(arg, clients[i].username) == 0)
            return RESULT_ERR(ERR_INVALID_ARGUMENT, detail_username_taken);
    }
    return ERR_OK;
}

int is_valid_username(const char *arg)
{
    while (*arg) {
        if (!isalnum((unsigned char)*arg) && *arg != '_')
            return 0;
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

    if (bind(server_fd, (struct sockaddr *) &address,
        sizeof(address)) == -1)
    {
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
