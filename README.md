# 𐍆𐍂𐌴𐌹𐌰 𐌸𐌹𐍅𐌹  
**freia thiwi — "servant of freedom"**

A lightweight, end-to-end encrypted C++ chat client using ImGui and TCP sockets.
Designed for oppressed, restricted, and privacy-seeking users who deserve safe communication – even on obsolete hardware.

---

## Core Philosophy

> *“The server serves the users.  
> The messages belong to the users.”*

- No plaintext messages stored or readable on the server  
- Dual-layer encryption (Transport + E2EE)  
- Learning by building everything manually: sockets, protocol, crypto, routing

This project is not a wrapper around an existing library.  
It is a deep dive into systems programming, networking, and cryptography.

This repo is the server side of the Freia Thiwi Project,

---

## Features (planned / in progress)

Client side encryption with a shared private password for symmetrical encryption.
Server encryption to hide usernames and protocols.
Unique protocol system.
Accounts and users with client side encrypted user data stored on the server but unreadable by the server.
Password share feature using asymmetrical encryption using temporary public and private keys.
File sharing.
and much more to follow.

---

## Architecture Overview

- The server routes messages but cannot decrypt them.
- Clients encrypt/decrypt messages using a shared secret password.
- Future enhancement: asymmetric bootstrap to exchange passwords securely.

---

## Wire format (all PROTs)

Every packet on the TCP socket is:

1. **4-byte length prefix** — `uint32_t` in **network byte order** (`htonl` / `ntohl`), equal to the size of the following blob  
2. **Transport ciphertext** — that many bytes, encrypted with the **server password** key (`serverSessionKey` on the client, `serverKey` on the server)

Nobody sends plaintext frames on the wire.

After transport decrypt, the plaintext frame always starts with a type line:

```
PROT<n>
…
```

Fields are separated by `\n`. Unknown `PROT` types should be ignored (client already logs `[Unknown protocol]`). Do not reuse or reshape an old PROT to add features; add a new number.

### Keys

| Key | Derived from | Who can use it | Purpose |
|---|---|---|---|
| Transport | Server password | Client and server | Wrap every frame |
| Chat (E2EE) | Shared chat password | Clients only | Inner body of PROT1 |
| Account | Account password | Client + stored verifier on server | PROT4 create / login |

The server can read PROT2 / PROT3 / PROT4 and the *headers* of PROT1. It must not be able to read the PROT1 chat body.

---

### PROT2 — Handshake (connect)

**When:** First bytes after TCP connect. No chat until this succeeds.

**Client → server**

```
PROT2
<username>
```

**Server → that client**

```
PROT2
Welcome <username>!
```

**What it is for**

- Prove both sides share the server password (decrypt must work)
- Bind this socket to a display name (`socketToUsername`)
- After OK: server broadcasts `userJoined`, sends that client a `userList`, client may send PROT4, then starts the receive loop

**Failure:** bad length, decrypt fail, or first line not `PROT2` → socket closed. Client treats a reply that is not `PROT2` as auth failure.

---

### PROT4 — Accounts

**When:** Immediately after a successful PROT2, if the client has an account password. Optional in principle; current client always sends it when configured with an account.

**Client → server**

```
PROT4
CREATE|LOGIN
<username>
<accountKeyBase64>
```

`accountKeyBase64` is the derived account key, base64-encoded. The server stores / compares that verifier in SQLite (`accounts.db`). It does not receive the account password as typed.

**Server → that client**

```
PROT4
SUCCESS
<message>
```

```
PROT4
FAIL
<reason>
```

**What it is for**

- `CREATE` — insert username + key if the name is free  
- `LOGIN` — accept only if name exists and key matches  

On `FAIL`, the current client disconnects. On `SUCCESS`, the TCP session continues into normal chat.

This is **identity**, not E2EE. It does not replace the chat password.

---

### PROT1 — Main chat (E2EE relay)

**When:** User hits Send on the **Main Chatroom** tab (`ClientConnect::sendMessage`).

**Client builds (plaintext inside the transport wrap)**

```
PROT1
<username>
<innerCipherLength>
<innerCiphertext>
```

Steps on send:

1. Encrypt the typed text with the **chat** key → `innerCiphertext`  
2. Build the frame above (`innerCipherLength` = byte size of that blob)  
3. Encrypt the whole frame with the **server** key  
4. Send length prefix + transport ciphertext  
5. Local echo: `user: text`

