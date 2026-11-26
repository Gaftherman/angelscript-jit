# Usage

**[English](USAGE.md)** | **[Español](USAGE.es.md)**

The JIT implements the [`asIJITCompilerV2`](https://www.angelcode.com/angelscript/sdk/docs/manual/classas_i_j_i_t_compiler_v2.html) interface (and version 1 for AngelScript 2.31.2 to 2.36.1).

```cpp
#include <angelsea.hpp>

engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true);
engine->SetEngineProperty(asEP_JIT_INTERFACE_VERSION, 2);
engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

angelsea::JitConfig config;
angelsea::Jit jit(config, *engine);
assert(engine->SetJITCompiler(&jit) >= 0);
```

Check the [`JitConfig` tunables](../include/angelsea/config.hpp) and adjust them for your application. The defaults suit a semi-heavily scripted commercial application.

To disable lazy compilation:

```cpp
angelsea::JitConfig config {
    .triggers = {
        .hits_before_func_compile = 0,
    }
};
```

Per-function tunables can be set with a callback. For an example that uses script builder metadata, see [`tests/configtest.cpp`](../tests/configtest.cpp).

> [!WARNING]
> The JIT compiler is not thread-safe with regard to multithreaded AngelScript contexts or engines.

## Ahead of time

`Jit::ExportModuleC` writes `<base>.c` and `<base>.externs`. Build the C with gcc, clang or MSVC into a shared library, then install it with `Jit::LoadAot`. See [DESIGN.md](DESIGN.md).
