# Benchmarks

How much faster the generated code is than the AngelScript interpreter, than the previous state of this fork, than
upstream angelsea, than the BlindMind Studios JIT, and than native C++ compiled with `-O2`.

## Method

- Scripts: `tests/bench/*.as`. Native equivalents of each one: `tests/bench/native/ceiling.cpp`.
- Runner: `python tests/bench/run.py --diff <angelsea-differential> --native <ceiling>`. It runs every script
  `--runs` times interleaved, each run being the best of `--inner` executions, and reports the median and the p95.
- The VM and the JIT run in the same process on the same engine, so the ratio does not depend on the release.
  Absolute milliseconds only compare inside one table.
- Machine: Windows 11, x64, MSVC 19.5 `/O2`, AngelScript 2.39.0, MIR level 2. Linux x64 and arm64 are checked for
  correctness (differential corpus, Catch2) on every change, not timed.

## Results (this fork, 2.39.0, Windows x64, ms)

| Benchmark | VM | JIT 0.2.0 | JIT now | Now / VM | Native C++ | Native / JIT now |
| --- | --- | --- | --- | --- | --- | --- |
| loops | 502 | 44.8 | 17.9 | x28.0 | 25.2 | 140% |
| matmul | 134 | 28.4 | 20.8 | x6.4 | 2.2 | 11% |
| sieve | 215 | 59.4 | 57.3 | x3.8 | 15.8 | 28% |
| nbody | 348 | 188.7 | 105.4 | x3.3 | 50.7 | 48% |
| virtual | 222 | 82.2 | 80.9 | x2.7 | 4.6 | 6% |
| objects | 231 | 90.5 | 89.0 | x2.6 | 3.9 | 4% |
| fib(32) | 135 | 48.8 | 41.1 | x3.3 | 7.7 | 19% |
| closures | 171 | 99.8 | 81.0 | x2.1 | 4.3 | 5% |
| strings | 203 | 140.7 | 139.7 | x1.46 | 8.6 | 6% |

`loops` beats the native build because MSVC does not vectorise that loop; read it as "at the ceiling".
`fib` gained 16% from the inline call setup below. `virtual` reads about 5% slower than the previous build in an interleaved
comparison with identical generated code (layout noise of the runtime); the rest did not get worse.

What changed since 0.2.0: the setup of a script call whose callee is known at compile time is generated inline (call
stack push, stack check, frame registers) and only falls back to the runtime helper when the call stack or the stack
block must grow; promotion of VM variables to C locals (`experimental_register_locals`) and three MIR fixes
(see CHANGELOG). Numeric loops gain the most; code dominated by calls into the interpreter or into application
functions (strings, interface calls) gains nothing because the time is spent there, not in the generated code.

## Linux x64 (gcc 13, Docker on the same machine, AngelScript 2.39.0, ms)

| Benchmark | VM | JIT | Now / VM | Native C++ | Native / JIT |
| --- | --- | --- | --- | --- | --- |
| loops | 837 | 18.8 | x44.6 | 26.4 | 141% |
| strings | 164 | 42.9 | x3.8 | 9.2 | 21% |
| sieve | 276 | 81.0 | x3.4 | 15.4 | 19% |
| matmul | 143 | 40.8 | x3.5 | 1.0 | 2% |
| nbody | 586 | 212.8 | x2.75 | 51.5 | 24% |
| objects | 363 | 168.9 | x2.15 | 6.6 | 4% |
| virtual | 335 | 159.5 | x2.1 | 4.0 | 3% |
| fib(32) | 169 | 82.3 | x2.05 | 4.0 | 5% |
| closures | 265 | 146.6 | x1.8 | 5.6 | 4% |

Linux runs inside a container on a Windows host, so absolute numbers are higher than the Windows table; compare the
ratios. `strings` gains x3.8 here against x1.46 on Windows on the same script; the cause was not isolated.
AOT (`tests/aot/run.sh`, gcc) also passes the whole corpus on Linux arm64 under qemu; arm64 is verified for correctness under qemu and not timed (emulation says nothing about speed).

## AOT (`ExportModuleC` + `LoadAot`, ms)

