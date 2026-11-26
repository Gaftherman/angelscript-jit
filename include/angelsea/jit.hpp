// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <angelscript.h>
#include <angelsea/config.hpp>
#include <angelsea/detail/compat.hpp>
#include <angelsea/fnconfig.hpp>
#include <functional>
#include <memory>

namespace angelsea {

namespace detail {
class MirJit;
}

#if ASEA_JIT_INTERFACE_V2
class Jit final : public asIJITCompilerV2 {
#else
class Jit final : public asIJITCompiler {
#endif
	public:
	Jit(const JitConfig& config, asIScriptEngine& engine);

	Jit(const Jit&)            = delete;
	Jit& operator=(const Jit&) = delete;

	Jit(Jit&&)            = default;
	Jit& operator=(Jit&&) = default;

	~Jit();

#if ASEA_JIT_INTERFACE_V2
	void NewFunction(asIScriptFunction* scriptFunc) override;
	void CleanFunction(asIScriptFunction* scriptFunc, asJITFunction jitFunc) override;
#else
	/// Interface version 1 (releases before 2.37): the engine calls this when a function is finished. The function is
	/// registered like NewFunction does and compiled lazily. Records of functions discarded with their module stay until
	/// the engine is destroyed, because version 1 does not tell which function a released JIT function belonged to.
	int  CompileFunction(asIScriptFunction* scriptFunc, asJITFunction* output) override;
	void ReleaseJITFunction(asJITFunction func) override;
#endif

	using CompileFunc = void(void* ud);

	/// Configure a compilation callback for asynchronous/threaded compilation.
	/// Your callback will be called with a callable function pointer, which you must invoke with the provided void*
	/// user data parameter, wherever you see fit. This can be in a thread pool or a dedicated thread, for instance.
	///
	/// Compile times are typically not very long, but long enough to be an issue for realtime applications.
	/// When lazy compilation triggers a function compile, a compile task will be spawned. Once the compile is finished,
	/// and the main thread running the script is hitting a JitEntry again, the script function will be patched to allow
	/// jumping into the JIT.
	///
	/// For the time being, compile tasks can lock mutexes for heavy tasks, and thus block the thread for a fairly long
	/// amount of time. Hence, it is discouraged to make compile jobs happen in the same background pool as other tasks.
	void SetCompileCallback(std::function<void(CompileFunc*, void*)> callback);

	/// Configure a function configuration callback. This allows you to adjust certain JIT tunables at a function level,
	/// and optionally bind those to script metadata (see \ref parse_function_metadata).
	///
	/// When `manual_discovery` is set, you **MUST** call `DiscoverFnConfig` after any module is built. This is done to
	/// accomodate the standard script builder module, which only populates metadata maps once the module was built.
	///
	/// For safety, you may want to set a null callback after calling \ref DiscoverFnConfig.
	void SetFnConfigRequestCallback(std::function<FnConfig(asIScriptFunction&)> callback, bool manual_discovery);

	/// See \ref SetFnConfigRequestCallback. For all pending functions, this will cause the provided function config
	/// callback to be called, and never again after.
	void DiscoverFnConfig();

	/// Ahead-of-time input: writes the C source of every function of \p module to `<path>.c` and the symbols it
	/// expects the loader to provide (`name<TAB>kind<TAB>key`) to `<path>.externs`. Compile the C with a gcc or clang
	/// that accepts GNU extensions. Returns false when nothing was written.
	bool ExportModuleC(asIScriptModule& module, const char* path);

	/// Ahead-of-time loader: installs the functions of a library built from the C written by \ref ExportModuleC.
	/// \p library is the shared library (`.dll`, `.so`, `.dylib`) and \p manifest the `<path>.externs` file. Call it after
	/// the module is built, before running it. Functions whose bytecode changed since the export are skipped, a library
	/// made for another AngelScript release is rejected. Returns how many functions were installed.
	std::size_t LoadAot(asIScriptModule& module, const char* library, const char* manifest);

	private:
	std::unique_ptr<detail::MirJit> m_compiler;
};

} // namespace angelsea