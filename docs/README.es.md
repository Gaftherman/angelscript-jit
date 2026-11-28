# angelscript-jit

**[English](../README.md)** | **[Español](README.es.md)**

Un compilador JIT para AngelScript, escrito en C++20 sobre el runtime [MIR](https://github.com/vnmakarov/mir). Los scripts con mucho cálculo se ejecutan entre 6 y 11 veces más rápido que en el intérprete. Es un fork mantenido de [asumagic/angelsea](https://github.com/asumagic/angelsea) (BSD-2-Clause) que mantiene el JIT funcionando en la versión actual de AngelScript. Es software en fase alfa, igual que el original.

La biblioteca, el destino de CMake (`angelsea`) y el espacio de nombres de C++ conservan los nombres del original, para poder enviar los cambios de vuelta.

## Versiones compatibles

Interfaces: `asIJITCompilerV2` (AngelScript 2.37.0 y posteriores) y `asIJITCompiler` versión 1 (2.31.2 a 2.36.1).

| AngelScript | Windows x64 | Linux x64 | Linux arm64 | macOS arm64 | macOS x64 | x86 de 32 bits |
| --- | --- | --- | --- | --- | --- | --- |
| 2.39.0 (actual) | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | solo AOT |
| 2.38.0 | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | ❌ |
| 2.37.0 | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | ❌ |
| 2.36.1 a 2.31.2 (interfaz V1) | 🧪 | 🧪 | | | ❌ | ❌ |
| 2.30.2 y anteriores | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |

✅ probado en cada push. ⚠️ trabajo experimental de CI. 🧪 comprobado a mano, no en CI. ❌ no compatible. **Solo AOT**: sin compilador en tiempo de ejecución; carga una biblioteca generada con `Jit::ExportModuleC`. Los detalles, otras arquitecturas y notas están en [PLATFORMS.es.md](PLATFORMS.es.md).

## Qué añade este fork

- Las excepciones de script que lanzan las funciones de la aplicación ya no se pierden.
- Lecturas y escrituras en aarch64 con desplazamientos sin alinear, y tres defectos de MIR, corregidos en `vendor/mir`.
- Las variables de la VM viven en registros y las llamadas entre funciones de script se preparan en línea.
- Interfaz V1 (2.31.2 a 2.36.1) y **AOT**: `Jit::ExportModuleC` escribe el C de un módulo, cualquier compilador de C lo construye y `Jit::LoadAot` lo instala.
- x86 de 32 bits mediante AOT, con thiscall de MSVC.
- Una prueba diferencial con programas aleatorios (`tests/differential`).

## Uso

```cpp
#include <angelsea.hpp>

engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true);
engine->SetEngineProperty(asEP_JIT_INTERFACE_VERSION, 2);
engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

angelsea::JitConfig config;
angelsea::Jit jit(config, *engine);
assert(engine->SetJITCompiler(&jit) >= 0);
```

Los parámetros, las funciones de configuración por función y el límite de hilos están en [USAGE.es.md](USAGE.es.md). Un proyecto completo está en [`example/hello/`](../example/hello).

## Compilar desde el código fuente

```bash
git clone --recurse-submodules https://github.com/angelscript-tools/angelscript-jit.git
cd angelscript-jit
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Para usar tus propias dependencias o añadirlo como subdirectorio de CMake, consulta [BUILDING.es.md](BUILDING.es.md).

## Documentación

- [Compilar](BUILDING.es.md) — dependencias, incluidas o propias, biblioteca estática y subdirectorio.
- [Uso](USAGE.es.md) — `JitConfig`, compilación perezosa y asíncrona.
- [Plataformas](PLATFORMS.es.md) — sistemas y arquitecturas compatibles.
- [Diseño](DESIGN.md) (en inglés) — el backend en C, las convenciones de llamada, MIR y los otros JIT.
- [Benchmarks](BENCHMARKS.md) (en inglés) — velocidad frente al intérprete, a C++ nativo y a otros JIT.
- [Pruebas](TESTS.md) — cómo ejecutar la suite.
- [Hoja de ruta](ROADMAP.md) (en inglés) — planes y límites, escrita para agentes LLM.
- [CHANGELOG.md](../CHANGELOG.md) — historial de versiones.

## Hoja de ruta

- macOS x64: diagnosticar el fallo en CI y añadir AOT con `.dylib`.
- x86 de 32 bits: verificar 2.37, 2.38 y la interfaz V1.
- Rendimiento: inlining de funciones de script pequeñas, sacar las comprobaciones de límites de arreglos de los bucles y sustituir los opcodes que aún usan respaldo.

La lista completa está en [ROADMAP.md](ROADMAP.md).

## Soporte

Abre una [incidencia](https://github.com/angelscript-tools/angelscript-jit/issues) con la versión de AngelScript, la plataforma y un script corto que reproduzca el problema. Los problemas del ejecutable van a [angelscript-cli](https://github.com/angelscript-tools/angelscript-cli/issues). Para colaborar, lee [CONTRIBUTING.es.md](CONTRIBUTING.es.md).

## Relacionados

| Proyecto | Uso |
| --- | --- |
| [angelscript-cli](https://github.com/angelscript-tools/angelscript-cli) | Ejecuta, comprueba y depura scripts desde la terminal. Sus compilaciones con JIT usan este repositorio |
| [angelscript-lsp](https://github.com/angelscript-tools/angelscript-lsp) | Soporte del editor para VS Code |
| [angelscript-dap](https://github.com/angelscript-tools/angelscript-dap) | Núcleo del depurador |
| [angelscript-engine](https://github.com/angelscript-tools/angelscript-engine) | Compilaciones del SDK parcheado de versiones anteriores de AngelScript |
| [angelscript-debugger-example](https://github.com/angelscript-tools/angelscript-debugger-example) | Una aplicación que muestra cómo usar el depurador |

### Proyectos similares

- [asumagic/angelsea](https://github.com/asumagic/angelsea): el proyecto original del que es fork este repositorio.
- [JIT de BlindMind Studios](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler): el otro JIT de AngelScript, con un backend para x86 de 32 bits.

## Créditos

Los avisos de terceros están en [THIRD_PARTY.md](THIRD_PARTY.md).

| Proyecto | Licencia | Uso |
| --- | --- | --- |
| [asumagic/angelsea](https://github.com/asumagic/angelsea) | BSD-2-Clause | El JIT del que este repositorio es fork |
| [MIR](https://github.com/vnmakarov/mir) de Vladimir Makarov | MIT | Generador de código en tiempo de ejecución y compilador de C |
| [AngelScript](https://github.com/anjo76/angelscript) de Andreas Jönsson | zlib | El lenguaje y su motor de ejecución |
| [{fmt}](https://github.com/fmtlib/fmt) | MIT | Formato de texto |
| [Catch2](https://github.com/catchorg/Catch2), [nanobench](https://github.com/martinus/nanobench) | BSL-1.0, MIT | Pruebas y benchmarks |

## Licencia

Este repositorio está bajo la [licencia BSD de 2 cláusulas](../LICENSE), como el original.
