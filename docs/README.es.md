# angelscript-jit

**[English](../README.md)** | **[Español](README.es.md)**

Un compilador JIT para AngelScript, escrito en C++20 sobre el runtime [MIR](https://github.com/vnmakarov/mir). Los scripts con mucho cálculo se ejecutan entre 6 y 11 veces más rápido que en el intérprete. Es un fork mantenido de [asumagic/angelsea](https://github.com/asumagic/angelsea) (BSD-2-Clause) que mantiene el JIT funcionando en la versión actual de AngelScript. Es software en fase alfa, igual que el original.

La biblioteca, el destino de CMake (`angelsea`) y el espacio de nombres de C++ conservan los nombres del original, para poder enviar los cambios de vuelta.

## Versiones compatibles

El JIT necesita la interfaz `asIJITCompilerV2`, que AngelScript tiene desde la 2.37.0. Cada versión de la tabla ejecuta la suite de Catch2 y la prueba diferencial (los mismos scripts en la VM normal y con el JIT deben dar la misma salida, el mismo estado y la misma excepción).

| AngelScript | Windows x64 | Linux x64 | Linux arm64 | macOS arm64 | macOS x64 | x86 de 32 bits |
| --- | --- | --- | --- | --- | --- | --- |
| 2.39.0 (actual) | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | solo AOT |
| 2.38.0 | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | ❌ |
| 2.37.0 | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | ❌ |
| 2.36 y anteriores | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |

- ✅ probado en cada push. ⚠️ se compila y prueba en CI como trabajo experimental. ❌ no compatible.
- Windows se prueba con MSVC y Linux con gcc. MinGW y clang funcionan en el original.
- macOS x64 falla en MIR; hay un job de CI experimental. El x86 de 32 bits no tiene backend en MIR, así que allí el JIT se compila sin él (`ASEA_NO_MIR`, automático con punteros de 32 bits) y solo funciona la vía AOT. Alternativa en 32 bits: el [JIT de BlindMind Studios](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler).
- Las versiones anteriores a la 2.37.0 no tienen la interfaz V2, así que todavía no hay JIT para ellas. [angelscript-cli](https://github.com/Gaftherman/angelscript-cli) indica cuáles de sus compilaciones llevan el JIT.

## Qué añade este fork

- **Las excepciones de script que lanzan las funciones de la aplicación ya no se pierden.** `CScriptArray::At` (un índice fuera de rango) y cualquier función que llame a `asIScriptContext::SetException` se ignoraban en las llamadas directas al sistema, porque el JIT nunca fijaba `m_callingSystemFunction`, que `SetException` exige. El script seguía con la referencia nula que devolvía la función y fallaba. Ahora el JIT la fija y la limpia alrededor de las llamadas directas y, como la VM, vuelve en cuanto el contexto deja de estar activo.
- **Lecturas y escrituras en aarch64 con desplazamientos sin alinear.** AngelScript alinea las propiedades de un objeto a 4 bytes, así que un `double` puede quedar en el desplazamiento 12. El backend aarch64 de MIR no sabía codificar ese acceso y abortaba; el arreglo está en `vendor/mir`.
- `hack_ignore_context_inspect` está **desactivado** por defecto, así que el puntero de programa se guarda antes de las llamadas directas y las excepciones indican la línea y la pila de llamadas correctas. Actívalo por velocidad si tus funciones nunca lanzan excepciones de script ni inspeccionan el contexto.
- AngelScript 2.37 vuelve a compilar: `asCScriptFunction::IsVariadic` existe desde la 2.38, y `GetDeclaredAt` de la 2.37 lee fuera de rango en funciones sin sección.
- **Interfaz JIT V1 y AOT.** Las versiones 2.31.2 a 2.36.1 funcionan mediante `asIJITCompiler`. `Jit::ExportModuleC` escribe el C de un módulo para compilarlo por adelantado con clang o gcc en una biblioteca compartida, y `Jit::LoadAot` la instala (sin compilar con MIR al arrancar; `nbody` un 25% más rápido que el código de MIR). Véase `docs/DESIGN.md` (en inglés).
- Una **prueba diferencial** (`tests/differential`). `generate.py --seed N` escribe programas aleatorios (aritmética, bucles, recursión, clases e interfaces, arreglos y cadenas, try/catch), de modo que un fallo se reproduce con la semilla.
- `vendor/mir` apunta a un fork de asumagic/mir que lleva el arreglo de aarch64, y `vendor/angelscript` está fijado a la v2.39.0.

## Estado

Hay una suite de pruebas que se ejecuta en CI y probamos el JIT con una compilación de desarrollo de [KAG](https://store.steampowered.com/app/219830/King_Arthurs_Gold). La rama `main` suele ser razonablemente estable.

Frente al [JIT de BlindMind Studios](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler), el proyecto original midió entre un 0 y un 40 % más de rendimiento en una aplicación real.

El tiempo de compilación y la memoria del JIT pueden ser mayores que los de ese JIT, pero:

- La compilación perezosa está activada por defecto: las funciones frías no se compilan nunca.
- La compilación asíncrona se activa con `Jit::SetCompileCallback`. Solo un hilo genera código a la vez, pero casi todo el proceso es asíncrono.
- Las funciones enormes (miles de líneas) se omiten por defecto, porque el coste de MIR crece rápido con el tamaño de la función. El límite se puede ajustar.

## Plataformas compatibles

|    | SO       | Arquitectura | ABI   | Notas |
|----|----------|--------------|-------|-------|
| ✅ | Linux    | x86-64       | gcc   | Probado en una aplicación real en el original |
| ✅ | Windows  | x86-64       | MSVC  | Probado aquí en cada push |
| ✅ | Windows  | x86-64       | MinGW | Probado en una aplicación real en el original |
| ⚠️ | Linux    | aarch64      | gcc   | Experimental |
| ⚠️ | macOS    | aarch64      | Apple | Experimental |
| ❌ | macOS    | x86-64       | Apple | Falla en MIR. Ver [asumagic/angelsea#3](https://github.com/asumagic/angelsea/issues/3) |
| ⚠️ | Windows, Linux | x86 de 32 bits | MSVC, gcc | Solo AOT: MIR no tiene backend; las bibliotecas AOT funcionan (29 de 29 scripts del corpus en ambos) |
| ❓ | Linux    | riscv64, ppc64le | gcc | MIR lo admite, sin probar |
| ❌ | Linux    | s390x        | gcc   | No se admite big-endian |

## Compilar y usar

La compilación (con las dependencias incluidas o propias, como biblioteca estática o como submódulo de CMake) y el uso de `JitConfig` se describen en el [README en inglés](../README.md#clone--build). Las instrucciones son idénticas.

## Documentación

- [Cómo ejecutar las pruebas](TESTS.md)
- [Cómo funciona y por qué](DESIGN.md) (en inglés)
- [Benchmarks](BENCHMARKS.md) (en inglés): velocidad frente al intérprete, a C++ nativo y a otros JIT, y sus límites
- [Cómo contribuir](CONTRIBUTING.es.md)
- [Avisos de terceros](THIRD_PARTY.md)

## Proyectos relacionados

| Proyecto | Uso |
| --- | --- |
| [angelscript-cli](https://github.com/Gaftherman/angelscript-cli) | Ejecuta, comprueba y depura scripts desde la terminal. Sus compilaciones con JIT usan este repositorio |
| [angelscript-engine](https://github.com/Gaftherman/angelscript-engine) | Compilaciones del SDK parcheado de versiones anteriores de AngelScript |
| [asumagic/angelsea](https://github.com/asumagic/angelsea) | El proyecto original |

## Licencia

[BSD-2-Clause](../LICENSE), como el original. Los avisos de terceros están en [THIRD_PARTY.md](THIRD_PARTY.md).
