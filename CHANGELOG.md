# Changelog

## [0.1.0] - 2026-11-25

First release of the fork of [asumagic/angelsea](https://github.com/asumagic/angelsea) at upstream commit `1d367d4`.

### Added

- A differential test host (`tests/differential`) and a seeded program generator (`generate.py`) that compare the plain VM with the JIT on every script.
- CI against AngelScript 2.37.0, 2.38.0 and 2.39.0 on Linux and Windows x86-64, plus Linux and macOS arm64 as experimental jobs.

### Changed

- The vendored AngelScript is pinned to v2.39.0.
- `JitConfig::hack_ignore_context_inspect` is `false` by default.
- The `mir` submodule URL is absolute.

### Fixed

- Script exceptions raised by application functions during direct system calls (an out of bounds `array` access, for example) were dropped, and the script crashed on the null reference that followed. The JIT now sets `m_callingSystemFunction` around direct calls and returns to the VM when the context is no longer active.
- AngelScript 2.37.0 did not build (`IsVariadic` exists from 2.38) and crashed with `human_readable` output (`GetDeclaredAt` reads out of bounds when a function has no section).
