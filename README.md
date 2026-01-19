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

This repo is the client side of the Freia Thiwi Project, the server is not publicly available.

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