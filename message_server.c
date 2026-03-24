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

struct client {
    int authenticated;
    char buffer[MAX_BUFF_SIZE];
    int buffer_len;
    char username[USERNAME_MAX_LEN];
};

// Function prototypes
int  can_add_username(
    char *arg, int *maxfd,
    int c, struct client *clients
);
void handle_client(
    int c, int s, int *maxfd,
    fd_set *main, struct client *clients
);
void handle_close_socket(
    int bytes_received, int c,
    struct client *clients, fd_set *main,
    int *maxfd, int s
);
void handle_new_socket(int s, fd_set *main, int *maxfd);
void handle_send_message(
    char *arg, int *maxfd,
    int c, struct client *clients);
void initialize_clients(struct client *c);
int  is_valid_username(char *arg);
void process_input(
    int c, int *maxfd,
    char *read_line, struct client *clients
);
void reset_client(int c, struct client *clients);
void setup_server(int *server);
void split_input(char *buffer, char **cmd, char **arg);
char *trim_arg(char *s);
void update_maxfd(int s, int *maxfd, fd_set *main);
int  validate_input(char **cmd, char **arg);

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
        if (
            select(
                maxfd + 1,
                &readfds,
                NULL, NULL, NULL
            ) == -1
        ) {
            printf("Error: Select encountered an error.\n");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &readfds)) {
                if (i == server)
                    handle_new_socket(i, &main, &maxfd);
                else
                    handle_client(
                        i,
                        server, 
                        &maxfd,
                        &main,
                        clients
                    );
            }
        }
    }

    exit(EXIT_SUCCESS);
}

/**
 * Function: initialize_clients
 * ------------------------------------
 * Purpose: Set all possible clients as unauthenticated and
 * empty usernames
 * Returns: void
 */
void initialize_clients(struct client *c) {
    for (int i = 0; i < FD_SETSIZE; i++) {
        c[i].authenticated = 0;
        c[i].buffer_len = 0;
        c[i].username[0] = '\0';
    }
}

/**
 * Function: handle_client
 * ------------------------------------
 * Purpose: Handle operations coming in from clients and
 * run commands based on command given as input if it
 * matches one of the macros for commands. Also closes
 * client socket and handles cleaning the clients array.
 * Returns: void
 */
void handle_client(
    int c, int s, int *maxfd, fd_set *main, struct client *clients
) {
    char temp_buff[MAX_BUFF_SIZE];
    ssize_t bytes_received = recv(
        c,
        temp_buff,
        sizeof(temp_buff) - 1,
        0
    );

    // Socket was either closed or encountered an error
    if (bytes_received <= 0) {
        handle_close_socket(
            bytes_received, c,
            clients, main,
            maxfd, s
        );
        return;
    }

    // Too many characters entered to fit into client buffer
    if (clients[c].buffer_len + bytes_received > MAX_BUFF_SIZE) {
        printf("Error: too many characters entered.\n");
        return;
    }
    
    // Socket receieved input with no error.
    // Copy receieved input into client buffer
    // and process it.
    memcpy(
        clients[c].buffer + clients[c].buffer_len,
        temp_buff,
        bytes_received
    );
    clients[c].buffer_len += bytes_received;
    char *buf = clients[c].buffer;

    for (int i = 0; i < clients[c].buffer_len; i++) {
        if (buf[i] == '\n') {
            if (i >= MAX_LINE_SIZE) {
                printf("Error: User input is too long.\n");
            } else {
                char read_line[MAX_LINE_SIZE];
                memcpy(read_line, buf, i);
                read_line[i] = '\0';

                process_input(c, maxfd, read_line, clients);
            }

            // Move client buffer pointer to position
            // after the \n.
            memmove(
                buf, buf + i + 1,
                clients[c].buffer_len - (i + 1)
            );
            clients[c].buffer_len -= (i + 1);
            // Trick, after loop iteration is over, i++
            // runs and sets i back to 0.
            i = -1;
        }
    }
}

/**
 * Function: process_input
 * ------------------------------------
 * Purpose: Takes the line read from the client buffer and
 * does operations to split, validate line, and determine the
 * command from user. If it's a valid command, it runs the
 * operations associated with that command. Otherwise it prints
 * an error.
 * Returns: void
 */
void process_input(
    int c, int *maxfd,
    char *read_line, struct client *clients
) {
    char *cmd, *arg;
    split_input(read_line, &cmd, &arg);
    if (!validate_input(&cmd, &arg))
        return;
        
    // Valid command and argument, handle command
    if (strcmp(cmd, AUTH) == 0) {
        if (can_add_username(arg, maxfd, c, clients)) {
            clients[c].authenticated = 1;
            strncpy(
                clients[c].username,
                arg,
                USERNAME_MAX_LEN - 1
            );
            clients[c].username[USERNAME_MAX_LEN - 1] = '\0';
            printf(
                "OK: %s is authenticated.\n",
                clients[c].username
            );
        }
    } else if (strcmp(cmd, SEND) == 0) {
        if (clients[c].authenticated)
            handle_send_message(arg, maxfd, c, clients);
        else
            printf("Need to authenticate to send message.\n");
    } else
        printf("Error: command not recognized.\n");
}

