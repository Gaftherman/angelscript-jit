# Uso

**[English](USAGE.md)** | **[Español](USAGE.es.md)**

El JIT implementa la interfaz [`asIJITCompilerV2`](https://www.angelcode.com/angelscript/sdk/docs/manual/classas_i_j_i_t_compiler_v2.html) (y la versión 1 para AngelScript 2.31.2 a 2.36.1).

```cpp
#include <angelsea.hpp>

engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true);
engine->SetEngineProperty(asEP_JIT_INTERFACE_VERSION, 2);
engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

angelsea::JitConfig config;
angelsea::Jit jit(config, *engine);
assert(engine->SetJITCompiler(&jit) >= 0);
```

Revisa los [parámetros de `JitConfig`](../include/angelsea/config.hpp) y ajústalos a tu aplicación. Los valores por defecto sirven para una aplicación comercial con una cantidad moderada de scripts.

Para desactivar la compilación perezosa:

```cpp
angelsea::JitConfig config {
    .triggers = {
        .hits_before_func_compile = 0,
    }
};
```

También se pueden fijar parámetros por función con una función de retorno. Para un ejemplo con metadatos del script builder, consulta [`tests/configtest.cpp`](../tests/configtest.cpp).

> [!WARNING]
> El compilador JIT no es seguro entre hilos con contextos o motores de AngelScript multihilo.

## Por adelantado (AOT)

`Jit::ExportModuleC` escribe `<base>.c` y `<base>.externs`. Compila el C con gcc, clang o MSVC en una biblioteca compartida e instálala con `Jit::LoadAot`. Consulta [DESIGN.md](DESIGN.md) (en inglés).