Same scripts, the JIT running only the library built from the export (`ASEA_HITS=999999999`, so MIR never compiles) against
the MIR code of the same build. Library compiled with `-O2`.

| Benchmark | Windows x64 clang | MIR (same run) | Linux x64 gcc 13 | MIR (same run) |
| --- | --- | --- | --- | --- |
| loops | 15.1 | 17.9 | - | - |
| nbody | 78.9 | 104.4 | 159.3 | 209.8 |
| matmul | 16.5 | 20.4 | 36.6 | 41.0 |
| fib(32) | 35.4 | 43.1 | 83.0 | 82.4 |
| sieve | 56.1 | 58.6 | - | - |

The compiler optimises the arithmetic better than MIR (nbody -25%, loops -16%), but the cost that remains is the same
in both: calls into the runtime, reference counting and application functions, which the AOT code reaches through the
same table of addresses. AOT is therefore worth it for numeric scripts and for avoiding the 4.5 ms compile cost per
function at start-up; it does not approach native C++ on call-heavy code.

## 32-bit x86 (AOT only, ms)

MIR has no 32-bit backend, so on x86 the JIT is built without it (`ASEA_NO_MIR`) and the only compiled code is an AOT
library: export the module, build the C with gcc or MSVC, load it. Everything not in the library runs in the
interpreter. Same scripts, `--time 3`, speed-up over the VM of the same build, AngelScript 2.39.0.

| Benchmark | Windows x86 (MSVC `cl /O2`) | Linux x86 (gcc 13 -O2, Docker) |
| --- | --- | --- |
| loops | x29.3 | x40.8 |
| matmul | x8.0 | x5.5 |
| sieve | x4.1 | x3.75 |
| fib(32) | x3.4 | x2.07 |
| nbody | x2.2 | x3.83 |

All 29 runnable scripts of the corpus match the VM on both. Application method calls (`array::opIndex`, `string`
members) are direct calls: on gcc the generic ABI (`this` as the first stack argument) is the Itanium i386 convention;
on MSVC `thiscall` is reached from C as `__fastcall` with a dummy EDX argument (`this` in ECX, everything else on the
stack). Virtual `thiscall` methods and `thiscall` with a by-value object return go through the runtime helper on both,
which is correct but slower; before the MSVC calling convention was added, `matmul` and `sieve` ran at x0.4 on Windows
x86 for that reason.

A runtime compiler for 32-bit was measured and rejected: compiling the generated C with TinyCC 0.9.27 in-process gives
`loops` x10 but `fib` x0.53, `nbody` x0.85, `sieve` x1.29 and `matmul` x1.41 (Linux x64, same code, MIR in the same run:
x44, x2.1, x2.7, x3.4, x3.5), so a TinyCC backend would be slower than the interpreter on call-heavy code.

## Against other JITs (same scripts, x64)

| Benchmark | This fork now | Upstream angelsea | BlindMind (2.30.2) |
| --- | --- | --- | --- |
| loops | x28.0 | x12.0 | x9.3 |
| matmul | x6.4 | x6.5 | x0.56 |
| sieve | x3.8 | x4.5 | x0.68 |
| nbody | x3.3 | x1.9 | x1.75 |
| virtual | x2.7 | x2.5 | x0.59 |
| objects | x2.6 | x2.2 | x0.58 |
| fib(32) | x3.3 | x2.0 | x1.44 |
| closures | x2.1 | x1.9 | x0.53 |
| strings | x1.46 | x1.48 | x1.04 |

Speed-up over the interpreter of the same build (higher is better; below 1 is slower than the interpreter).

- **Upstream angelsea** is commit `1d367d4` of this repository, built and measured with the same runner. It does
  not pass this fork's exception and null-handle tests, and it is faster than this fork on `sieve` (x4.5 against
  x3.8) and `closures`. The cause was not isolated; the fork's direct application calls keep the context valid
  for script exceptions, which upstream does not.
