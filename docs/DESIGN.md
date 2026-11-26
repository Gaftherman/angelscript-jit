# Design notes

Questions and answers carried over from upstream angelsea about how the compiler is built.

## How does the JIT compiler work?

AngelScript uses a bytecode virtual machine, so a JIT compiler has to take this
bytecode and translate some or all of it to native code.  
Bytecode functions have one or more "JIT entry points" from which a JIT function
can start. This effectively allows JIT functions to drop down to the VM for
unsupported instructions, but still making it possible to be called again.

Angelsea compiles AngelScript bytecode to C functions. c2mir compiles that to
MIR in memory, and MIR ultimately emits machine code.

Lazy compilation works by giving AngelScript dummy JIT functions that merely
count how often they were called. Once the configurable threshold is reached,
compilation will be triggered, potentially asynchronously.

## What is the best supported calling convention?

Currently, the `asCALL_GENERIC` calling convention is the best supported (though
some things are not covered).

There is experimental support for the native calling convention (on by default).
Many cases should be covered, but some are omitted.

Supported system calls are much faster with the JIT with either the generic or
native calling convention.

There is also an experimental "stack elision" optimization
(`experimental_stack_elision`) that can improve the native calling convention
performance by bypassing stack pushes entirely when possible.

As of writing, `asIScriptGeneric` is not a particularly efficient interface (see
below), but when the time comes, we may try to contribute back design
improvements for it.

<details>
<summary>Elaborating on the generic calling convention</summary>

`asIScriptGeneric` is a rather slow interface by design. You might think that
the native calling convention (e.g. `asCALL_CDECL` and all) might be faster, but
it's not actually obvious why that would be true. In stock AngelScript (i.e. no
JIT), the native convention is actually fairly slow as it needs to go through
native shims that are fairly complex due to needing to handle arbitrary
signatures, resulting in a rather impressive pile of C++ ABI emulation for each
unique platform.

AngelScript tries to be clever about this in some cases (e.g. `asBC_Thiscall1`),
but native calls otherwise carry more overhead than you might think.

In essence, the native calling convention actually fundamentally has to do
_more_ work than the generic calling convention! In both cases, all arguments
live on the AS stack either way, and have to be pulled out of it sooner or
later. Whether AS itself or your app need to look up those arguments doesn't
really matter.

That being said, the generic calling convention *is* actually somewhat slow
out-of-the-box in AngelScript, but this is not an unsolvable problem.  
The callee (your code) needs to do, for _every_ argument or other call to the
generic, a virtual function call. This is usually not outrageously expensive,
but it prevents inlining despite the functions being (overall) not very
expensive each.  
Worse, some functions also do a lot of work and e.g. need to look up the
script function type information and then loop over arguments,
_for every argument lookup you do_.

When automatically wrapping functions for the generic calling convention, it
actually is feasible to hack a lot of that complexity away by directly poking at
`asCGeneric`.

Angelsea is able to make generic calls much faster by doing a lot less work than
the AngelScript VM, though, largely thanks to the fact we can generate code
tailored for each function, skipping steps and branches we know are
unnecessary. It also has some hacks like pooling the `asCGeneric` objects at
function level to skip reinitialization of fields that never change.  
This allows angelsea to give a ~5x performance uplift for a 1 million generic
calls benchmark.

</details>

## Why generate C instead of MIR?

It actually makes a lot of sense to take this "lazy" approach.

1. Our bytecode2c compiler generates standard (enough) C code. Entry points use
the `asJITFunction` signature. We also try to resolve all references to pointers
baked in the bytecode and forward detailed information via a callback.  
Fundamentally, nothing about the codegen is really specific to JIT or even to
MIR...
2. ... So nothing really prevents you from AOT compiling AngelScript code to C
using bytecode2c for release builds, which might be interesting for consoles and
certain platforms (e.g. iOS) which notoriously ban JITs. You still would need
the interpreter (if only because Angelsea will fallback to it), but in theory,
all you would need to do is use bytecode2c with appropriate symbol callbacks,
and add some glue code by implementing your own `asIJITCompiler` that map JIT
entry points to C++. (This isn't really an easy task yet, and there are _still_
some prerequisite refactors before this is viable, but the overall design
fundamentally allows it.)
3. In theory, it enables the ability to inject native C code. Because MIR is
capable of inlining functions, they could be made to implement
performance-sensitive things like some array calls and avoid a native function
call. (This would still be feasible even if we generated MIR directly, but it is
an advantage of using C.)
4. Generated C code is a lot like the VM code. This is fairly quick to do and is
surprisingly human-readable even to people with no prior compilation experience.
    - We do take more care with strict aliasing rules than AS does, though.
5. The fact we can just speak C greatly simplifies interfacing with script
engine structures. Dealing with the C++ ABI (such as for certain native function
calls, or to call virtual AS engine functions) would be annoying, but we can
work around it to some extent.
6. In theory, it does mean we can leverage native tooling such as
AddressSanitizer and static analysis to debug JIT bugs.

