# angelscript-jit

**[English](README.md)** | **[Español](docs/README.es.md)**

A JIT compiler for AngelScript, written in C++20 on top of the [MIR](https://github.com/vnmakarov/mir) runtime. Scripts run 6 to 11 times faster than the interpreter on compute-heavy code. This is a maintained fork of [asumagic/angelsea](https://github.com/asumagic/angelsea) (BSD-2-Clause) that keeps the JIT working on the current AngelScript release. It is alpha-quality software, like upstream.

The library, the CMake target (`angelsea`) and the C++ namespace keep their upstream names, so changes can go back upstream.

## Supported releases

Interfaces: `asIJITCompilerV2` (AngelScript 2.37.0 and later) and `asIJITCompiler` version 1 (2.31.2 to 2.36.1).

| AngelScript | Windows x64 | Linux x64 | Linux arm64 | macOS arm64 | macOS x64 | 32-bit x86 |
| --- | --- | --- | --- | --- | --- | --- |
| 2.39.0 (current) | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | AOT only |
| 2.38.0 | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | ❌ |
| 2.37.0 | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | ❌ |
| 2.36.1 to 2.31.2 (interface V1) | 🧪 | 🧪 | | | ❌ | ❌ |
| 2.30.2 and earlier | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |

✅ tested on every push. ⚠️ experimental CI job. 🧪 checked by hand, not in CI. ❌ not supported. **AOT only**: no runtime compiler; load a library built from `Jit::ExportModuleC`. Details, other architectures and notes are in [docs/PLATFORMS.md](docs/PLATFORMS.md).

## What this fork adds

- Script exceptions raised by application functions are no longer lost.
- aarch64 loads and stores at unaligned offsets, and three MIR defects, fixed in `vendor/mir`.
- VM variables live in registers and script-to-script calls are set up inline.
- Interface V1 (2.31.2 to 2.36.1) and **AOT**: `Jit::ExportModuleC` writes the C of a module, any C compiler builds it, `Jit::LoadAot` installs it.
- 32-bit x86 through AOT, with MSVC thiscall.
- A random-program differential test (`tests/differential`).

## Use

```cpp
#include <angelsea.hpp>

engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true);
engine->SetEngineProperty(asEP_JIT_INTERFACE_VERSION, 2);
engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

angelsea::JitConfig config;
angelsea::Jit jit(config, *engine);
assert(engine->SetJITCompiler(&jit) >= 0);
```

Tunables, per-function callbacks and the thread-safety limit are in [docs/USAGE.md](docs/USAGE.md). A complete project is in [`example/hello/`](example/hello).

## Building from source

```bash
git clone --recurse-submodules https://github.com/Gaftherman/angelscript-jit.git
cd angelscript-jit
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Use your own dependencies or add it as a CMake subdirectory: see [docs/BUILDING.md](docs/BUILDING.md).

## Documentation

- [Building](docs/BUILDING.md) — dependencies, vendored or system, static library and subdirectory.
- [Usage](docs/USAGE.md) — `JitConfig`, lazy and asynchronous compilation.
- [Platforms](docs/PLATFORMS.md) — supported systems and architectures.
- [Design](docs/DESIGN.md) — the C backend, calling conventions, MIR and the other JIT compilers.
- [Benchmarks](docs/BENCHMARKS.md) — speed against the interpreter, native C++ and other JITs.
- [Tests](docs/TESTS.md) — running the suite.
- [Roadmap](docs/ROADMAP.md) — plans and limits, written for LLM agents.
- [CHANGELOG.md](CHANGELOG.md) — release history.

## Roadmap

- macOS x64: diagnose the failure in CI and add AOT with `.dylib`.
- 32-bit x86: verify 2.37, 2.38 and interface V1.
- Performance: inline small script functions, hoist array bounds checks, replace the remaining opcode fallbacks.

The full list is in [docs/ROADMAP.md](docs/ROADMAP.md).

## Support

Open an [issue](https://github.com/Gaftherman/angelscript-jit/issues) with the AngelScript release, the platform and a short script that reproduces it. Problems of the executable go to [angelscript-cli](https://github.com/Gaftherman/angelscript-cli/issues). To help, read [CONTRIBUTING.md](CONTRIBUTING.md).

## Related

| Project | Use |
| --- | --- |
| [angelscript-cli](https://github.com/Gaftherman/angelscript-cli) | Run, check and debug scripts from the terminal. Its JIT builds use this repository |
| [angelscript-lsp](https://github.com/Gaftherman/angelscript-lsp) | Editor support for VS Code |
| [angelscript-dap](https://github.com/Gaftherman/angelscript-dap) | Debugger core |
| [angelscript-engine](https://github.com/Gaftherman/angelscript-engine) | Patched SDK builds of older AngelScript releases |
| [angelscript-debugger-example](https://github.com/Gaftherman/angelscript-debugger-example) | A host that shows how to use the debugger |

### Similar projects

- [asumagic/angelsea](https://github.com/asumagic/angelsea): the upstream project this repository forks.
- [BlindMind Studios JIT](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler): the other AngelScript JIT, with a 32-bit x86 back end.

## Credits

Third-party notices are in [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).

| Project | License | Use |
| --- | --- | --- |
| [asumagic/angelsea](https://github.com/asumagic/angelsea) | BSD-2-Clause | The JIT this repository forks |
| [MIR](https://github.com/vnmakarov/mir) by Vladimir Makarov | MIT | Runtime code generator and C compiler |
| [AngelScript](https://github.com/anjo76/angelscript) by Andreas Jönsson | zlib | The language and its runtime |
| [{fmt}](https://github.com/fmtlib/fmt) | MIT | Formatting |
| [Catch2](https://github.com/catchorg/Catch2), [nanobench](https://github.com/martinus/nanobench) | BSL-1.0, MIT | Tests and benchmarks |

## License

This repository is licensed under the [BSD 2-Clause License](LICENSE), like upstream.
