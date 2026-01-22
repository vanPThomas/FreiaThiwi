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

## [v0.2.2] - 2026-01-20
### Changed
- server.cpp: Added mutex to lock delicate code to avoid undefined behavior
- server.cpp: More consistent error handling
- server.cpp: Improved disconnect logic
- server.cpp: improved variable naming

## [v0.2.1] - 2026-01-19
### Changed
- server.cpp: refectored for cleared protocol handling

## [v0.2.0] - 2026-01-18
### added
- Added protocol recognition

## [v0.1.4] - 2025-11-18
### added
- Added package length for complete package transfer.

## [v0.1.3] - 2025-11-13
### Changed
- Complete refactoring of the `run` function in `server.cpp`.
- Hollowed out `run` with clear titled functions for clearer flow.

## [v0.1.2] - 2025-11-12
### Changed
- Converted `Server` into proper non-static class
- Hollowed out `main.cpp` putting everything in `server.cpp`

## [v0.1.1] - 2025-11-12
### Changed
- Moved socket setup and cleanup functions into `Server` static class.
- Cleaned up `main.cpp`, reduced redundancy.

## [v0.1.0] - 2025-11-11
### Added
- Basic TCP server.
- Multi-client support via select().