// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <angelscript.h>

// Releases before 2.37 only have the version 1 JIT interface (asIJITCompiler): the engine calls CompileFunction when a
// function is finished and has no asIScriptFunction::SetJITFunction. angelsea implements both; everything that differs
// between the two goes through this header.
#if ANGELSCRIPT_VERSION >= 23700
#define ASEA_JIT_INTERFACE_V2 1
#else
#define ASEA_JIT_INTERFACE_V2 0
#endif

// asIScriptFunction::GetDeclaredAt was added in 2.37.
#if ANGELSCRIPT_VERSION >= 23700
#define ASEA_HAS_GET_DECLARED_AT 1
#else
#define ASEA_HAS_GET_DECLARED_AT 0
#endif

#if ANGELSCRIPT_VERSION < 23600
// Before 2.36 angelscript.h has no asINT32 and CALLSTACK_FRAME_SIZE lives in as_context.cpp (same value, 9 dwords).
typedef signed int asINT32;
static constexpr int CALLSTACK_FRAME_SIZE = 9;
static constexpr int RESERVE_STACK        = 2 * AS_PTR_SIZE;
#endif

// asSScriptVariable::onHeap appeared in 2.36; before it every object variable held a pointer to a heap object.
#if ANGELSCRIPT_VERSION >= 23600
#define ASEA_VAR_ON_HEAP(var) ((var)->onHeap)
#else
#define ASEA_VAR_ON_HEAP(var) ((var)->type.IsObject() || (var)->type.IsFuncdef())
#endif

// Engine properties and system function flags added in 2.33; before it there is no call stack limit, the generic call
// mode does not exist and there are no composite properties.
#if ANGELSCRIPT_VERSION >= 23300
#define ASEA_MAX_CALLSTACK_SIZE(engine) ((engine).ep.maxCallStackSize)
#define ASEA_GENERIC_CALL_MODE(engine)  ((engine).ep.genericCallMode)
#define ASEA_IS_COMPOSITE_INDIRECT(sys_fn) ((sys_fn).isCompositeIndirect)
#else
#define ASEA_MAX_CALLSTACK_SIZE(engine) (0)
#define ASEA_GENERIC_CALL_MODE(engine)  (0)
#define ASEA_IS_COMPOSITE_INDIRECT(sys_fn) (false)
#endif

// asCModule renamed its global property table (scriptGlobals) to m_scriptGlobals in 2.35.
#if ANGELSCRIPT_VERSION >= 23500
#define ASEA_MODULE_GLOBALS(module) ((module).m_scriptGlobals)
#else
#define ASEA_MODULE_GLOBALS(module) ((module).scriptGlobals)
#endif

namespace angelsea::detail {

/// Section name, row and column where \ref fn was declared. Releases without GetDeclaredAt only know the section.
inline void get_declared_at(const asIScriptFunction& fn, const char** section, int* row, int* col) {
#if ASEA_HAS_GET_DECLARED_AT
	fn.GetDeclaredAt(section, row, col);
#else
	*section = fn.GetScriptSectionName();
	*row     = 0;
	*col     = 0;
#endif
}

} // namespace angelsea::detail
