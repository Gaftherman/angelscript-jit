# Third-party notices

Project code is BSD-2-Clause licensed, like [asumagic/angelsea](https://github.com/asumagic/angelsea), which this repository forks. Keep the attribution in source **and** binary distributions.

- [angelsea](https://github.com/asumagic/angelsea): the JIT compiler this repository forks (BSD-2-Clause, copyright asumagic).
- [MIR](https://github.com/vnmakarov/mir): the JIT runtime and C11 compiler (MIT License, copyright Vladimir Makarov), in `vendor/mir`. The fix for unaligned aarch64 offsets is a change to this code, under the same license.
- [AngelScript](https://github.com/anjo76/angelscript): the engine, in `vendor/angelscript` (zlib license, copyright Andreas Jönsson).
- [{fmt}](https://github.com/fmtlib/fmt): formatting, in `vendor/fmt` (MIT License, with an optional exception for binary distributions).
- [Catch2](https://github.com/catchorg/Catch2) and [nanobench](https://github.com/martinus/nanobench): tests and benchmarks only, in `tests/vendor` (BSL-1.0 and MIT).