- **BlindMind Studios** (`BlindMindStudios/AngelScript-JIT-Compiler`) only builds against old releases: measured with
  AngelScript 2.30.2 and one source change (`auxiliary` was renamed `objForThiscall`) on x64. It is slower than the
  interpreter on everything that calls an application function or the array add-on (`sieve`, `matmul`, `objects`,
  `virtual`, `closures`), and faster on pure arithmetic. It targets the `asIJITCompiler` V1 interface.
- **sven_jit** (x86-32, V1, GPL-3) was not measured: it needs a 32-bit build against 2.36.1 and the code cannot be
  reused under this license. It is x86-32 only, like BlindMind it is a template JIT.

## JIT interface V1 (releases before 2.37, Windows x64)

Speed-up over the interpreter of the same release, `angelsea-differential --time 3`:

| Release | loops | fib(32) | sieve | nbody | matmul |
| --- | --- | --- | --- | --- | --- |
| 2.36.1 | x28.7 | x3.5 | x3.9 | x3.9 | x6.2 |
| 2.35.1 | x31.8 | x3.4 | x4.8 | x4.1 | x7.0 |
| 2.34.0 | x23.7 | x2.3 | x3.1 | x3.1 | x4.9 |
| 2.33.0 | x20.4 | x2.2 | x2.9 | x3.1 | x5.0 |
| 2.32.0 | x23.8 | x2.4 | x3.2 | x2.9 | x5.3 |
| 2.31.2 | x19.6 | x2.3 | x3.2 | x3.0 | x5.2 |

The ratio depends on how fast each release's interpreter is, so only compare rows of the same column loosely. Single
runs of `--time 3`, not the interleaved median of `run.py`.

## What was done, what it cost, what is left

| Item | Result | Complexity |
| --- | --- | --- |
| Variables in registers | loops x2.5, nbody x1.8, matmul x1.35 | High: per-function dataflow, spill rules, three MIR defects in `vendor/mir` |
| Inline call setup | fib -16..20% | Medium |
| Native `FuncPtr`, `**`, `CallPtr` | closures -15%, no interpreter round trip for `**` | Low to medium |
| JIT interface V1 | 2.31.2 to 2.36.1 work, same speed-ups | High: one header of per-release differences, internals differ |
| AOT export and load | nbody -25% against MIR, no compile at start-up | High: every address became a table slot |
| MIR level 3, compile thresholds | no gain, defaults kept | Measured only |
| Helpers instead of the remaining interpreter fallbacks (`CALLBND`, list instructions) | not done: rare in the corpus | - |
| Inlining of small callees | not done: it would fuse frames and change what `GetCallstack` and exceptions see; the inline call setup already took the cheap part | Very high |
| `array<T>` bounds-check hoisting | not done: depends on the add-on's layout in each release | High |
| Releases before 2.31.2 | do not build (no `asIStringFactory`-era API, older internals) | - |

## Limits

- Calls into application functions (`string`, `dictionary`, `array::opIndex`) cost what the C++ costs; the JIT removes
  only the interpretation around them.
- Reference counting and handle bookkeeping are not eliminated (no escape analysis): `objects`, `virtual`, `closures`.
- Variables promoted to registers are written back before every call into the interpreter and when control leaves the
  function, but not before every direct application call (`experimental_spill_on_native_calls`, off by default),
  so a callee that reads script variables through the context sees stale values.
- Functions with interface, bound or function pointer calls keep their variables in the frame: every such call returns
  to the interpreter and re-enters, and reloading all variables each time measured slower.
- The generated C is compiled by MIR (-O2). It reaches about half of what a native compiler gets on scalar numeric
  code and much less where the cost is memory traffic or calls.
- MIR optimisation level 3 was measured against level 2 on `loops`, `sieve`, `nbody`, `matmul` and `fib`: the differences
  (-5% to +6%) are inside the noise, so level 2 stays the default.
- Compile trigger (`hits_before_func_compile`, default 15000) measured with `ASEA_HITS` on `fib(22)` and `fib(27)`:
  compiling a function costs about 4.5 ms (C generation, c2m and MIR) whatever the threshold, and 15000 hits of a
  small function is under a millisecond of interpreter time, so thresholds of 100, 1000 and 15000 finish within 2%
  of each other. A program that runs less than about 5 ms in total is slower with the JIT than without it; the default
  is kept.
