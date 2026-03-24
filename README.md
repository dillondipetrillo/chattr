# C Socket Chat Server

A multi-client, event-driven chat server written in C using `select()`, supporting user authentication and real-time messaging between connected clients.

This project focuses on low-level networking, system design, and protocol development.

## Features
*  Multi-client support using `select()`
*  Per-client state management
*  Username authentication (`AUTH`)
*  Real-time messaging between users (`SEND`)
*  Custom input buffering and message framing (`\n`-delimited)
*  Graceful client disconnect handling
*  Input validation (usernames, commands, buffer limits)

## Architecture Overview

This server is built around an **event-driven model**:

* Uses `select()` to monitor multiple sockets
* Maintains a `struct client` per file descriptor
* Each client has:
    * Authentication state
    * Username
    * Input buffer for partial reads

### Core Concept: Message Framing

Since TCP is a stream protocol, messages are not guaranteed to arrive in full.

This server:

* Accumulates incoming bytes into a per-client buffer
* Processes input only when a newline (`\n`) is detected
* Handles:
    * Partial messages
    * Multiple messages in a single read

## Supported Commands
### 1. Authenticate User

```AUTH <username>\n```

Example:

```AUTH dillon```

Behavior:
* Assigns a unique username to the client
* Rejects:
    * Duplicate usernames
    * Invalid characters
    * Already authenticated clients

### 2. Send Message

```SEND <username> <message>\n```

Example:

```SEND user123 hello there```

Behavior:

* Sends a message to another authenticated client
* Output on receiver’s terminal:
```[dillon] hello there```

* Rejects:
    * Sending to self
    * Non-existent users
    * Unauthenticated sender

## How to Run
1. Compile
```gcc -o server message_server.c```

2. Start Server
```./server```

3. Connect Clients (in separate terminals)
```nc localhost 8080```

## Key Implementation Details
### Per-Client Buffering

Each client has:

```
char buffer[MAX_BUFF_SIZE];
int buffer_len;
```

Incoming data is:

* Appended via `memcpy`
* Parsed when `\n` is detected
* Shifted using `memmove` after processing

### Command Parsing

Input is split into:

* `cmd` (command)
* `arg` (arguments)

Then validated and routed:

```
if (strcmp(cmd, AUTH) == 0) { ... }
else if (strcmp(cmd, SEND) == 0) { ... }
```

### Message Routing

Messages are:

* Constructed using `snprintf`
* Delivered using `send()` to the recipient’s socket