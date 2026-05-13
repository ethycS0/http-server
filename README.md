# HTTP Server in C from scratch

This is a multithreaded, autoscaling HTTP server written entirely from scratch in C using standard POSIX APIs. I built this project to practice raw C programming, deepen my understanding of systems architecture, and deliberately remove bad coding habits formed by over-reliance on AI and LLMs.

## Features

- **Written from Scratch:** Built without external web frameworks, relying strictly on standard C libraries and POSIX sockets.
- **Multithreaded Architecture:** Implements a custom thread pool to handle concurrent client connections efficiently without spawning a new thread for every request.
- **Autoscaling Thread Pool:** A dedicated monitor thread actively checks the queue load and scales the worker threads dynamically. It provisions more threads under heavy load and spins them down during idle periods to conserve resources.
- **Thread-Safe Task Queue:** Utilizes a custom queue managed by mutexes and condition variables to safely distribute incoming socket connections from the main listener to the worker threads.

## Prerequisites

To build and run this project, your system needs:

- A C compiler (`gcc`) and C++ compiler (`g++` for the test suite)
- GNU Make
- POSIX threads (`pthread`)
- Google Test (`gtest`) for compiling and running the test suite

_Note: A `flake.nix` and `.envrc` are provided. If you use Nix, you can drop directly into a fully configured development shell containing all required dependencies._

## Building and Running

1. Compile the server and its object files:

```bash
make

```

2. Execute the compiled binary:

```bash
./main.elf

```

By default, the server will bind to `0.0.0.0` and start listening on port `6969`. You can verify it is working by opening a web browser and navigating to `http://localhost:6969` to view the server dashboard.
