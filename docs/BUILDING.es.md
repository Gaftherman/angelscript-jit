# Compilar

**[English](BUILDING.md)** | **[Español](BUILDING.es.md)**

Clona el repositorio:

```bash
git clone --recurse-submodules https://github.com/Gaftherman/angelscript-jit.git
cd angelscript-jit
```

O inclúyelo como submódulo de Git en tu repositorio:

```bash
cd whatever/vendor/directory/
git submodule add https://github.com/Gaftherman/angelscript-jit.git
```

**Proyecto de ejemplo:** [`example/hello/`](../example/hello)

## Dependencias

Dependencias obligatorias:

- [**AngelScript**](https://www.angelcode.com/angelscript/) v2.37.0 o posterior (la interfaz `asIJITCompilerV2`; las versiones 2.31.2 a 2.36.1 usan la interfaz V1).
- [**fmt**](https://github.com/fmtlib/fmt).
- [**MIR**](https://github.com/vnmakarov/mir) v1.0, de un [fork derivado](https://github.com/asumagic/mir) (consulta la advertencia). No hace falta en destinos de 32 bits (`ASEA_NO_MIR`).

Opcionales: [**Catch2**](https://github.com/catchorg/Catch2/tree/devel) para las pruebas y [**nanobench**](https://github.com/martinus/nanobench/) para los benchmarks.

> [!NOTE]
> Podrías copiar Angelsea a tu árbol sin CMake y configurar tú mismo los directorios de inclusión. No se prueba ni se admite.

### 1. Usar las versiones incluidas

Por defecto las dependencias se incluyen como submódulos de Git. Al añadir Angelsea como subdirectorio de CMake puedes enlazar los destinos que ofrece para ellas. Para usar en tu proyecto la versión de AngelScript que incluimos, enlaza `asea_angelscript`.

> [!WARNING]
> El MIR original lleva un año sin actividad. Un fork derivado evita problemas concretos:
> - Las optimizaciones de carga y almacenamiento del paso GVN fallaron en varias ocasiones con el código generado, así que pasaron a un nivel "-O3". El valor por defecto es `config.mir_optimization_level = 2`; no se recomienda cambiarlo.
> - Corrige la [mala compilación de la extensión de signo de enteros](https://github.com/vnmakarov/mir/issues/423) y el [uso tras liberación de la optimización de saltos con referencias a etiquetas](https://github.com/vnmakarov/mir/issues/424). Con el MIR original, usa `config.mir_optimization_level = 1;` como solución provisional.
> - Memoria alta: un truco, `config.hack_mir_minimize` (activado por defecto).

```bash
git submodule update --init --recursive vendor/angelscript
git submodule update --init --recursive vendor/mir
git submodule update --init --recursive vendor/fmt
# para ejecutar las pruebas
git submodule update --init --recursive tests/vendor/*
```

### 2. Aportar tus propias dependencias (opcional)

Los pasos actuales sirven para bibliotecas estáticas, no para bibliotecas dinámicas ni para las pruebas.

| Dependencia | Opciones de CMake |
| --- | --- |
| AngelScript | `-DASEA_ANGELSCRIPT_SYSTEM=1 -DASEA_ANGELSCRIPT_ROOT=/ruta/a/angelscript/sdk/` (por defecto `vendor/angelscript/sdk`) |
| fmt | `-DASEA_FMT_SYSTEM=1` para usar `find_package(fmt)`, o además `-DASEA_FMT_EXTERNAL=1 -DASEA_FMT_ROOT=/ruta/a/fmt` (por defecto `vendor/fmt`) |
| MIR | `-DASEA_MIR_SYSTEM=1 -DASEA_MIR_ROOT=/ruta/a/mir/` (por defecto `vendor/mir`) |

## Compilar la biblioteca estática independiente

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

En Linux la biblioteca es `build/libangelsea.a`. Con las dependencias incluidas, enlaza también:

- `build/libasea_mir.a`
- `build/vendor/fmt/libfmt.a` (`libfmtd.a` en depuración)
- `build/vendor/angelscript/sdk/angelscript/projects/cmake/libangelscript.a`

## Incluir como subdirectorio de CMake

```cmake
add_subdirectory(ruta/a/angelsea)
target_link_libraries(tuejecutable PRIVATE angelsea)
```

Esto compila las dependencias incluidas. Para aportar una tú, fija la opción **antes** de `add_subdirectory`:

```cmake
set(ASEA_FMT_SYSTEM OFF CACHE BOOL "")
```

Las variables de caché son persistentes; limpia el espacio de trabajo si los errores de compilación resultan confusos.
