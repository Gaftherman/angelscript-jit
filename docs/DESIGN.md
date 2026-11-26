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
