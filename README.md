# angelscript-jit

**[English](README.md)** | **[Español](docs/README.es.md)**

A JIT compiler for AngelScript, written in C++20 on top of the [MIR](https://github.com/vnmakarov/mir) runtime. Scripts run 6 to 11 times faster than the interpreter on compute-heavy code. This is a maintained fork of [asumagic/angelsea](https://github.com/asumagic/angelsea) (BSD-2-Clause) that keeps the JIT working on the current AngelScript release. It is alpha-quality software, like upstream.

The library, the CMake target (`angelsea`) and the C++ namespace keep their upstream names, so changes can go back upstream.

## Supported releases

The JIT implements both JIT interfaces: `asIJITCompilerV2` (AngelScript 2.37.0 and later) and `asIJITCompiler`, version 1 (2.31.2 to 2.36.1). Every release marked below runs the differential test (the same scripts on the plain VM and with the JIT must give the same output, state and exception); 2.37.0 and later also run the Catch2 suite.

| AngelScript | Windows x64 | Linux x64 | Linux arm64 | macOS arm64 | macOS x64 | 32-bit x86 |
| --- | --- | --- | --- | --- | --- | --- |
| 2.39.0 (current) | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | AOT only |
| 2.38.0 | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | ❌ |
| 2.37.0 | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | ❌ |
| 2.36.1 to 2.33.0 (interface V1) | 🧪 | 🧪 | | | ❌ | ❌ |
| 2.32.0 and 2.31.2 (interface V1) | 🧪 | | | | ❌ | ❌ |
| 2.30.2 and earlier | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |

- AOT only: no runtime compiler (MIR has no 32-bit backend); functions run in the interpreter unless an ahead-of-time library built from `Jit::ExportModuleC` is loaded.
- ✅ tested on every push. ⚠️ built and tested in CI as an experimental job. 🧪 checked by hand against the differential corpus and the benchmarks, not in CI yet (Linux: 2.35.1 only). ❌ not supported.
- Windows is tested with MSVC, Linux with gcc. MinGW and clang work upstream.
- macOS x64 fails in MIR upstream; a CI job for it runs as experimental. 32-bit x86 has no MIR backend, so there the JIT is built without it (`ASEA_NO_MIR`, automatic when pointers are 32 bits) and only the ahead-of-time path works: see [Benchmarks](docs/BENCHMARKS.md). The alternative on 32-bit is the [BlindMind Studios JIT](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler).
- Releases before 2.37.0 use the version 1 interface through a small adapter (`Jit::CompileFunction`): functions are registered when the engine finishes them and compiled lazily, as in V2. The engine does not say which function a released JIT function belonged to, so records of functions discarded with their module stay until the engine is destroyed. 2.30.2 and earlier do not build (`asITypeInfo` and the type layout differ). Releases below 2.33 have no `try`/`catch`, so the differential scripts that use it do not run there. [angelscript-cli](https://github.com/Gaftherman/angelscript-cli) tells you which of its builds have the JIT.

## What this fork adds

- **Script exceptions raised by application functions are no longer lost.** `CScriptArray::At` (an out of bounds index) and any other function that calls `asIScriptContext::SetException` were ignored in direct system calls, because the JIT never set `m_callingSystemFunction`, which `SetException` requires. The script went on with the null reference the callee returned and crashed. The JIT now sets and clears it around direct calls and, like the VM, returns as soon as the context is no longer active.
- **aarch64 loads and stores at unaligned offsets.** AngelScript aligns object properties to 4 bytes, so a `double` can sit at offset 12. MIR's aarch64 backend could not encode that access and aborted; the fix lives in `vendor/mir`.
- `hack_ignore_context_inspect` is **off** by default, so the program pointer is saved before direct calls and exceptions report the right line and call stack. Turn it on for speed if your application functions never raise script exceptions or inspect the context.
- AngelScript 2.37 builds again: `asCScriptFunction::IsVariadic` is 2.38+, and 2.37's `GetDeclaredAt` reads out of bounds for functions without a section.
- **VM variables live in registers** (`experimental_register_locals`) and script calls to a known callee are set up inline: numeric loops run up to 2.5 times faster than before and `fib` 16-20% faster. See [Benchmarks](docs/BENCHMARKS.md).
- **JIT interface V1 and AOT.** Releases 2.31.2 to 2.36.1 run through `asIJITCompiler`. `Jit::ExportModuleC` writes the C of a module so you can compile it ahead of time with clang or gcc into a shared library, and `Jit::LoadAot` installs it (no MIR compile at start-up; `nbody` 25% faster than the MIR code). See `docs/DESIGN.md`.
- Three MIR defects fixed in `vendor/mir` (operand renumbering after a copy-propagation swap, an operand overflow in the spill rewrite, a silent assertion abort on aarch64).
- A **differential test** (`tests/differential`). `generate.py --seed N` writes random programs (arithmetic, loops, recursion, classes and interfaces, arrays and strings, try/catch), so a failure can be reproduced from the seed.
- `vendor/mir` points to a fork of asumagic/mir that carries the aarch64 fix, and `vendor/angelscript` is pinned to v2.39.0.

## Status

We have a test suite run in CI and we test the JIT against a development build of [KAG](https://store.steampowered.com/app/219830/King_Arthurs_Gold). The `main` branch should generally be reasonably stable.

Compared to the [BlindMind Studios JIT](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler), upstream measured +0~40% runtime performance in a real world application.

JIT compile times and memory use can be higher than that JIT, however:

- Lazy compilation is enabled by default: cold functions are never compiled.
- Asynchronous compilation can be enabled using `Jit::SetCompileCallback`. Only one thread runs code generation at once, but most of the process is asynchronous.
- Huge functions (thousands of lines) are skipped by default, because MIR's cost grows quickly with function size. The limit is adjustable.

## Supported platforms

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

## Clone & Build

Start by cloning the repository itself:

```bash
git clone --recurse-submodules https://github.com/Gaftherman/angelscript-jit.git
cd angelscript-jit
```

Or, if you want to vendor it as a Git submodule to your repository:

```bash
cd whatever/vendor/directory/
git submodule add https://github.com/Gaftherman/angelscript-jit.git
cd angelscript-jit
```

**Example project:** [`example/hello/`](example/hello)

Angelsea has hard dependencies on:

- [**AngelScript**](https://www.angelcode.com/angelscript/) v2.37.0+(\*)
- [**fmt**](https://github.com/fmtlib/fmt),
- [**MIR**](https://github.com/vnmakarov/mir) v1.0(\*\*)

(\*): The version requirement stems from the use of the `asIJITCompilerV2`
interface.  
(\*\*): We require [a downstream fork](https://github.com/asumagic/mir) for now,
see rationale further below.

It has optional dependencies on:
- [**Catch2**](https://github.com/catchorg/Catch2/tree/devel) (for testing)
- [**nanobench**](https://github.com/martinus/nanobench/) (for benchmarking in testing)

> [!NOTE]
> In theory, if you want to avoid building via CMake, you could pull Angelsea
> into your trunk to avoid the build process, and you would just have to ensure
> the include directories are right. We do not test or support this usecase.

### 1. Use vendored versions

**By default, these dependencies are vendored via git submodules.**  
When you add Angelsea as a CMake subdirectory, you can also use the targets it
provides for those libraries to link against them.

If you use CMake to build your project, you can choose to use the AngelScript
version we vendor for your own project and link against the `asea_angelscript`
target.

If you want to provide any of dependencies yourself (e.g. you also use them and
don't want to rely on Angelsea building it), see the optional step.

> [!WARNING]
> As an user, you should know that upstream MIR has not seen activity in a year.
> I provide a downstream fork of MIR to work around specific problems, and we
> hope the defaults to be fully stable.
>
> The contents of the fork is:
> - The load/store optimizations of GVN pass broke on various occasions with our
> generated code. It just seems to intensely dislike the constant AS stack back
> and forth we are doing.
> We worked around some issues (see below), but it is frankly more trouble than
> it is worth.
> Thus we moved it to a "-O3" level; and we default to
> `config.mir_optimization_level = 2`, and strongly discourage changing this.
> - Solve [Integer sign-extension miscompile](https://github.com/vnmakarov/mir/issues/423)
> (workaround if using upstream: set `config.mir_optimization_level = 1;`)
> - Solve [Jump optimization can cause use-after-free when using label references](https://github.com/vnmakarov/mir/issues/424)
> (workaround if using upstream: set `config.mir_optimization_level = 1;`)
> - Solve high memory usage: implemented a hack; see `config.hack_mir_minimize` (defaults to true)

```bash
git submodule update --init --recursive vendor/angelscript
git submodule update --init --recursive vendor/mir
git submodule update --init --recursive vendor/fmt
# if you want to run angelsea tests
git submodule update --init --recursive tests/vendor/*
```

### Optional: Provide specific dependencies yourself

TODO: the current steps work for static lib builds, but won't for dynamic libs
and doesn't work for tests, and it should provide a way to provide the paths to
built dependencies in that case.

#### AngelScript

Pass to CMake:

- `-DASEA_ANGELSCRIPT_SYSTEM=1`
- `-DASEA_ANGELSCRIPT_ROOT=/path/to/angelscript/sdk/` **(defaults to `vendor/angelscript/sdk`)**

#### fmt

Either:

- Pass `-DASEA_FMT_SYSTEM=1` to CMake to rely on `find_package(fmt)`.
- Pass the following to point headers to a known directory:
    - `-DASEA_FMT_SYSTEM=1`
    - `-DASEA_FMT_EXTERNAL=1`
    - `-DASEA_FMT_ROOT=/path/to/fmt` **(defaults to `vendor/fmt`)**

#### MIR

Pass to CMake:

- `-DASEA_MIR_SYSTEM=1`
- `-DASEA_MIR_ROOT=/path/to/mir/` **(defaults to `vendor/mir`)**

### 2a. Build the standalone library statically

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

On Linux anyway, the resulting library will be available as
`build/libangelsea.a`, which you can now link against in your build. For
vendored dependencies, make sure you link against them:

- `build/libasea_mir.a`
- `build/vendor/fmt/libfmt.a` (`libfmtd.a` in debug)
- `build/vendor/angelscript/sdk/angelscript/projects/cmake/libangelscript.a`

### 2b. Include as a CMake submodule

If you use CMake to build your project, you can do the following:

```cmake
add_subdirectory(path/to/angelsea)
target_link_libraries(yourexecutable PRIVATE angelsea)
```

This will automatically build any vendored dependencies. You can also achieve
the steps in 1b with this setup by adding the following **BEFORE** the
`add_subdirectory` step:

```cmake
set(ASEA_FMT_SYSTEM OFF CACHE BOOL "")
```

Note that cache variables tend to be sticky; clear the workspace if the build
errors are confusing.

## Use

We implement the [`asIJITCompilerV2`](https://www.angelcode.com/angelscript/sdk/docs/manual/classas_i_j_i_t_compiler_v2.html)
JIT interface.

Enabling the JIT engine amounts to:

```cpp
#include <angelsea.hpp>

// ...

engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true);
engine->SetEngineProperty(asEP_JIT_INTERFACE_VERSION, 2);
engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

// example config
angelsea::JitConfig config;
angelsea::Jit jit(config, *engine);
assert(engine->SetJITCompiler(&jit) >= 0);
```

It is strongly encouraged to check [`JitConfig` tunables](include/angelsea/config.hpp)
and to adjust it accordingly for your application.  
The defaults are overall tuned for the needs of a semi-heavily scripted
commercial application.

For instance, to disable lazy compilation:

```cpp
angelsea::JitConfig config {
    .triggers = {
        .hits_before_func_compile = 0,
    }
};
```

It is also possible to specify some per-function JIT tunables with a callback.
For an example that leverages script builder metadata, see [`tests/configtest.cpp`](tests/configtest.cpp).

> [!WARNING]
> The JIT compiler itself is not currently thread-safe with regards to
> multithreaded AngelScript contexts or engines.

## Documentation

- [Running the test suite](docs/TESTS.md)
- [How it works and why](docs/DESIGN.md): the C backend, calling conventions, MIR and the other JIT compilers
- [Benchmarks](docs/BENCHMARKS.md): speed against the interpreter, native C++, upstream angelsea and BlindMind, and the limits
- [Contributing](CONTRIBUTING.md)
- [Third-party notices](docs/THIRD_PARTY.md)

## Related projects

| Project | Use |
| --- | --- |
| [angelscript-cli](https://github.com/Gaftherman/angelscript-cli) | Run, check and debug scripts from the terminal. Its JIT builds use this repository |
| [angelscript-engine](https://github.com/Gaftherman/angelscript-engine) | Patched SDK builds of older AngelScript releases |
| [asumagic/angelsea](https://github.com/asumagic/angelsea) | The upstream project |

## License

[BSD-2-Clause](LICENSE), like upstream. Third-party notices are in [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).
