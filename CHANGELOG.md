# Changelog

## [0.2.0]

### Added

- A compiled function that calls another compiled function keeps running when the callee returns, instead of going back to the VM and being dispatched in again (`JitConfig::experimental_direct_continue`, on by default). It applies up to 256 nested script frames; deeper calls go through the VM, so deep recursion cannot exhaust the native stack. `fib(32)` runs about 28% faster.
- Interface method calls (`CALLINTF` on an interface) are compiled instead of left to the VM, through `asea_resolve_virtual_function`. A loop calling interface methods runs about 17% faster.
- `tests/bench` (six scripts) and `angelsea-differential --time N`, which reports the best of N runs of the VM and of the JIT. The `angelsea-bench` target runs them.
- `angelsea-differential` accepts a folder as an argument, so CI does not depend on shell patterns.
- Regression scripts: a `double` at an unaligned offset, recursion 20000 frames deep, and method calls through null handles.
- `README.es.md`, `CONTRIBUTING.md` (English and Spanish), `docs/THIRD_PARTY.md`, and a support table by AngelScript release and platform.

### Fixed

- aarch64: a load or store at an offset that is not a multiple of the access size aborted code generation with `fatal failure in matching insn` (AngelScript aligns object properties to 4 bytes, so a `double` can sit at offset 12). The offset is now rebuilt in a temporary register, in `vendor/mir`. Every generated script that had a class with an `int` before a `double` failed on Linux and macOS arm64.
- A virtual method call through a null handle crashed with a segmentation fault. The VM now executes the call and raises the script exception.

### Changed

- The upstream questions and answers moved to `docs/DESIGN.md`, and `doc/TESTS.md` to `docs/TESTS.md`.
- CI builds only `main` and pull requests, skips documentation-only changes, cancels superseded runs and caches the downloaded SDKs.

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
