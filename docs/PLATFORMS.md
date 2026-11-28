# Platforms

**[English](PLATFORMS.md)** | **[Español](PLATFORMS.es.md)**

## Systems and architectures

|    | OS       | Architecture | ABI   | Notes |
|----|----------|--------------|-------|-------|
| ✅ | Linux    | x86-64       | gcc   | Tested on a real application upstream |
| ✅ | Windows  | x86-64       | MSVC  | Tested here on every push |
| ✅ | Windows  | x86-64       | MinGW | Tested on a real application upstream |
| ⚠️ | Linux    | aarch64      | gcc   | Experimental |
| ⚠️ | macOS    | aarch64      | Apple | Experimental |
| ❌ | macOS    | x86-64       | Apple | Fails in MIR. See [asumagic/angelsea#3](https://github.com/asumagic/angelsea/issues/3) |
| ⚠️ | Windows, Linux | 32-bit x86 | MSVC, gcc | AOT only: no MIR backend, ahead-of-time libraries run (29 of 29 corpus scripts on both) |
| ❓ | Linux    | riscv64, ppc64le | gcc | Supported by MIR, not tested |
| ❌ | Linux    | s390x        | gcc   | Big-endian is not supported |

## Notes

- Windows is tested with MSVC, Linux with gcc. MinGW and clang work upstream.
- macOS x64 fails in MIR upstream; a CI job for it runs as experimental.
- 32-bit x86 has no MIR backend, so there the JIT is built without it (`ASEA_NO_MIR`, automatic when pointers are 32 bits) and only the ahead-of-time path works: see [Benchmarks](BENCHMARKS.md). The alternative on 32-bit is the [BlindMind Studios JIT](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler).
- Releases before 2.37.0 use the version 1 interface through a small adapter (`Jit::CompileFunction`): functions are registered when the engine finishes them and compiled lazily, as in V2. The engine does not say which function a released JIT function belonged to, so records of functions discarded with their module stay until the engine is destroyed. 2.30.2 and earlier do not build (`asITypeInfo` and the type layout differ). Releases below 2.33 have no `try`/`catch`, so the differential scripts that use it do not run there. [angelscript-cli](https://github.com/angelscript-tools/angelscript-cli) tells you which of its builds have the JIT.
- Every release marked in the README runs the differential test (the same scripts on the plain VM and with the JIT must give the same output, state and exception); 2.37.0 and later also run the Catch2 suite.

## Status and compile cost

The test suite runs in CI and the JIT is tested against a development build of [KAG](https://store.steampowered.com/app/219830/King_Arthurs_Gold). The `main` branch should generally be reasonably stable.

Compared to the [BlindMind Studios JIT](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler), upstream measured +0~40% runtime performance in a real world application. Compile times and memory use can be higher:

- Lazy compilation is enabled by default: cold functions are never compiled.
- Asynchronous compilation can be enabled using `Jit::SetCompileCallback`. Only one thread runs code generation at once, but most of the process is asynchronous.
- Huge functions (thousands of lines) are skipped by default, because MIR's cost grows quickly with function size. The limit is adjustable.

## What this fork adds, in detail

- **Script exceptions raised by application functions are no longer lost.** `CScriptArray::At` (an out of bounds index) and any other function that calls `asIScriptContext::SetException` were ignored in direct system calls, because the JIT never set `m_callingSystemFunction`, which `SetException` requires. The JIT now sets and clears it around direct calls and, like the VM, returns as soon as the context is no longer active.
- **aarch64 loads and stores at unaligned offsets.** AngelScript aligns object properties to 4 bytes, so a `double` can sit at offset 12. MIR's aarch64 backend could not encode that access and aborted; the fix lives in `vendor/mir`.
- `hack_ignore_context_inspect` is **off** by default, so the program pointer is saved before direct calls and exceptions report the right line and call stack. Turn it on for speed if your application functions never raise script exceptions or inspect the context.
- AngelScript 2.37 builds again: `asCScriptFunction::IsVariadic` is 2.38+, and 2.37's `GetDeclaredAt` reads out of bounds for functions without a section.
- **VM variables live in registers** (`experimental_register_locals`) and script calls to a known callee are set up inline: numeric loops run up to 2.5 times faster than before and `fib` 16-20% faster. See [Benchmarks](BENCHMARKS.md).
- **JIT interface V1 and AOT.** Releases 2.31.2 to 2.36.1 run through `asIJITCompiler`. `Jit::ExportModuleC` writes the C of a module so you can compile it ahead of time with clang, gcc or MSVC into a shared library, and `Jit::LoadAot` installs it (no MIR compile at start-up; `nbody` 25% faster than the MIR code). See [DESIGN.md](DESIGN.md).
- Three MIR defects fixed in `vendor/mir` (operand renumbering after a copy-propagation swap, an operand overflow in the spill rewrite, a silent assertion abort on aarch64).
- A **differential test** (`tests/differential`). `generate.py --seed N` writes random programs (arithmetic, loops, recursion, classes and interfaces, arrays and strings, try/catch), so a failure can be reproduced from the seed.
- `vendor/mir` points to a fork of asumagic/mir that carries the aarch64 fix, and `vendor/angelscript` is pinned to v2.39.0.
