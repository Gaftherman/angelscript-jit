# Building

**[English](BUILDING.md)** | **[Español](BUILDING.es.md)**

Clone the repository:

```bash
git clone --recurse-submodules https://github.com/angelscript-tools/angelscript-jit.git
cd angelscript-jit
```

Or vendor it as a Git submodule of your repository:

```bash
cd whatever/vendor/directory/
git submodule add https://github.com/angelscript-tools/angelscript-jit.git
```

**Example project:** [`example/hello/`](../example/hello)

## Dependencies

Hard dependencies:

- [**AngelScript**](https://www.angelcode.com/angelscript/) v2.37.0+ (the `asIJITCompilerV2` interface; releases 2.31.2 to 2.36.1 use interface V1).
- [**fmt**](https://github.com/fmtlib/fmt).
- [**MIR**](https://github.com/vnmakarov/mir) v1.0, from a [downstream fork](https://github.com/angelscript-tools/mir) (see the warning below). Not needed on 32-bit targets (`ASEA_NO_MIR`).

Optional: [**Catch2**](https://github.com/catchorg/Catch2/tree/devel) for testing and [**nanobench**](https://github.com/martinus/nanobench/) for benchmarks.

> [!NOTE]
> You could pull Angelsea into your tree without CMake and set the include directories yourself. This is not tested or supported.

### 1. Use the vendored versions

By default the dependencies are vendored as Git submodules. When you add Angelsea as a CMake subdirectory you can link the targets it provides for them. To use the AngelScript version we vendor in your own project, link `asea_angelscript`.

> [!WARNING]
> Upstream MIR has not seen activity in a year. A downstream fork works around specific problems:
> - The load/store optimizations of the GVN pass broke on various occasions with the generated code, so they moved to a "-O3" level. The default is `config.mir_optimization_level = 2`; changing it is strongly discouraged.
> - Fixes [integer sign-extension miscompile](https://github.com/vnmakarov/mir/issues/423) and [jump optimization use-after-free with label references](https://github.com/vnmakarov/mir/issues/424). With upstream MIR, set `config.mir_optimization_level = 1;` as a workaround.
> - High memory usage: a hack, `config.hack_mir_minimize` (defaults to true).

```bash
git submodule update --init --recursive vendor/angelscript
git submodule update --init --recursive vendor/mir
git submodule update --init --recursive vendor/fmt
# to run the tests
git submodule update --init --recursive tests/vendor/*
```

### 2. Provide dependencies yourself (optional)

The current steps work for static library builds, not for dynamic libraries or the tests.

| Dependency | CMake flags |
| --- | --- |
| AngelScript | `-DASEA_ANGELSCRIPT_SYSTEM=1 -DASEA_ANGELSCRIPT_ROOT=/path/to/angelscript/sdk/` (default `vendor/angelscript/sdk`) |
| fmt | `-DASEA_FMT_SYSTEM=1` to use `find_package(fmt)`, or also `-DASEA_FMT_EXTERNAL=1 -DASEA_FMT_ROOT=/path/to/fmt` (default `vendor/fmt`) |
| MIR | `-DASEA_MIR_SYSTEM=1 -DASEA_MIR_ROOT=/path/to/mir/` (default `vendor/mir`) |

## Build the standalone static library

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

On Linux the library is `build/libangelsea.a`. With vendored dependencies, link also:

- `build/libasea_mir.a`
- `build/vendor/fmt/libfmt.a` (`libfmtd.a` in debug)
- `build/vendor/angelscript/sdk/angelscript/projects/cmake/libangelscript.a`

## Include as a CMake subdirectory

```cmake
add_subdirectory(path/to/angelsea)
target_link_libraries(yourexecutable PRIVATE angelsea)
```

This builds the vendored dependencies. To provide one yourself, set the flag **before** `add_subdirectory`:

```cmake
set(ASEA_FMT_SYSTEM OFF CACHE BOOL "")
```

Cache variables are sticky; clear the workspace if the build errors are confusing.
