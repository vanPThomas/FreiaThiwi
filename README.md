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

## Real hardware benchmark — Hazuki (2009)

Freia Thiwi is designed to make self-hosted messaging practical on modest hardware. This benchmark records the actual resource use of the full server release, not a reduced test build.

> **154 KiB server executable · 0.10% average server CPU during manual messaging · less than 10 MiB resident memory.**

On **26 November 2026**, the unmodified server release `v0.6.0` (commit `c1959c5`) was compiled natively and tested on Hazuki, a 2009 Dell Inspiron 1545. Two external clients authenticated, joined the same newly created password-protected room, and exchanged encrypted messages using the normal server and SQLite implementation. No compatibility patches or reduced-feature build were used.

### Test hardware and software

| Component | Configuration |
|---|---|
| System | Dell Inspiron 1545 (2009; 17 years old at test time), BIOS A07 (2009-05-13) |
| CPU | Intel Celeron 900 (Penryn), 2.20 GHz maximum, 1 core / 1 thread, x86-64 |
| CPU cache | 32 KiB L1 instruction + 32 KiB L1 data, 1 MiB L2 |
| Instruction support | No AES-NI or AVX; AES is executed without dedicated AES hardware instructions |
| RAM | 4 GB DDR2-800 (2 × 2 GB); 3.8 GiB usable, 2 GiB swap configured |
| Storage | Hitachi HTS54503-series mechanical SATA HDD, 320 GB nominal (298.1 GiB reported) |
| OS / kernel | Ubuntu 24.04.5 LTS; Linux `6.8.0-139-generic` |
| Build tools | GCC/G++ 13.3.0; CMake 3.28.3; Release build, `-j1` |
| Libraries | OpenSSL 3.0.13; SQLite 3.45.1 |
| Server executable | 154 KiB (rounded `ls -lh` output); 64-bit ELF PIE, dynamically linked, not stripped |

The machine had about 538 MiB of system memory in use before server startup, 3.3 GiB available, and no swap in use. The executable size does not include dynamically linked libraries or the database.

### Server memory and CPU measurements

| Workload | RSS | PSS | Private memory (USS) | CPU usage |
|---|---:|---:|---:|---|
| Idle, no clients | 9,240 KiB (9.02 MiB) | 3,079 KiB (3.01 MiB) | 1,412 KiB (1.38 MiB) | 0.0% in 10 `top` samples |
| Two authenticated clients | 9,708 KiB (9.48 MiB) | — | — | 0.0% lifetime average (`ps`) |
| One room, two clients, after exchanging messages | 9,712 KiB (9.48 MiB) | 3,264 KiB (3.19 MiB) | 1,564 KiB (1.53 MiB) | 0.1% lifetime average (`ps`) |
| 20-second manual busy-traffic test, two clients | 9,600 KiB (9.38 MiB) | — | — | **0.10% average; 1.00% highest one-second sample** (`pidstat`) |

The first three stages reported one server thread. Process swap was zero in both `smaps_rollup` snapshots. Between the idle and post-messaging snapshots, RSS increased by only 472 KiB, PSS by 185 KiB and private memory by 152 KiB. RSS includes shared library pages; PSS apportions shared pages across processes, and USS here is the sum of `Private_Clean` and `Private_Dirty`. A dash indicates a metric not measured at that stage.

### 20-second manual messaging test

Two authenticated clients exchanged repeated encrypted messages in the same room while the server process was sampled once per second (`pidstat -u -r -d -p <PID> 1 20`). The server processes transport encryption, room-specific delivery and SQLite writes of encrypted room-message bodies; room-content encryption and decryption take place on the clients.

| Metric | Observed result |
|---|---:|
| Average CPU (userspace + kernel) | **0.10%** (0.05% + 0.05%) |
| Highest one-second CPU sample | 1.00% |
| CPU samples reported as 0.00% | 18 of 20 |
| CPU scheduling wait | 0.00% average |
| RSS (minimum / maximum / average) | 9,600 / 9,600 / 9,600 KiB |
| Observed RSS growth | 0 KiB |
| Virtual memory (VSZ) | 13,832 KiB throughout |
| Minor / major page faults | 0.10 / 0.00 per second on average |
| Average process-attributed disk writes | 17.99 KiB/s |
| Highest one-second disk-write sample | 48 KiB/s |
| Approximate process-attributed writes | 360 KiB over the test |
| Process-attributed disk reads | 0 KiB/s |
| Reported process I/O delay | 0 (as reported by `pidstat`) |

### Reproducing the benchmark

Build the server from the release tag on Linux, using its normal dependencies:

```bash
git checkout v0.6.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j1
cd bin
./freia-thiwi
```

With one server instance running, use a separate terminal on the server host:

```bash
pid=$(pgrep -x freia-thiwi)
ps -p "$pid" -o pid,pcpu,pmem,rss,vsz,nlwp,etime
cat "/proc/$pid/smaps_rollup"
top -b -d 1 -n 10 -p "$pid"
pidstat -u -r -d -p "$pid" 1 20
```

`pidstat` is provided by the `sysstat` package. Use a dedicated test database and test accounts. Record the idle baseline first, then connect two clients from another machine, authenticate both, create/join a room, and exchange messages during the 20-second sampling window.

### Scope and next steps

These are measured **server-side** results for one machine and a two-client manual workload, not minimum system requirements, peak instantaneous CPU usage, or a maximum-capacity benchmark. The message count, message sizes, delivery latency and exact traffic rate were not recorded. The 20-second test does not establish long-term memory stability, storage latency or security. The graphical client's own CPU/RAM usage on Hazuki was not measured.

A reproducible automated stress test with controlled message rates, more concurrent clients, multiple rooms, reconnects and longer runs is planned. A 32-bit machine from around 2000 (Celeron-chan, 651 MHz, 384 MB RAM) is also planned as an experimental compatibility target; it has **not yet** run Freia Thiwi.

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
