# Roadmap

This file is written for LLM agents and contributors picking up the work. It states what the project is, what works, the known limits, and what is planned next, in priority order. Verify claims against the code before acting on them.

## What this repository is

`angelscript-jit` is a fork of [asumagic/angelsea](https://github.com/asumagic/angelsea): a JIT for AngelScript. Pipeline:

1. AngelScript bytecode is translated to C (`src/angelsea/detail/bytecode2c.cpp`). This translator is identical on every target.
2. The C is turned into machine code by one of two back ends:
   - **MIR** (`vendor/mir`, c2mir + MIR_gen, level 2) at runtime: x86-64 and aarch64.
   - **AOT**: `ExportModuleC` writes `<base>.c` and `<base>.externs`; any C compiler (gcc, clang, MSVC) builds a shared library; `LoadAot` installs its entries over the interpreter.
3. Anything not translated stays in the AngelScript VM (interpreter fallback is always correct).

Supported AngelScript releases: 2.37, 2.38, 2.39 (2.39.0 is the reference). Interfaces: V2 and V1 (`jit.hpp`).

## Status by platform

| Target | State | Notes |
| --- | --- | --- |
| Windows x64 (MSVC, MinGW) | complete, CI blocking | MIR runtime + AOT |
| Linux x64 (gcc, clang) | complete, CI blocking | MIR runtime + AOT |
| Linux / macOS arm64 | experimental, CI non-blocking | verified under qemu; MIR needed fixes in `vendor/mir` (branch `aarch64-unaligned-offsets`) |
| 32-bit x86 (Windows MSVC, Linux gcc) | AOT only | MIR has no 32-bit back end. Built with `ASEA_NO_MIR` (automatic when `sizeof(void*)==4`). Verified on 2.39.0 only: 29/29 corpus scripts match the VM |
| macOS x64 | unverified | upstream issue asumagic/angelsea#3. CI job `macos-15-intel` added, never observed. No Mac available to the maintainer |

Measured speed-up over the VM (2.39.0): x64 loops x28-44, nbody ~x3, fib x2-3. 32-bit AOT: loops x29-41, matmul x5.5-8, sieve x3.8-4.1, fib x2.1-3.4, nbody x2.2-3.8. See `docs/BENCHMARKS.md`.

## Known limitations

- **No inlining** of script-to-script calls (planned item 2.5). Call-heavy code (fib) gains less than loop-heavy code.
- **No array bounds-check hoisting** (planned item 2.6). Array indexing in hot loops re-checks bounds each access.
- **Some opcodes fall back to the VM** or to runtime helpers instead of native code (planned item 2.3 remainder). Correct but slower.
- **Application function calls** cost the same on every platform. Virtual thiscall and by-value object returns on 32-bit MSVC go through `asea_call_system_function` (correct, slower).
- **32-bit x86 has no runtime JIT.** Only AOT libraries run natively; unlisted functions run in the VM. TinyCC as an in-process back end was measured and rejected (fib x0.53, nbody x0.85 versus the VM; loops x10 only).
- **32-bit x86 verified on 2.39.0 only.** 2.37, 2.38 and the V1 interface are untested there and stay marked unsupported in the README matrix.
- **macOS x64 failure is undiagnosed.** Suspects, in order: mmap/W^X in `MIR_gen`; `__USER_LABEL_PREFIX__` and `asea_*` symbol resolution in `mirjit.cpp`; `long double` / `va_list` in the c2mir headers; vtable dereference in virtual calls (`bytecode2c.cpp`, `MACOS_X86_64` branch).
- **AOT on macOS** is not implemented in `tests/aot/run.sh` (needs `-dynamiclib`, `.dylib`).
- **`sven_jit`** integration (a Sven Co-op plugin host) is not done.

## Planned work, in priority order

### 1. macOS x64 (needs CI, no local Mac)
1. Run the `macos-15-intel` job from `.github/workflows/ci.yml` and read the failure.
2. Fix at the cause: in `vendor/mir` (separate commit, like JIT-14) or in the `bytecode2c` ABI branch.
3. Add `.dylib` support to `tests/aot/run.sh` and an AOT step to that CI job.
4. Mark macOS x64 supported only when Catch2, the differential corpus, fuzz seeds 1-25 and AOT all pass in CI.
Expected ceiling: equal to Linux x64 (same ISA, ABI and MIR).

### 2. 32-bit x86 follow-ups
- Verify 2.37, 2.38 and V1 on x86; update the README matrix rows.
- Add `LINUX_GCC_X86` ABI mask if the generic stack-`this` path proves a bottleneck.
- Direct (non-helper) calls for `array` thiscall variants.
- Expected ceiling: 70-90% of the x64 AOT speed-up (8 registers, 64-bit values in pairs).

### 3. Performance (shared by all targets)
- 2.5 inline small script functions.
- 2.6 hoist array bounds checks out of loops.
- 2.3 replace remaining opcode fallbacks with native code.
Acceptance rule: keep a change only if the target benchmark improves >=5% and no other row worsens >3% (`tests/bench/run.py`).

### 4. Tooling and integration
- CLI (`angelscript-cli`): wire AOT into x86 builds (`scripts/plan-engine-builds.py`, `scripts/build-engine.py`).
- `sven_jit` host integration.

## Pending commits (not yet pushed)

All work below is uncommitted in the working tree. Commit messages are drafted in `_audit-coord/patches/`: CLI-13, JIT-14 (submodule), JIT-15, JIT-16, JIT-17, JIT-18, CLI-14. Order matters: the `vendor/mir` commit must be pushed before the `angelscript-jit` commits that reference it, and CLI-14 updates the submodule pointer last.

## Working rules for agents

- Never commit or push on the user's behalf; only the user commits.
- No personal paths, emails or usernames in tracked files.
- Build and test commands: `cmake -S . -B build -G Ninja -DASEA_ENABLE_TESTING=ON`; run Catch2 from the `tests` directory (a wrong cwd gives false failures): `../build/tests/angelsea-tests "~[benchmark]"`; differential host: `build/tests/angelsea-differential`; AOT corpus: `CC=<cc> EXT=<so|dll|dylib> sh tests/aot/run.sh <differential exe> <outdir>`.
- 32-bit Linux checks run in Docker: `docker run --rm -v "$PWD:/src:ro" i386/debian:12 sh /src/tests/aot/ci-i386.sh`.
- Design details: `docs/DESIGN.md`. Measurements: `docs/BENCHMARKS.md`.
