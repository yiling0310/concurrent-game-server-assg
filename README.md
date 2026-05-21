# Multiplayer Tic-Tac-Toe Game (C++)

## Demo Video
https://youtu.be/0UzytMTj5yA

A multiplayer Tic-Tac-Toe game developed using:

- Shared Memory (`shm_open`, `mmap`)
- FIFO / Named Pipes (`mkfifo`)
- Multi-processing (`fork`)
- POSIX Threads (`pthread`)
- Process-shared Mutex Synchronization
- Circular Buffer Logging System
- Persistent Score Storage

---

# Features

- Multiplayer gameplay
- Shared memory communication
- FIFO-based client-server communication
- Turn scheduler with timeout system
- Persistent score saving
- Logging system with circular buffer
- Multiple game sessions without restarting server
- Symbol selection phase
- Win and draw detection
- Concurrent synchronization using mutex

---

# Requirements

- Linux / Ubuntu / Kali Linux
- `g++`
- POSIX Threads (`pthread`)

---

# Project Structure

```text
project/
│
├── server.cpp
├── client.cpp
├── README.md
├── game.log
└── scores.txt
```

---

# Compilation

## Compile Server

```bash
g++ server.cpp -o server -pthread
```

## Compile Client

```bash
g++ client.cpp -o client -pthread
```

---

# How to Run

## Step 1 — Open Project Folder

Example:

```bash
cd sf_share
```

Check files:

```bash
ls
```

Expected output:

```text
server.cpp
client.cpp
README.md
```

---

## Step 2 — Start Server

Open **Terminal 1**:

```bash
./server
```

Expected output:

```text
SERVER RUNNING. Waiting for clients...
```

---

## Step 3 — Start First Client (Create Lobby)

Open **Terminal 2**:

```bash
./client
```

Choose:

```text
INIT
```

Enter number of players.

Example:

```text
2
```

---

## Step 4 — Other Players Join

Open another terminal for each additional player:

```bash
./client
```

Choose:

```text
JOIN
```

---

# Gameplay Flow

## 1. Lobby Phase

- First player creates lobby
- Other players join
- Server waits until all required players connect

---

## 2. Symbol Selection Phase

Each player selects a unique alphabet symbol.

Example:

```text
Choose symbol: X
```

---

## 3. Gameplay Phase

Players take turns placing symbols on the board.

Example:

```text
Enter grid number: 5
```

---

# Turn Timeout System

- Each player has **20 seconds** per turn
- If timeout occurs:
  - Scheduler automatically skips the player
  - Turn moves to next player

Example server output:

```text
[Scheduler] Forced skip for Player 2
```

---

# Persistent Scores

Scores are stored in:

```text
scores.txt
```

Scores are:

- Loaded when server starts
- Saved when game ends
- Saved during shutdown

---

# Logging System

All important events are written into:

```text
game.log
```

Logged events include:

- Player connections
- Symbol selections
- Moves
- Turn changes
- Timeout events
- Winners
- Draws
- New game sessions

---

# Multiple Game Sessions

The server supports multiple game sessions without restarting.

After one game ends, players can create a new lobby again using:

```text
INIT
```

Implemented using:

```cpp
game_id
```

to prevent old child processes from interfering with new game sessions.

---

# IPC Mechanisms Used

| Mechanism          | Purpose                     |
|-------------------|-----------------------------|
| Shared Memory      | Shared game state           |
| FIFO / Named Pipe  | Client-server communication |
| Mutex              | Synchronization             |
| `fork()`           | Multi-process handling      |
| Threads            | Logger and scheduler        |

---

# Synchronization

The project uses:

```cpp
pthread_mutex_t
```

with:

```cpp
PTHREAD_PROCESS_SHARED
```

to synchronize access across processes.

Protected resources:

- Game board
- Shared game state
- Logging buffer
- Scores

---

# Shutdown

To safely shutdown the server:

```bash
Ctrl + C
```

The server will:

- Save scores
- Cleanup shared memory
- Remove FIFO pipe
- Exit safely