## What about other JIT compilers?

- [BlindMindStudio's JIT compiler](https://github.com/BlindMindStudios/AngelScript-JIT-Compiler)
served its purpose, but it is unmaintained (although people have forked it), has
way suboptimal and x86-only code generation, falls back to the interpreter
often (in our usecase), and contains subtle bugs.
- I previously worked on [asllvm](https://github.com/asumagic/asllvm).
It was a functional proof-of-concept (in fact some of angelsea's infrastructure
originates from it), but it was way too large in scope. It more or less intended
to take over the entire interpreter, which meant total coverage of *all* of AS'
low-level semantics before it was any useful. This is doubly a problem, because
to speak the C ABI -- let alone the C++ ABI -- you basically need to do it
yourself AFAIK. Besides, LLVM is _huge_, breaks API compatibility on almost
every major update, and is rather unreasonable for an embeddable language.
- Hazelight's UnrealEngine-AngelScript is way more involved than I thought and
includes a [C++ AOT transpiler](https://github.com/Hazelight/UnrealEngine-Angelscript/tree/e1bdb40e97da880ae907030dda65639d5a4b7b3d/Engine/Plugins/Angelscript/Source/AngelscriptCode/Private/StaticJIT).
To my understanding it is tightly coupled to that project and its fork of AS,
and does not feature an actual JIT compiler.
- There is an [AOT compiler](https://github.com/quarnster/asaot), but it is
unmaintained and I don't know what it is worth nowadays. Due to its approach, if
AOT is fine for you, it might actually make sense to use.

To my knowledge, there is no other (public...) project of that kind.

## Why this name?

I was looking for puns with "mir" or "mimir" and miserably failed, so all you
get is Angel→C, which seems memorable enough.

## JIT interface V1

Releases before 2.37 call `asIJITCompiler::CompileFunction(function, &output)` when a function is finished, where
2.37+ calls `asIJITCompilerV2::NewFunction`. The adapter registers the function exactly as `NewFunction` does. The
counting entry point is installed through `MirJit::set_jit_function`, which writes `scriptData->jitFunction` directly
on V1 releases (the engine reads that field in `asBC_JitEntry`; `SetJITFunction` only exists from 2.37).
`ReleaseJITFunction` does nothing: the code lives in MIR, and a V1 engine does not say which function it releases.
Everything else that differs between releases is behind macros and helpers in `include/angelsea/detail/compat.hpp`.
The object variables that start out null come from `objVariablePos` before 2.36, because the variable table has
neither temporaries nor the `onHeap` flag there.

## AOT (ahead of time)

`Jit::ExportModuleC(module, base)` runs the translator over a whole module (one translation unit, one set of runtime
declarations) and writes `<base>.c` and `<base>.externs`. You compile the C with any compiler that accepts GNU
extensions (clang or gcc, `-shared -O2 -DASEA_SUPPORT=1`, plus `-DASEA_ABI_MSVC=1` on Windows), which gives a library,
and `Jit::LoadAot(module, library, manifest)` installs its entry points in a module built by the same script.

How the generated code stays free of run-specific addresses: the MIR path hands every address (script functions, native
functions, type infos, global variables, string constants, the callee bytecode, the runtime helpers) to the linker by
name. `aot_redirect_externs` (`src/angelsea/detail/aot.cpp`) rewrites each declaration of such a symbol into a macro that
reads `asea_aot_ext[i]` with the declared type, and the library exports that table, the symbol names and the entry points
(`asea_aot_ext`, `asea_aot_ext_names`, `asea_aot_functions`). The manifest gives every extern a kind and a key that is
stable between runs: the declaration for functions and bytecode, `namespace::name` for globals, the hex of the characters
for string constants, the type declaration for types.

`LoadAot` resolves every symbol before it touches anything (a missing one rejects the whole library), checks each function
against a hash of its instruction stream (a function changed since the export is skipped and keeps running in the VM or
MIR), writes the `JitEntry` numbers the translator chose, drops the function from the lazy list and installs the entry
point. The library is rejected when `ANGELSCRIPT_VERSION` differs, because the generated code uses the engine's
structure offsets directly (`emit_hardcoded_vm_offsets`), so a library is also tied to the build of the engine and the
compiler ABI it was made for.

Limits: only script functions and methods of the module are exported. The export was built with clang (Windows x64)
and gcc 13 (Linux x64) and not tried with MSVC or on arm64 and macOS. `tests/aot/run.sh` exports, builds, loads and
compares every script of the corpus against the VM running only AOT code: every runnable script matches on both
platforms (29 on Linux).

### 32-bit targets (no MIR)

MIR has no 32-bit backend, so when `CMAKE_SIZEOF_VOID_P` is 4 (or with `-DASEA_NO_MIR=ON`) the build defines
`ASEA_NO_MIR`: `mirjit.cpp` keeps the bytecode to C translation, `ExportModuleC` and `LoadAot` and drops everything that
touches MIR; `register_function` does nothing, so functions stay in the interpreter until `LoadAot` installs library
code. The translator is the same on every target; only the step that turns the C into machine code changes.

What the translator assumes about the target, and where it is handled:

- Pointer-sized operands use `AS_PTR_SIZE` and `sizeof(asPWORD)` (`var_types::pword` and `void_ptr` were 8 bytes); an
  immediate of `asPWORD` is an `Immediate` of the integer of the same width, because `asPWORD` is its own type on 32-bit.
- The ABI ladder in the generated C selects the application-call code with the compiler's own macros, and falls back to
  the generic ABI (`this` is the first stack argument, which is the Itanium i386 convention, so gcc and clang work).
  MSVC x86 has `AbiMask::WINDOWS_MSVC_X86`: C has no `__thiscall`, but `thiscall` is ECX for `this` and the rest on the
  stack, which `__fastcall(this, dummy, args...)` reproduces exactly. Virtual `thiscall` and by-value object returns use
  the runtime helper on those targets.
- `tests/aot/msvc_shim.h` picks the width of `size_t`/`uintptr_t` from `_WIN64`; `tests/aot/run.sh` passes
  `/DASEA_ABI_MSVC=1` only for x64 targets (`VSCMD_ARG_TGT_ARCH`).

A runtime compiler for 32-bit (TinyCC in-process) was measured and rejected, see `docs/BENCHMARKS.md`.

## Register locals

When `JitConfig::experimental_register_locals` is enabled (the default), a
per-function pre-pass promotes eligible AngelScript frame slots (`fp - offset`)
to function-scoped scalar C locals (`asea_v_N`, or `asea_v_mN` for negative
parameter offsets). Scalar C locals are used rather than `union asea_var`
locals because `c2mir` places aggregates on the C stack and MIR memory GVN is
disabled below optimization level 3; type-punned accesses to the same slot
(such as `SetV8` initializing a `double` slot or mixed-type temporary reuse)
convert via `asea_i2f_inst` / `asea_i2f64_inst` as needed.

### Eligibility rules

An offset is promoted only if all of the following hold:
- Every access in the bytecode uses a constant frame offset with the same size
  class: 4-byte (`u32`, `s32`, `f32`, `u8`, `s8`, `u16`, `s16`) or 8-byte
  (`u64`, `s64`, `f64`, and `pword` on 64-bit targets).
- The offset is never reached through `frame_ptr(...)` (`PSF`, `LDV`,
  `LOADOBJ`, `STOREOBJ`, `LoadVObjR`, `RDR1`, `RDR2`, `NOT`, `iTOb`, `iTOw`),
  taken by address (`FREE`, `RefCpyV`), pushed indirectly (`VAR` for
  `GETOBJ`/`GETOBJREF`/`GETREF`), or referenced by an unsupported fallback
  instruction.
- It does not overlap any other accessed or disqualified slot (an 8-byte slot at
  `N` occupies dword slots `N` and `N - 1` in `fp - offset` coordinates).
- It is not covered by an object, handle, funcdef, or `onHeap` variable in
  `asCScriptFunction::scriptData->variables` (which the VM cleans up during
  exception unwinding), nor the hidden `DoesReturnOnStack()` return-pointer
  slot.
- Parameters (`offset <= 0`, including `this` at `0`) follow the same rules and
  are loaded from `fp` at entry.

### Spill and reload rules

- **Prologue / late JIT entries**: all promoted locals are loaded from `fp` in
  the function prologue before `entryLabel` dispatch, covering both initial
  entry and VM-to-JIT re-entry.
- **Calls and runtime helpers**: a callee never reads or writes the caller's
  promoted slots (they are not address-taken), so nothing is reloaded after a
  call and the locals stay in registers while a callee runs. Written promoted
  locals are spilled to `fp` only where the frame is handed back to the VM:
  before the `return;` that follows a script call whose callee did not return
  normally, before the fallback call path, and before `asBC_Cast`.
  Direct application calls spill only with `experimental_spill_on_native_calls`
  (off by default): it keeps `GetVar` and `GetAddressOfVar` exact inside the
  callee at about 10% on loops that call `array::opIndex`.
- **Functions that are not promoted**: any function with `CALLINTF`, `CALLBND`
  or `CallPtr` keeps its variables in the frame, because each of those calls
  leaves compiled code and re-enters through a `JitEntry` that would reload
  every promoted local.
- **VM fallbacks and exceptions**: `emit_error_handlers` spills all written
  promoted locals before `asea_set_internal_exception` in `err_*` handlers and
  at `vm:` before returning to the VM (including `SUSPEND` when
  `hack_ignore_suspend == false`). Error handlers and `emit_leave_system_call`
  branch to `vm_after_spill:` so an exception after a system call cannot
  overwrite frame edits made via `GetAddressOfVar`.
- **Function return (`asBC_RET`)**: when `experimental_fast_script_return` is
  false, `RET` falls back to `vm:` and spills; when true, the frame is popped
  and dead locals are not spilled.
