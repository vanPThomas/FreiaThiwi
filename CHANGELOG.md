# Changelog
All notable changes to **Freia Thiwi** will be documented here.

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