**Server**

- Transport-decrypts  
- Checks it is PROT1 and that the length field is sane  
- **Forwards the original transport ciphertext** to every other connected socket  
- Does not decrypt `innerCiphertext`, does not store the message  

**Receiving client**

- Transport-decrypt  
- Parse user + length  
- Take the **last N bytes** of the frame as ciphertext (so the body may contain `\n`)  
- Chat-decrypt with the chat key  
- Append `username: text` to the main tab history  

**What it is for:** the global lobby channel only. Room tabs do **not** use PROT1 today.

---

### PROT3 — Presence / server notices

**When:** Server-originated. Clients do not send PROT3.

**Frame**

```
PROT3
<messageType>
<payload>
```

`payload` may itself contain extra `\n` lines (user lists).

| `messageType` | Payload | Who gets it | Client effect |
|---|---|---|---|
| `userJoined` | username | everyone | add to online set, system line |
| `userLeft` | username | everyone | remove from online set, system line |
| `userDisconnected` | text (e.g. `Name disconnected.`) | everyone | system line |
| `userList` | names, one per line | one client (usually the one who just joined) | replace online set |

**What it is for:** who is on the server, not chat text. Encrypted only with the transport key, which is correct — the server must build these frames.

---

### Not a PROT (yet)

| Action | Where it lives now |
|---|---|
| Create room | `ClientConnect::createRoom` → local `chatRooms` |
| Open room tab | `connectToRoom` → local `connectedChatRooms` |
| Send in a room tab | `sendMessageToRoom` → that `ChatRoom`’s vector only |

A future room protocol should be a **new number**, leaving PROT1 as main chat.

---

### Typical session

```
TCP connect
  client  PROT2          →  server
  server  PROT2 welcome  →  client
  server  PROT3 userJoined / userList
  client  PROT4 CREATE|LOGIN
  server  PROT4 SUCCESS|FAIL

  then, any time:
  client  PROT1  ↔  server forwards PROT1 to others
  server  PROT3  →  clients on join/leave
```

---

### Compatibility rule

Add features as **new PROT numbers**. Keep the length prefix and transport wrap unchanged. An old peer should still speak PROT1–4 while a newer one also speaks rooms.

---


## Performance Testing (Server Benchmark)

Freia Thiwi has been benchmarked on extremely low-end hardware with outstanding results.

Hardware:

- Dell Inspiron 1545 (2009)
- Intel Celeron 900 @ 2.20 GHz (single core)
- 2 GB DDR2 RAM
- Ubuntu Server 64-bit

Load:
- 2 simultaneous clients (LAN)
- Continuous message spam (no idle time)

Resource Usage:

- Metric:	            Result
- CPU:	                ~0.0%
- RAM:	                ~0.2% (~4 MB)
- File Descriptors: 	3
- Stability:            No packet loss, no stalls
- Throughput:           Instant Message delivery

Verdict
Ultra-lightweight. Near-zero overhead. Runs flawlessly on 15-year-old hardware.

Freia Thiwi’s network loop and memory model are efficient enough to run on:

- decade-old laptops
- Raspberry Pi Zero
- embedded boards
- home servers
- virtual private servers with minimal resources

This makes it ideal for self-hosted, low-power, and always-on deployments.

---

## Build Instructions (Linux)

```bash
git clone <repo>
cd <repo>
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
./freia-thiwi

## Build Dependencies

### Debian / Ubuntu / Lubuntu
sudo apt update
sudo apt install build-essential cmake git libssl-dev

### Arch
sudo pacman -Syu
sudo pacman -S base-devel cmake git openssl

#Fedora
sudo dnf install gcc-c++ cmake git openssl-devel make

(Adjust GLFW package if using X11.)

```

Name Origin

Freia Thiwi is Gothic:
- 𐍆𐍂𐌴𐌹𐌰 (freia) — free, belonging to no master
- 𐌸𐌹𐍅𐌹 (thiwi) — maid, servant, helper

The servant of freedom. A system that serves the people, not the authorities.

Final Note

This project is built with a single goal:

No dictatorship, corporation, or government should have a monopoly on communication.

If this resonates with you:
Follow development, contribute by giving tips and ideas, and help bring secure speech to those who need it most.