/**
 * Function: handle_send_message
 * ------------------------------------
 * Purpose: Handle operations to extract username and
 * message to send to specified user.
 * Returns: void
 */
void handle_send_message(
    char *arg, int *maxfd,
    int c, struct client *clients
) {
    char *receiver, *message;
    split_input(arg, &receiver, &message);
    if (!validate_input(&receiver, &message))
    return;
    
    // Refuse sending message to self
    if (strcmp(clients[c].username, receiver) == 0) {
        printf("Error: can't send message to self.\n");
        return;
    }

    char full_message[MAX_BUFF_SIZE];
    snprintf(
        full_message,
        sizeof(full_message),
        "[%s] %s\n",
        clients[c].username, message
    );

    int found = 0;
    for (int i = 0; i <= *maxfd; i++) {
        if (clients[i].authenticated &&
            strcmp(
            clients[i].username,
            receiver) == 0
        ) {
            if (send(
                    i, full_message,
                    strlen(full_message), 0) < 0
            )
                printf("Error: couldn't send message.\n");
            found = 1;
            break;
        }
    }
    
    if (!found) {
        printf("Error: no user found.\n");
    }
}

/**
 * Function: handle_close_socket
 * ------------------------------------
 * Purpose: Handle operations to close a socket connected to
 * a client. Prints message based on if client was
 * authenticated or not, or if it was closed because of an
 * error.
 * Returns: void
 */
void handle_close_socket(
    int bytes_received,
    int c,
    struct client *clients,
    fd_set *main,
    int *maxfd,
    int s
) {
    if (bytes_received == 0) {
        if (clients[c].authenticated) {
            printf("Client '%s' disconnected (socket %d)\n",
                clients[c].username, c);
        } else {
            printf(
                "Unauthenticated client disconnected (socket %d)\n",
                c
            );
        }
    } else {
        printf("Error: Couldn't receieve stream on socket %d\n", c);
    }
    close(c);
    reset_client(c, clients);
    FD_CLR(c, main);
    if (c == *maxfd)
        update_maxfd(s, maxfd, main);
}

/**
 * Function: reset_client
 * ------------------------------------
 * Purpose: Resets the authenticated and username variables
 * for a specific client, typically when closing the socket.
 * Returns: void
 */
void reset_client(int c, struct client *clients) {
    clients[c].authenticated = 0;
    clients[c].buffer_len = 0;
    clients[c].username[0] = '\0';
}

/**
 * Function: can_add_username
 * ------------------------------------
 * Purpose: Once the input is split into command and
 * argument, validate that the argument(username) is a
 * certain length, valid chars, and is not taken or resetting
 * an already authenticated client.
 * Returns: 0 when false and 1 when true
 */
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

/**
 * Function: is_valid_username
 * ------------------------------------
 * Purpose: Loops through argument(username) and ensures
 * it contains only valid characters for usernames - alphanumeric
 * chars or underscores.
 * Returns: 0 when false and 1 when true
 */
int is_valid_username(char *arg) {
    while (*arg) {
        if (!isalnum((unsigned char)*arg) && *arg != '_') {
            return 0;
        }
        arg++;
    }
    return 1;
}

/**
 * Function: split_input
 * ------------------------------------
 * Purpose: Takens an input string from client and tries
 * to search for a space char to separate the command from
 * the argument.
 * Returns: void
 */
void split_input(char *buffer, char **cmd, char **arg) {
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

/**
 * Function: validate_input
 * ------------------------------------
 * Purpose: Trims the command and argument before checking
 * if either is not set.
 * Returns: 0 (false) if either is not set or 1 (true) if set
 */
int validate_input(char **cmd, char **arg) {
    *cmd = trim_arg(*cmd);
    *arg = trim_arg(*arg);
    if (*cmd == NULL || **cmd == '\0') {
        printf("Error: command not provided.\n");
        return 0;
    }
    if (*arg == NULL || **arg == '\0') {
        printf("Error: argument not provided.\n");
        return 0;
    }
    return 1;
}

/**
 * Function: trim_arg
 * ------------------------------------
 * Purpose: Trim the leading and trailing white spaces from
 * a string.
 * Returns: pointer to the trimmed string
 */
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

/**
 * Function: handle_new_socket
 * ------------------------------------
 * Purpose: Connects new socket connects using accept()
 * system call. Prints error if not successful.
 * Returns: void
 */
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

/**
 * Function: setup_server
 * ------------------------------------
 * Purpose: Sets up a server socket to accept client sockets
 * to connect to.
 * Returns: void
 */
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

/**
 * Function: update_maxfd
 * ------------------------------------
 * Purpose: Iterate through the max length of fd_set and check
 * if index of iteration is an active client and greater than
 * the last.
 * Returns: void
 */
void update_maxfd(int s, int *maxfd, fd_set *main) {
    int new_maxfd = s;
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, main) && i > new_maxfd)
            new_maxfd = i;
    }
    *maxfd = new_maxfd;
}
