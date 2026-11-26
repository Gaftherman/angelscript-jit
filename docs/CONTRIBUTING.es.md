# Contribuir a angelscript-jit

**[English](../CONTRIBUTING.md)** | **[Español](CONTRIBUTING.es.md)**

Gracias por ayudar. Los informes de errores, las correcciones, las pruebas y la documentación son bienvenidos.

## Informar de un problema

Abre una [incidencia](https://github.com/Gaftherman/angelscript-jit/issues) con la versión de AngelScript, tu sistema operativo y arquitectura, el compilador y el script que falla. Si el mismo script funciona bien en el intérprete, indícalo: es un error del JIT. `tests/differential` muestra cómo comparar ambos.

Un problema del propio MIR, o que también ocurra en [asumagic/angelsea](https://github.com/asumagic/angelsea), conviene informarlo también allí.

## Proponer un cambio

1. Haz un fork del repositorio y crea una rama desde `main`.
2. Mantén cada cambio pequeño y centrado. Un cambio en el código generado necesita un script que falle sin él.
3. Escribe los mensajes de commit como `tipo(ámbito): descripción corta en imperativo` sin punto final. Tipos: `feat`, `fix`, `test`, `perf`, `refactor`, `style`, `docs`, `chore`.
4. Abre una pull request que explique qué cambió y cómo lo comprobaste. Un cambio de velocidad necesita los números del benchmark antes y después.

## Compilar y probar

~~~sh
git clone --recurse-submodules https://github.com/Gaftherman/angelscript-jit.git
cd angelscript-jit
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DASEA_ENABLE_TESTING=ON
cmake --build build
(cd tests && ../build/tests/angelsea-tests "~[benchmark]")
ctest --test-dir build/tests -R differential --output-on-failure
~~~

Para probar otra versión de AngelScript, apunta `ASEA_ANGELSCRIPT_ROOT` a una carpeta de SDK (la que contiene `angelscript/` y `add_on/`). [angelscript-engine](https://github.com/Gaftherman/angelscript-engine) prepara las versiones anteriores con `scripts/prepare-sdk.py`.

Antes de enviar un cambio al generador de código, ejecuta también el corpus generado:

~~~sh
cd tests
python differential/generate.py --seed 1 --count 120 --out build-gen
../build/tests/angelsea-differential build-gen
~~~

Hay más sobre la suite en [TESTS.md](TESTS.md).

## Estilo

C++20. Sigue `.clang-format`. Los ayudantes de Python usan Python 3 sin dependencias.

## Licencia

Al contribuir aceptas que tu trabajo se publique con la [licencia BSD-2-Clause](../LICENSE).

## Proyectos relacionados

| Proyecto | Uso |
| --- | --- |
| [angelscript-cli](https://github.com/Gaftherman/angelscript-cli) | Ejecuta, comprueba y depura scripts desde la terminal |
| [angelscript-engine](https://github.com/Gaftherman/angelscript-engine) | Compilaciones del SDK parcheado de versiones anteriores de AngelScript |
| [angelscript-lsp](https://github.com/Gaftherman/angelscript-lsp) | Compatibilidad de editor con VS Code |
