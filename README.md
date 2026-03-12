# C socket server
A simple C socket server that accepts one client connection at a time and prints the message it's sent. 

## Features
* Accepts a single TCP client connection at a time.
* Prints client messages to the console.
* Easy to compile and run on Linux, macOS, and with minor adjustments, Windows.

## Requirements
* C compiler (e.g., `gcc`, `clang`, or MSVC for Windows)
* POSIX-compatible system for Linux/macOS (Windows users need `Winsock2` initialization

## Compilation
### Linux/macOS
`gcc message_server.c -o message_server`
### Windows (using MinGW)
`gcc message_server.c -o message_server.exe -lws2_32`

## Running the server
`./message_server`
The server will start and listen on port 8080

## Testing the server
You can send messages to the server using netcat(`nc`):
`nc localhost 8080`
Type the messages in the netcat terminal and press Enter - the server will display them.
> On Windows, you can use `telet localhost 8080` or install netcat for Windows.

## Notes
* The server currently handles one client at a time. Additional clients must wait until the current client disconnects.
* Messages are printed as-is, no special formatting.
