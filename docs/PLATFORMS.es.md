# Plataformas

**[English](PLATFORMS.md)** | **[Español](PLATFORMS.es.md)**

## Sistemas y arquitecturas

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

## Notas

- Windows se prueba con MSVC y Linux con gcc. MinGW y clang funcionan en el original.
- macOS x64 falla en MIR; hay un job de CI experimental.
- El x86 de 32 bits no tiene backend en MIR, así que allí el JIT se compila sin él (`ASEA_NO_MIR`, automático con punteros de 32 bits) y solo funciona la vía AOT: consulta [Benchmarks](BENCHMARKS.md). Alternativa en 32 bits: el [JIT de BlindMind Studios](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler).
- Las versiones anteriores a la 2.37.0 usan la interfaz versión 1 mediante un adaptador pequeño (`Jit::CompileFunction`): las funciones se registran cuando el motor las termina y se compilan de forma perezosa, como en V2. El motor no indica a qué función pertenecía una función JIT liberada, así que los registros de las funciones descartadas con su módulo permanecen hasta que se destruye el motor. La 2.30.2 y anteriores no compilan (`asITypeInfo` y la disposición de tipos difieren). Las versiones inferiores a la 2.33 no tienen `try`/`catch`, así que los scripts diferenciales que lo usan no se ejecutan allí. [angelscript-cli](https://github.com/angelscript-tools/angelscript-cli) indica cuáles de sus compilaciones llevan el JIT.
- Cada versión marcada en el README ejecuta la prueba diferencial (los mismos scripts en la VM normal y con el JIT deben dar la misma salida, el mismo estado y la misma excepción); la 2.37.0 y posteriores ejecutan además la suite de Catch2.

## Estado y coste de compilación

La suite de pruebas se ejecuta en CI y el JIT se prueba con una compilación de desarrollo de [KAG](https://store.steampowered.com/app/219830/King_Arthurs_Gold). La rama `main` suele ser razonablemente estable.

Frente al [JIT de BlindMind Studios](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler), el proyecto original midió entre un 0 y un 40 % más de rendimiento en una aplicación real. El tiempo de compilación y la memoria pueden ser mayores:

- La compilación perezosa está activada por defecto: las funciones frías no se compilan nunca.
- La compilación asíncrona se activa con `Jit::SetCompileCallback`. Solo un hilo genera código a la vez, pero casi todo el proceso es asíncrono.
- Las funciones enormes (miles de líneas) se omiten por defecto, porque el coste de MIR crece rápido con el tamaño de la función. El límite se puede ajustar.

## Qué añade este fork, en detalle

- **Las excepciones de script que lanzan las funciones de la aplicación ya no se pierden.** `CScriptArray::At` (un índice fuera de rango) y cualquier función que llame a `asIScriptContext::SetException` se ignoraban en las llamadas directas al sistema, porque el JIT nunca fijaba `m_callingSystemFunction`, que `SetException` exige. Ahora el JIT la fija y la limpia alrededor de las llamadas directas y, como la VM, vuelve en cuanto el contexto deja de estar activo.
- **Lecturas y escrituras en aarch64 con desplazamientos sin alinear.** AngelScript alinea las propiedades de un objeto a 4 bytes, así que un `double` puede quedar en el desplazamiento 12. El backend aarch64 de MIR no sabía codificar ese acceso y abortaba; el arreglo está en `vendor/mir`.
- `hack_ignore_context_inspect` está **desactivado** por defecto, así que el puntero de programa se guarda antes de las llamadas directas y las excepciones indican la línea y la pila de llamadas correctas. Actívalo por velocidad si tus funciones nunca lanzan excepciones de script ni inspeccionan el contexto.
- AngelScript 2.37 vuelve a compilar: `asCScriptFunction::IsVariadic` existe desde la 2.38, y `GetDeclaredAt` de la 2.37 lee fuera de rango en funciones sin sección.
- **Las variables de la VM viven en registros** (`experimental_register_locals`) y las llamadas a un destino conocido se preparan en línea: los bucles numéricos van hasta 2,5 veces más rápido que antes y `fib` un 16-20 % más. Consulta [Benchmarks](BENCHMARKS.md).
- **Interfaz JIT V1 y AOT.** Las versiones 2.31.2 a 2.36.1 funcionan mediante `asIJITCompiler`. `Jit::ExportModuleC` escribe el C de un módulo para compilarlo por adelantado con clang, gcc o MSVC en una biblioteca compartida, y `Jit::LoadAot` la instala (sin compilar con MIR al arrancar; `nbody` un 25 % más rápido que el código de MIR). Véase [DESIGN.md](DESIGN.md) (en inglés).
- Tres defectos de MIR corregidos en `vendor/mir` (renumeración de operandos tras un intercambio de propagación de copias, un desbordamiento de operandos en la reescritura de derrames y un aborto silencioso por aserción en aarch64).
- Una **prueba diferencial** (`tests/differential`). `generate.py --seed N` escribe programas aleatorios (aritmética, bucles, recursión, clases e interfaces, arreglos y cadenas, try/catch), de modo que un fallo se reproduce con la semilla.
- `vendor/mir` apunta a un fork de asumagic/mir que lleva el arreglo de aarch64, y `vendor/angelscript` está fijado a la v2.39.0.
