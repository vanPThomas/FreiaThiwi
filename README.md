# 𐍆𐍂𐌴𐌹𐌰 𐌸𐌹𐍅𐌹 — Freia Thiwi Server

**freia thiwi — “servant of freedom”**
**Version: 0.6.0 (development release)**

The lightweight C++17 TCP server for **Freia Thiwi**, a self-hosted encrypted messaging project. It relays the main chat, authenticates accounts, and manages persistent password-protected chatrooms without decrypting their message bodies.

> *“The server serves the users. The messages belong to the users.”*

Freia Thiwi is built with privacy, modest hardware requirements, and hands-on systems programming in mind. This is the **server** repository. The companion repository is [vanPThomas/FreiaThiwiClient](https://github.com/vanPThomas/FreiaThiwiClient).

## What's included in 0.6.0

- TCP server with `select()`-based connection handling and configurable port, password, and client limit.
- Account creation and login with SQLite-backed credentials.
- Persistent room metadata, recorded room memberships, and room message history.
- Server-side verification of room join credentials; the encryption key itself is not sent in the room join request.
- Live room presence calculated from current sockets, not saved as permanent online status.
- Per-room routing of messages and presence updates.

This release completes the initial **`PROT5` chatroom workflow** (creation, joining, messaging, live member lists, leaving, and SQLite persistence). Server and client **v0.6.0 should be upgraded together**: the room protocol changed from previous development versions.

## Performance on older hardware

Freia Thiwi is intended for small, independently hosted groups, including on repurposed hardware. The unmodified v0.6.0 server was tested on **Hazuki**, a 2009 Dell Inspiron 1545 with a single-core 2.20 GHz Celeron 900, 4 GB DDR2 and a mechanical HDD (no AES-NI).

| Workload / measurement | Result |
|---|---:|
| Server executable (unstripped Release build) | 154 KiB |
| Idle server RSS | 9.02 MiB |
| 2 users, manual messaging (20 s) | 0.10% average CPU; 9.38 MiB RSS |
| 20 active users, one room (10 min) | **240,000 verified deliveries; 0 missing** |
| 20-user server CPU / peak RSS / p95 peer latency | **2.89% / 11.38 MiB / 14.83 ms** |
| 50 active users, one room (60 s) | 150,000 verified deliveries; 0 missing; p95 peer latency 3.67 s |

The automated runs used 256-byte message bodies at one submission per second per user; delivery totals include sender echoes. The 50-user run passed delivery checks but exhibited multi-second tail latency. These are workload-specific LAN results, not minimum hardware requirements or maximum capacity guarantees.

**[Read the full benchmark report → BENCHMARKS.md](BENCHMARKS.md)** for hardware details, all measurements, methodology and limitations.

## How the system works

Three independent passwords have different roles:

| Password | Purpose | Server access |
|---|---|---|
| Server password | Derives the transport key for all framed packets | Server and clients share it |
| Account password | Derives account credentials for `PROT4` registration/login | Server stores a Base64-encoded derived key for comparison; not the typed password |
| Main-chat / room password | Derives client-side keys to encrypt chat content | Server relays content without the message-decryption key; each room has its own password |

These mechanisms are **not equivalent to a formally verified or audited secure messenger**; read [Security status](#security-status) before relying on the program for sensitive communications.

### TCP packet framing

Each TCP packet consists of a **4-byte network-order unsigned length** followed by that many bytes of transport-encrypted data. After transport decryption the frame starts with a protocol name; most fields are newline-separated. The server has a 64 KiB transport-packet limit in v0.6.0.

### Protocols

| Protocol | Direction | Function |
|---|---|---|
| `PROT1` | Client → server → other clients | Main/public-chat messages; body separately encrypted by sender |
| `PROT2` | Client ↔ server | Server-password transport handshake and display-name claim |
| `PROT3` | Server → client(s) | Server-wide presence, room-name lists, and notices |
| `PROT4` | Client ↔ server | Account `CREATE` / `LOGIN`; `SUCCESS` / `FAIL` |
| `PROT5` | Client ↔ server | Persistent password-protected chatrooms |

A typical session is: encrypted `PROT2` handshake → `PROT4` account authentication → `PROT3` online-user and room lists → optional `PROT1` main chat or `PROT5` room actions. A `PROT2` display-name claim alone is **not** a logged-in account; the server requires successful `PROT4` authentication before room operations.

### PROT5 chatrooms

Client requests (inside the transport-encrypted frame):

```text
PROT5
CREATE|CONNECT
<room_name>
<username>
<room_login_token_base64>
```

```text
PROT5
LEAVE
<room_name>
```

```text
PROT5
MESSAGE
<room_name>
<message_ciphertext_base64>
```

Server responses/events include:

| Response | Purpose |
|---|---|
| `PROT5 ROOM` | Room metadata and the initial online-user snapshot; sent on successful join |
| `PROT5 USERS` | Complete replacement online-user list (`room_name`, count, usernames) |
| `PROT5 MESSAGE` | Room-only ciphertext broadcast; also used to replay recent stored history |
| `PROT5 LEFT` | Confirms leaving a room |
| `PROT5 ERROR` | Reports a room-specific error (including wrong password) |

Creating a room automatically joins its creator. `ROOM` is sent before history and `USERS` updates. Messages are Base64-encoded *inside* the newline-delimited protocol so encrypted binary data cannot break parsing. Messages are routed only to currently joined sockets; disconnecting removes the socket from live room membership, and remaining participants receive updated user lists. Joining again requires the room password.

### Server persistence

`accounts.db` contains the existing `accounts` table and these room tables:

| Table | Purpose |
|---|---|
| `rooms` | Name, creator, creation timestamp, random password-verifier salt, and derived room-login verifier |
| `room_members` | Records the username and first-join time for accounts that have joined rooms |
| `room_messages` | Sender, timestamp, room name, and **Base64-encoded client-encrypted message body** |

The server reloads rooms and recent history after restart. **Online room users are not persisted**: the server determines them from live joined sockets. The most recent **100 messages** per room are sent to a client after it joins; older messages may remain in the database.

Room password verification uses a client-generated HMAC-based room-login token, verified against a salted PBKDF2-HMAC-SHA256 value in SQLite. The room AES key is not handed to the server for verification; however, room names, creators, sender identities, timestamps, and membership are visible to the server.

## Build and run (Linux)

**Dependencies:** CMake 3.16+, a C++17 compiler, OpenSSL development headers, and SQLite3 development headers.

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake git libssl-dev libsqlite3-dev
```

On Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake git openssl sqlite
```

```bash
git clone https://github.com/vanPThomas/FreiaThiwi.git
cd FreiaThiwi
cmake -S . -B build
cmake --build build -j
cd bin
./freia-thiwi
```

At startup, enter a port (1024–65535), the maximum number of clients, and the server password. The server listens on all network interfaces. Only expose it to networks you trust while security hardening remains unfinished.

The SQLite database file `accounts.db` is created in the **working directory** from which you start the server, not necessarily next to the executable. Reuse the same working directory across restarts to preserve accounts and rooms. Back up the database (and its WAL sidecars if present), preferably with the server stopped, before upgrades.

## Security status

**v0.6.0 is an experimental/development release, not a security-audited product.**

- Transport and chat-body encryption currently use **AES-256-CBC**. CBC alone does **not** authenticate ciphertext or prevent active tampering; authenticated encryption (for example, AES-GCM) is an important future upgrade.
- Password-to-key derivation currently uses PBKDF2-HMAC-SHA256 with a fixed/empty salt for the general client/server keys. The account verifier in `accounts.db` is a reusable derived value, not a modern salted server-side password hash.
- Room verifiers are salted, but human-chosen room passwords can still be weak and subject to guessing attacks. Room metadata and the existence of messages are visible to the server.
- The server password is shared by all clients of a server. The present transport design is not a substitute for a fully authenticated end-to-end key exchange or a security review.
- **Ephemeral public/private-key-based secure password/invitation exchange is planned, not implemented.** Do not assume keys are exchanged securely merely because message bodies are encrypted once everyone knows the same password.

Avoid using Freia Thiwi to protect high-risk communications until these security limitations have been addressed and independently reviewed.

## Roadmap (not yet implemented)

- Authenticated encryption and protocol/security review.
- Authenticated secure exchange of room passwords/invitations using ephemeral key material (and a design for verifying recipient identities).
- Additional usability improvements, testing, and reliability hardening.
- Potential file sharing and further messaging features.

The goal is a **small, useful, maintainable messenger**, not an endlessly expanding list of prerequisites for 1.0.

## Project name and purpose

Freia Thiwi is Gothic: **𐍆𐍂𐌴𐌹𐌰** (*freia*, free) and **𐌸𐌹𐍅𐌹** (*thiwi*, servant/helper). *The servant of freedom.*

The project is intended to make independent communication more accessible, including on older and inexpensive computers. Contributions, testing, and constructive security feedback are welcome.

See [CHANGELOG.md](CHANGELOG.md) for the release history. Licensed under the terms in [LICENSE](LICENSE).
