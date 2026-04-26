# State Bus (Flux): The Real-Time Lifecycle Engine

The State Bus is a high-performance, binary-protocol "Dumb Engine" designed for ultra-low latency state synchronization. Built in C, it serves as the foundational nervous system for collaborative ecosystems that require instantaneous updates and ephemeral data management.

## The Philosophy: "Dumb" for Speed
Unlike traditional message brokers (Kafka, RabbitMQ) that focus on persistence and heavy processing, the **State Bus** focuses on **Routing and Decay**.
- **Mechanism over Policy:** The server is application-agnostic. It doesn't interpret your data; it move bytes based on ```scope_id``` and ```packet_type```.
- **Hardware-Level TTL:** Native support for Time-To-Live (TTL) functionality, ensuring data expires at the protocol level rather than during background database cleanup.
- **Zero-Copy Mentality:** Designed to handle high-concurrency bursts (500+ packets) with minimal CPU jitter.

## Features
- **Binary Protocol:** Packed C structs for minimal bandwidth overhead.
- **Strict Byte-Ordering:** Big-Endian enforcement (`htonl`, `htobe64`) for cross-platform compatibility (Mobile/Web/Desktop).
- **Non-Blocking I/O:** Powered by a high-efficiency `select()` loop.
- **Identity Gate:** Server-side authority for `sender_id` to prevent identity spoofing.
- **Stress Tested:** Successfully handles 500+ packet bursts without sync loss.
- **Scope-Based Routing:** Logical isolation of data (Rooms/Channels) via ```scope_id```.

## Project Structure
```
.
├── include/           # Header files (The Public API)
│   ├── protocol.h     # Binary packet definitions & constants
│   ├── logger.h       # System logging interface
│   └── utils.h        # Networking wrappers
├── src/               # Implementation files
│   ├── server.c       # Core State Bus Router
│   ├── client.c       # Reference Test Client
│   ├── logger.c       # Thread-safe logging logic
│   └── utils.c        # Byte-handling logic
├── Makefile           # Automated build system
└── README.md          # System documentation
```

## Quick Start
**1. Build the Ecosystem**
The project uses a ```Makefile``` to handle complex linking and flags:
```
make clean && make
```

**2. Launch the Bus (Server)**
Start the central router. By default, it listens on port ```8080```.
```
./server
```

**3. Connect a Node (Client):**
Open multiple terminals to simulate real-time collaboration:
```
./client
```
