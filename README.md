# concurrent-game-server
Concurrent multiplayer Linux game server developed using C++, POSIX threads, shared memory, IPC, and process synchronization.

## Features
- Hybrid concurrency model using fork() and pthreads
- Shared memory synchronization
- Named pipe (FIFO) IPC communication
- Round Robin scheduler
- Concurrent logger thread
- Persistent score storage

## Technologies
- C++
- Linux / Kali Linux
- POSIX Threads
- Shared Memory
- FIFO Pipes
- Mutex Synchronization

## Compile
make

## Run
./server
./client
