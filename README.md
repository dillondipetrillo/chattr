# State Bus C-Engine 

A high-performance, binary-protocol "Dumb Engine" designed for real-time state synchronization. Built in C to provide a low-latency nervous system for collaborative workspaces and ephemeral social applications.

## The Philosophy: "The Dumb Engine"
Unlike traditional message brokers (Kafka/AWS Event Bridge) that focus on storage and heavy processing, the **State Bus** focuses on **Routing and Decay**. 
- **Mechanism over Policy:** The server doesn't care what the data is; it only cares where it needs to go and when it expires.
- **Hardware-Level TTL:** Native support for Time-To-Live (TTL) functionality, ensuring data expires at the protocol level rather than during background database cleanup.

## Features
- **Binary Protocol:** Packed C structs for minimal bandwidth overhead.
- **Strict Byte-Ordering:** Big-Endian enforcement (`htonl`, `htobe64`) for cross-platform compatibility (Mobile/Web/Desktop).
- **Non-Blocking I/O:** Powered by a high-efficiency `select()` loop.
- **Identity Gate:** Server-side authority for `sender_id` to prevent identity spoofing.
- **Stress Tested:** Successfully handles 500+ packet bursts without sync loss.

## Project Structure
- `server.c`: The central State Bus router.
- `client.c`: Terminal-based test client.
- `protocol.h`: The shared binary protocol definition.
- `utils.c/h`: Core networking and byte-handling wrappers.

## Quick Start
Currently, this project uses a flat directory structure.

1. **Compile the Server:**
   `gcc -Wall -Wextra server.c utils.c -o server`

2. **Compile the Client:**
   `gcc -Wall -Wextra client.c utils.c -o client`

3. **Run:**
   - Terminal A: `./server`
   - Terminal B: `./client` (Receiver)
   - Terminal C: `./client` (Sender)
