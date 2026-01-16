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