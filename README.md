# Distributed File System

A robust, C-based Distributed File System (DFS) inspired by architectures like HDFS. The project provides a highly scalable and fault-tolerant environment for distributed file storage, access control, and concurrent modifications across multiple storage nodes.

## Table of Contents
- [Overview](#overview)
- [Key Features](#key-features)
- [Project Structure](#project-structure)
- [Prerequisites](#prerequisites)
- [Installation & Setup](#installation--setup)
- [Usage Instructions](#usage-instructions)
  - [1. Start the Name Server](#1-start-the-name-server)
  - [2. Start Storage Server(s)](#2-start-storage-servers)
  - [3. Start the Client](#3-start-the-client)
- [Client Commands](#client-commands)

---

## Overview

The system is composed of three main architectural components:

1. **Name Server (NS)**: Acts as the central coordinator (similar to a NameNode). It registers and monitors Storage Servers and Clients, manages file metadata, handles Access Control Lists (ACLs), and routes client requests to the appropriate Storage Server. It utilizes an LRU cache and a Trie data structure for fast file lookups.
2. **Storage Server (SS)**: Responsible for actually storing the file data. It processes read, write, and stream requests directly from clients after they are authorized by the Name Server. It also supports replication, concurrency locking, and undo operations.
3. **Client**: Provides an interactive shell for users to interact with the DFS. Clients first talk to the Name Server for metadata and authorization, and then connect directly to the Storage Servers for data transfer.

## Key Features

- **Efficient Lookups**: Uses a Trie for fast file routing and an LRU cache to speed up repeated metadata queries.
- **Access Control (ACL)**: File owners can grant or revoke `READ` and `WRITE` permissions to other users.
- **Concurrency & Locking**: Supports concurrent reads and safely locks files during write operations to prevent data races.
- **Replication & Fault Tolerance**: Storage servers replicate data to ensure availability in case of node failures.
- **Advanced File Operations**: Supports streaming large files chunks, real-time read/write, and undoing previous changes.
- **Auto-Save & Persistence**: State and metadata are periodically auto-saved to disk to prevent data loss.

## Project Structure

- `client/` - Client application source code and headers.
- `name_server/` - Name Server implementation, routing, ACL, and caching.
- `storage_server/` - Storage Server implementation, file I/O, locking, and replication.
- `common/` - Shared networking protocols, error codes, and logging utilities.
- `Makefile` - Root build script to compile all components.

---

## Prerequisites

- A POSIX-compliant operating system (Linux, macOS, WSL).
- **GCC** compiler.
- **Make** build system.

---

## Installation & Setup

1. **Clone the repository:**
   ```bash
   git clone <repository_url>
   cd OSN-WalNet-Kernel
   ```

2. **Build the project:**
   Compile the entire codebase using the provided Makefile. This will create the executable binaries in the `bin/` directory.
   ```bash
   make all
   ```
   *(To clean up build artifacts later, you can run `make clean` or `make distclean`)*

---

## Usage Instructions

To run the system locally, you will need to open multiple terminal windows: one for the Name Server, one or more for Storage Servers, and one or more for Clients.

### 1. Start the Name Server
The Name Server must be started first as all other components register with it.
```bash
./bin/name_server
```
*Note the IP address it binds to (usually `127.0.0.1` for local testing) and its default port.*

### 2. Start Storage Server(s)
Next, spin up one or more Storage Servers.
```bash
./bin/storage_server <nameserver_ip> <port_to_assign_to_storageserver> <storage_server_id>
```
**Example:**
```bash
./bin/storage_server 127.0.0.1 8081 1
```

### 3. Start the Client
Finally, start a Client instance to interact with the DFS.
```bash
./bin/client <nameserver_ip>
```
**Example:**
```bash
./bin/client 127.0.0.1
```
Upon starting, you will be prompted to enter a username. Once connected, you will drop into an interactive prompt where you can execute commands.

---

## Client Commands

Inside the client interface, you can use the following commands to interact with the file system:

- **`VIEW [-a] [-l] [-al]`** - List files. `-a` shows all files, `-l` shows detailed metadata.
- **`LIST`** - List all registered users in the system.
- **`READ <filename>`** - Display the contents of a file.
- **`INFO <filename>`** - Show metadata and permission info for a specific file.
- **`HELP`** - Show the list of all available commands and their usage.
- **`ADDACCESS -R <file> <user>`** - Grant READ access to another user (must be file owner).
- **`ADDACCESS -W <file> <user>`** - Grant WRITE access to another user (must be file owner).
- **`REMACCESS <file> <user>`** - Remove all access permissions for a user on a file.
- **`EXEC <filename>`** - Execute a script file stored in the DFS.
- **`CREATE <filename>`** - Create a new empty file.
- **`DELETE <filename>`** - Delete a file you own.
- **`WRITE <file> <sentence_idx>`** - Start a write session at a specific sentence index. Commands during a write session include `WORD <index> <word>`, `COMMIT`, and `ROLLBACK`.
- **`UNDO <filename>`** - Undo the last committed change to a file.
- **`STREAM <filename>`** - Stream file contents with live updates.
- **`EXIT`** - Quit the client application.

## Contributors
* [@phanindragollapalli](https://github.com/phanindragollapalli)
* [@YVKartikeya9](https://github.com/YVKartikeya9)
