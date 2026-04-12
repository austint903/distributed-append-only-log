# Distributed Append-Only Log

A Kafka-inspired distributed append-only log written in C++20. Messages are produced via UDP or HTTP, durably written to
disk using Linux `io_uring` async I/O, and consumed through a CLI tool (dlog) or HTTP. The system supports multiple
independent topics, concurrent reads/writes, long-poll tail consumers, primary/replica replication, and a smart router
that partitions topics across replica groups — all coordinated through a lightweight HTTP control plane.

---

## Table of Contents

1. [What It Does](#what-it-does)
2. [Architecture](#architecture)
3. [How It Was Built](#how-it-was-built)
4. [Project Structure](#project-structure)
5. [Running the Project](#running-the-project)
6. [The dlog CLI](#the-dlog-cli)
7. [Running Tests](#running-tests)

---

## What It Does

- **Produce** messages to named topics over UDP or HTTP
- **Consume** messages by topic + sequence offset
- **Tail** a topic in real-time with long-polling (blocks until a new message arrives)
- **Multi-topic** support — each topic is an independent append-only log file
- **Concurrent writes/reads** — io_uring-backed appends with a mutex-protected in-memory index
- **Crash recovery** — on restart the server scans the log file and rebuilds its sequence index
- **CRC32 checksums** on each log record to detect corruption
- **Replication** — each primary node syncs writes to its replicas via HTTP
- **Router** — a stateless router partitions topics across 3 primary replica groups, with health tracking
- **`dlog` CLI** — a compiled binary you drop in your `PATH` to produce/consume from the terminal

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                         Client (Mac)                             │
│  dlog CLI  ──HTTP──►  Router :9090                               │
└──────────────────────────┬───────────────────────────────────────┘
                           │ partition assignment
          ┌────────────────┼─────────────────────┐
          ▼                ▼                      ▼
    Server-1 :8080   Server-2 :8081        Server-3 :8083
    (primary)        (primary)             (primary)
    UDP :9991        UDP :9992             UDP :9993
       │                │                     │
    ┌──┴──┐          ┌──┴──┐              ┌───┴──┐
   r1    r2         r1    r2             r1     r2
 (replica)(replica) (replica)(replica) (replica)(replica)
```

**Router** — receives all produce/consume/tail requests from the CLI. It maps each topic to a primary server, forwards
the request, and handles failover if a node is unhealthy.

**Server (node)** — each node runs:

- A **UDP server** for low-latency fire-and-forget writes (primarily for testing)
- An **HTTP server** (port 8080) for produce/consume/tail/replication control-plane requests
- An **AppendLog** per topic — each one is a flat binary file of length-prefixed records written via `io_uring`
- A **TopicRegistry** that lazily creates topics on first write
- A **NodeManager** that replicates every write to its replicas

**AppendLog** — core storage engine:

- Records are written as: `[4-byte length][payload][4-byte CRC32]`
- `io_uring` is used for async kernel-bypass I/O (Linux only)
- An in-memory `unordered_map<seq → file_offset>` index enables O(1) seeks by sequence number
- `wait_for_seq()` uses a condition variable so tail consumers block efficiently without polling

---

## How It Was Built

### Linux I/O on macOS with Docker

`io_uring` is a Linux-only syscall interface. Because development happens on macOS (Apple Silicon), the server runs
inside a Docker container (Ubuntu 22.04) while source files live on the Mac. Docker Compose mounts the project directory
into each container — no `COPY` step needed for dev — so CLion edits on the Mac are immediately visible inside
containers.

CLion's SSH toolchain connects to the container over port `2222` (mapped from the container's port 22), compiling and
running the server binary without leaving the IDE.

### Async I/O with io_uring

Rather than `write()` + `fsync()`, each record append submits a `IORING_OP_WRITE` to an io_uring submission queue and
synchronously waits for the completion entry. This avoids the overhead of traditional blocking syscalls and lays the
groundwork for batching multiple appends in flight simultaneously. The queue depth is currently 1 (one in-flight op at a
time), which gives durability semantics similar to a synchronous write but through the async path.

### Replication

Each primary node's HTTP server handles a `/replicate` endpoint. After a successful local append, `NodeManager` fans the
payload out to all replica URLs. Replicas apply the write identically so reads can be served by any node in the group.

### Router & Partition Map

The router uses a `PartitionMap` that assigns each topic to one of the replica groups. Assignments
are persisted to `data/router/partitions.json` so the mapping survives restarts. A background `HealthTracker` pings
every node's `/health` endpoint and marks unhealthy nodes so the router can skip them on reads.

---

## Project Structure

```
.
├── src/
│   ├── server/
│   │   ├── append_log/       # Core io_uring log engine
│   │   ├── http_server/      # HTTP produce/consume/tail/replicate API
│   │   ├── udp_server/       # UDP write path
│   │   ├── log_reader/       # Sequential record scanner
│   │   ├── topic_registry/   # Multi-topic management
│   │   └── node_manager/     # Replication to replicas
│   ├── router/
│   │   ├── partition_map/    # Topic → replica group assignment
│   │   ├── router_server/    # HTTP proxy + routing logic
│   │   ├── health_tracker/   # Node health polling
│   │   └── request_manager/  # Retry / failover logic
│   ├── protocol/             # Wire format definitions
│   └── util/
│       ├── util.{h,cpp}      # CRC32, record serialization
│       └── dlog/main.cpp     # CLI entry point
├── tools/producer/           # Standalone UDP producer binary
├── tests/                    # Unit + integration tests (GoogleTest)
├── infra/server/Dockerfile   # Ubuntu 22.04 + io_uring dev image
├── docker-compose.yml        # 3 primaries × 2 replicas + router
├── CMakeLists.txt
└── vendor/httplib.h          # Embedded cpp-httplib (header-only)
```

---

## Running the Project

### Prerequisites

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) (with Compose)
- [CLion](https://www.jetbrains.com/clion/) (recommended IDE)
- CMake ≥ 3.20

---

### Step 1 — Clone the repo

```bash
git clone <repo-url>
cd distributed-append-only-log
```

---

### Step 2 — Start the containers

```bash
docker compose up --build
```

This starts 9 server containers (3 primaries + 6 replicas) and 1 router container. The router listens on
`localhost:9090`.

> Containers mount the project directory at `/app` — no rebuild needed when you edit source files.

---

### Step 3 — Set up CLion SSH Toolchain (for compiling inside Docker)

CLion needs to compile and run inside the container since `io_uring` only works on Linux.

1. **Open** CLion → `Settings` → `Build, Execution, Deployment` → `Toolchains`
2. **Add** a new `Remote Host` toolchain
3. Set credentials:
    - **Host**: `localhost`
    - **Port**: `2222`
    - **User**: `root`
    - **Password**: `root`
4. CLion will auto-detect CMake, GCC, and GDB inside the container

---

### Step 4 — Configure CMake with the SSH Toolchain

1. Go to `Settings` → `Build, Execution, Deployment` → `CMake`
2. Add (or edit) a profile, e.g. named **`dev-server`**
3. Set **Toolchain** to the SSH toolchain you created above
4. Set **Build directory** to `cmake-build-dev-server`

CMake will configure and build inside the container. The compiled binaries land in `cmake-build-dev-server/` which is
volume-mounted back to the Mac.

---

### Step 5 — Build and run the server

From CLion, select the **`server`** run target and hit Run, or SSH into the container and run manually:

```bash
docker exec -it distributed_log_server_1 bash
/app/cmake-build-dev-server/server
```

The server will start its UDP listener (port 9999 inside container) and HTTP server (port 8080).

---

### Step 6 — Build `dlog` and add it to your shell

Build the `dlog` target in CLion (or via CMake), then add the binary to your PATH so you can call it from any terminal:

```bash
# Add to ~/.zshrc
export PATH="$PATH:/path/to/distributed-append-only-log/cmake-build-dev-server"
```

Verify it works:

```bash
dlog
# Usage:
#   dlog produce --topic <topic> <message>
#   dlog consume --topic <topic> --offset <offset>
#   dlog tail   --topic <topic> [--offset <offset>] [--timeout-ms <ms>]
#   dlog topics list
```

The `dlog` binary connects to `localhost:9090` (the router) by default.

---

## The `dlog` CLI

All commands route through the router at `localhost:9090`.

### `produce` — write a message to a topic

```bash
dlog produce --topic <topic> <message>
```

| Argument    | Required | Description                             |
|-------------|----------|-----------------------------------------|
| `--topic`   | yes      | Topic name to write to                  |
| `<message>` | yes      | Message body (last positional argument) |

**Example:**

```bash
dlog produce --topic events "user signed up"
# → 200 OK, assigned sequence number returned
```

---

### `consume` — read a single message by offset

```bash
dlog consume --topic <topic> --offset <offset>
```

| Argument   | Required | Description                         |
|------------|----------|-------------------------------------|
| `--topic`  | yes      | Topic to read from                  |
| `--offset` | yes      | Sequence number to read (0-indexed) |

**Example:**

```bash
dlog consume --topic events --offset 0
# → "user signed up"
```

---

### `tail` — stream new messages in real-time

```bash
dlog tail --topic <topic> [--offset <offset>] [--timeout-ms <ms>]
```

| Argument       | Required | Default | Description                        |
|----------------|----------|---------|------------------------------------|
| `--topic`      | yes      | —       | Topic to tail                      |
| `--offset`     | no       | `0`     | Starting sequence number           |
| `--timeout-ms` | no       | `30000` | Long-poll timeout per request (ms) |

Tail holds an open HTTP connection and blocks until a new message arrives on the server (long-poll). When a message
comes in, it prints it with its sequence number and immediately re-subscribes from the next offset. Press `Ctrl+C` to
stop.

**Example:**

```bash
dlog tail --topic events
# Tailing topic 'events' from offset 0... (Ctrl+C to stop)
# [seq=0] user signed up
# [seq=1] user clicked button
# ...
```

---

### `topics list` — list all known topics

```bash
dlog topics list
```

Returns a list of all topics that have at least one message on the node the router resolves to.

**Example:**

```bash
dlog topics list
# events
# orders
# metrics
```

---

## Running Tests

Tests use GoogleTest and are fetched autohow do

Individual test binaries:

| Binary                        | What it tests                             |
|-------------------------------|-------------------------------------------|
| `test_crc32`                  | CRC32 checksum correctness                |
| `test_scan_log`               | Log file record scanning                  |
| `test_protocol`               | Wire protocol encoding/decoding           |
| `test_producer`               | Producer send logic                       |
| `test_log_reader`             | Sequential log reader                     |
| `test_append_log_concurrency` | Concurrent appends under io_uring         |
| `test_topic_registry`         | Multi-topic creation and lookup           |
| `test_tail_consumer`          | Long-poll tail consumer blocking behavior |
| `test_router`                 | Partition map + routing decisions         |
| `test_health_tracker`         | Node health polling and failover          |
| `test_request_manager`        | Request retry and node selection          |
| `test_node_manager`           | Replication from primary to replicas      |
