# Changelog

## [Unreleased]

### Added

- JIT interface version 1 (`asIJITCompiler`): the JIT now runs on AngelScript 2.31.2 to 2.36.1 through `Jit::CompileFunction`, with `detail/compat.hpp` holding every difference between releases (`SetJITFunction`, `GetDeclaredAt`, `asINT32`, the call stack constants, `onHeap`, the composite and generic call properties). Speed-ups over the interpreter match 2.39's (`loops` x20-32, `fib` x2.2-3.4, `matmul` x5-7) and the differential corpus passes on Windows x64 from 2.31.2 to 2.36.1 and on Linux x64 with 2.35.1. 2.30.2 and earlier do not build.
- The call stack push and stack check of a script call to a known callee are generated inline, with the runtime helper as the slow path: `fib(32)` runs 16-20% faster.
- VM variables are promoted to C locals (`JitConfig::experimental_register_locals`, on by default; see `docs/DESIGN.md`). Measured on Windows x64 with AngelScript 2.39: `loops` 2.5 times faster, `nbody` 1.8 times, `matmul` 1.35 times; the other benchmarks are within noise. `experimental_spill_on_native_calls` (off) writes the locals back before every direct application call.
- `tests/bench/run.py` (interleaved runs, median and p95), `matmul`, `virtual` and `closures` benchmarks, native C++ equivalents in `tests/bench/native/ceiling.cpp`, and `docs/BENCHMARKS.md` with the comparison against upstream angelsea and BlindMind Studios.
- AOT: `Jit::ExportModuleC(module, base)` writes the C of a module to `<base>.c` and the symbols the loader must provide to `<base>.externs`; compiled to a shared library with clang or gcc, `Jit::LoadAot(module, library, manifest)` installs it (symbols are resolved by name at load, functions whose bytecode changed are skipped, a library for another AngelScript release is rejected). The script corpus passes against the VM running only AOT code (`tests/aot/run.sh`); on Windows x64 with clang `-O2`, `nbody` runs 25% faster than the MIR code, `matmul` 20% and `fib` 18%. `angelsea-differential` exposes both as `ASEA_EXPORT_C=<base>` and `ASEA_AOT=<base>`, see `docs/DESIGN.md`.
- The generated C no longer holds the address of the callee's bytecode (inline script calls) or of the function of a `FuncPtr` as literals: both are extern symbols, like the other addresses.
- The `**` instructions (`POWi`, `POWu`, `POWf`, `POWd`, `POWdi`, `POWi64`, `POWu64`) are compiled natively instead of going back to the interpreter, with the VM's overflow exception; covered by `tests/scripts/pow_ops.as`.
- Calls through a function pointer (`asBC_CallPtr`) to a script function are compiled natively, like virtual calls (a null pointer, a delegate or an application function still goes to the interpreter, which raises the exception or performs the call). `closures` runs 15% faster (95 to 81 ms on Windows x64).
- `asBC_FuncPtr` is compiled natively (it pushed through the interpreter before); covered by `tests/scripts/misc_ops.as`, now part of the differential test.
- `angelsea-differential` environment switches for diagnosis: `ASEA_NO_REGLOCALS`, `ASEA_MIR_LEVEL`, `ASEA_MIR_DEBUG`, `ASEA_DUMP_MIR`, `ASEA_SHOW_OUT`, `ASEA_HITS` (lazy compilation at N hits instead of eager).

- AOT libraries also build with MSVC `cl /TC` (the GCC type macros and `restrict` come from `tests/aot/msvc_shim.h`, forced with `/FI`); `tests/aot/run.sh` takes `CC=cl` and the 29 runnable scripts match on Windows x64. AOT on arm64 passes under qemu with gcc.
- 32-bit x86 builds (`ASEA_NO_MIR`, automatic when pointers are 32 bits): the JIT compiles without MIR and runs AOT libraries only. `loops` x29-41, `matmul` x5-8, `sieve` x4 and `fib` x2-3.4 over the VM; the 29 corpus scripts match on Windows x86 (MSVC) and Linux x86 (gcc, 32-bit container). Two new CI jobs (`x86-linux`, `x86-windows`, experimental) and `tests/aot/ci-i386.sh`. New ABI `AbiMask::WINDOWS_MSVC_X86`: non-virtual `thiscall` is called as `__fastcall` with a dummy EDX argument.
- `.github/workflows/ci.yml`: an experimental macOS x64 job (`macos-15-intel`) to reproduce the upstream failure; macOS x64 is still unsupported until it passes.

### Fixed

- Three MIR bugs, found because the promoted locals produce many more live registers (`vendor/mir`, a separate commit in the `mir` submodule):
  - copy propagation swapped the operands of an `and` without renumbering their SSA edges, which `gen_assert` caught on aarch64 (silent abort) and which corrupted memory on x64;
  - `try_spilled_reg_mem` overflowed its operand array when one pseudo register appeared three times in an instruction (`xors t, t, t`), and left a stale operand behind, so `x ^ x`, `x + x` and `x - x` returned a wrong value;
  - the aarch64 assertion macro now prints the line before aborting.

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
