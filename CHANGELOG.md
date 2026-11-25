# Changelog
All notable changes to **Freia Thiwi** will be documented here.

## [0.6.0] - 2026-11-25
### Added
- `PROT5` chatroom support: create, join, leave, and room-specific encrypted message relay.
- Automatic room join for the creator, and `PROT5 ROOM` replies on both first and repeat joins.
- Live room-user snapshots (`PROT5 USERS`) broadcast when participants join, leave, or disconnect.
- SQLite `rooms`, `room_members`, and `room_messages` tables in the existing `accounts.db`.
- Persistent room metadata, creator/creation time, recorded membership, and encrypted message history (up to 100 recent messages replayed on join).
- Server-side room-password checks using a salted PBKDF2-HMAC-SHA256 verifier of a separate room-login token.
- Persistent room-name list sent to authenticated clients.

### Changed
- Account authentication through `PROT4` is now required before access to chatroom operations.
- Room messages are delivered only to sockets joined to the target room; the server stores encrypted, Base64-encoded message bodies, not their plaintext.
- Room-user presence is derived from active socket membership; SQLite is used for persistent data rather than an online-state cache.
- Transport packet limit increased to 64 KiB, with complete send loops and improved framing checks.

### Fixed
- Repeat joins previously returned a success notice but could omit the required room data.
- Live member changes no longer disappear when `ChatRoom` objects are copied out of the vector.
- Disconnect cleanup refreshes online room-user lists.
- Incorrect packet-length diagnostic string construction and Base64 decoding issues.

### Notes
- Room credentials and `PROT5` wire behavior changed: upgrade server and client together for v0.6.0.
- Historical in-memory rooms cannot be recovered if they were never saved before this release.
- AES-256-CBC is still unauthenticated; v0.6.0 is a development release, not a security audit or final secure-messaging milestone.

---


## [0.5.0] - 2026-02-13
### Added
- Proper Embedded Database has been added for accounts 

---

## [0.4.1] - 2026-02-11
### Changed
- Created helper function sendWithLengthPrefix for cleaner code

---

## [0.4.0] - 2026-02-07
### Added
- Full account system: user registration and login with per-account passwords
- New PROT4 protocol for account creation (`CREATE`) and login (`LOGIN`)
- In-memory account storage (username → base64-encoded derived key) as temporary DB replacement
- Server validates account credentials after PROT2 transport handshake
- Proper SUCCESS/FAIL replies for account operations
- Error handling for duplicate usernames, wrong passwords, malformed requests

### Changed
- Authentication now layered: PROT2 (server password) → PROT4 (account credentials)
- Existing shared-server-password clients still work (PROT2 only)

### Security
- Passwords never stored in plaintext — only derived keys (Argon2id) are kept
- All account traffic protected by existing transport encryption

This release introduces persistent user identity while preserving the original lightweight, privacy-first design.

---

## [v0.3.5] - 2026-01-30
### Changed
- Cleaned up code

---

## [v0.3.4] - 2026-01-29
### Fixed
- Fixed bug where disconnected people weren't properly removed from online list

---

## [v0.3.3] - 2026-01-28
### Fixed
- Fixed bug where all online people wasn't properly shown

---

## [v0.3.2] - 2026-01-26
### Added
- Added PROT3 message to send connected clients to all clients

---

## [v0.3.1] - 2026-01-23
### Added
- Added PROT3 for server messages
- Added user disconnect message

---

## [v0.3.0] - 2026-01-22
### Added
- Initial server authentication handshake (PROT2)
  - Clients must now send an encrypted handshake containing username immediately after TCP connect
  - Server verifies server password correctness via decryption
  - Username is extracted and stored upon successful handshake
  - Only authenticated clients are added to the active client list
- Server sends encrypted "OK" reply on successful authentication
- Immediate socket close on handshake failure (wrong password, malformed frame, etc.)
- Username is now available right after connect (prepares for future system messages like disconnect notices)

### Changed
- Connection flow is now strictly authenticated before any chat messages are processed

### Security
- Username never sent in plaintext
- Server password enforcement happens at the very first packet

---

## [v0.2.2] - 2026-01-20
### Changed
- server.cpp: Added mutex to lock delicate code to avoid undefined behavior
- server.cpp: More consistent error handling
- server.cpp: Improved disconnect logic
- server.cpp: improved variable naming

---

## [v0.2.1] - 2026-01-19
### Changed
- server.cpp: refectored for cleared protocol handling

---

## [v0.2.0] - 2026-01-18
### added
- Added protocol recognition

---

## [v0.1.4] - 2025-11-18
### added
- Added package length for complete package transfer.

---

## [v0.1.3] - 2025-11-13
### Changed
- Complete refactoring of the `run` function in `server.cpp`.
- Hollowed out `run` with clear titled functions for clearer flow.

---

## [v0.1.2] - 2025-11-12
### Changed
- Converted `Server` into proper non-static class
- Hollowed out `main.cpp` putting everything in `server.cpp`

---

## [v0.1.1] - 2025-11-12
### Changed
- Moved socket setup and cleanup functions into `Server` static class.
- Cleaned up `main.cpp`, reduced redundancy.

---

## [v0.1.0] - 2025-11-11
### Added
- Basic TCP server.
- Multi-client support via select().