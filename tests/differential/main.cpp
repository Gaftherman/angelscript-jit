// SPDX-License-Identifier: BSD-2-Clause

// Differential test host: runs each script once on the plain bytecode VM and once with angelsea, and fails if the
// printed output, the final context state or the exception differ. Unlike the Catch2 suite it does not know what a
// script should print, so it works on any corpus (including generated scripts).

#include <angelscript.h>
#include <angelsea/config.hpp>
#include <angelsea/jit.hpp>

#include <scriptarray/scriptarray.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scriptstdstring/scriptstdstring.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::string g_out;
std::size_t g_hints = 0;

void print_string(const std::string& value) { g_out += value + '\n'; }
void print_int(std::int64_t value) { g_out += std::to_string(value) + '\n'; }
void print_uint(std::uint64_t value) { g_out += std::to_string(value) + '\n'; }
void print_double(double value) {
	char buffer[64];
	std::snprintf(buffer, sizeof buffer, "%.9g", value);
	g_out += std::string(buffer) + '\n';
}
void put_char(char value) { g_out += value; }

void on_message(const asSMessageInfo* info, void* param) {
	if (info->type == asMSGTYPE_INFORMATION) {
		// Messages from the JIT about code that stays on the VM are reported at this level.
		++g_hints;
		return;
	}
	if (param != nullptr) {
		std::cerr << info->section << ':' << info->row << ':' << info->col << ": " << info->message << '\n';
	}
}

struct Result {
	std::string out;
	std::string error;
	int         state = -1;
	std::size_t hints = 0;
	double      milliseconds = 0; // time spent running the entry points, not building or compiling
};

struct Options {
	std::string               entry = "void main()";
	std::vector<asEBCInstr>   blacklist;
	asEBCInstr                fallback_after = asEBCInstr(-1);
	bool                      verbose        = false;
	bool                      all            = false; // run every global `void f()` instead of one entry point
	int                       time_runs      = 0;     // > 0: also report the best of that many timed runs
	int                       only           = 0; // 1: only the VM, 2: only the JIT (for narrowing a crash down)
};

bool register_host(asIScriptEngine* engine) {
	RegisterStdString(engine);
	RegisterScriptArray(engine, true);
	return engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(print_string), asCALL_CDECL) >= 0
	    && engine->RegisterGlobalFunction("void print(int64)", asFUNCTION(print_int), asCALL_CDECL) >= 0
	    && engine->RegisterGlobalFunction("void print(uint64)", asFUNCTION(print_uint), asCALL_CDECL) >= 0
	    && engine->RegisterGlobalFunction("void print(double)", asFUNCTION(print_double), asCALL_CDECL) >= 0
	    && engine->RegisterGlobalFunction("void putchar(uint8)", asFUNCTION(put_char), asCALL_CDECL) >= 0;
}

Result run_once(const std::string& path, const Options& options, bool with_jit) {
	Result result;
	g_out.clear();
	g_hints = 0;

	asIScriptEngine* engine = asCreateScriptEngine();
	engine->SetMessageCallback(asFUNCTION(on_message), options.verbose ? &g_hints : nullptr, asCALL_CDECL);
	engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

	angelsea::JitConfig config{};
	config.triggers.eager                 = std::getenv("ASEA_HITS") == nullptr;
	if (const char* hits = std::getenv("ASEA_HITS")) config.triggers.hits_before_func_compile = std::strtoull(hits, nullptr, 10);
	config.experimental_stack_elision     = std::getenv("ASEA_NO_ELISION") == nullptr;
	config.log_targets.performance_hint   = asMSGTYPE_INFORMATION;
	config.log_targets.verbose            = asEMsgType(-1);
	config.c.human_readable               = std::getenv("ASEA_HUMAN") != nullptr;
	config.hack_ignore_exceptions         = std::getenv("ASEA_STRICT_EXC") == nullptr;
	config.experimental_direct_continue   = std::getenv("ASEA_NO_CONTINUE") == nullptr;
	config.experimental_register_locals   = std::getenv("ASEA_NO_REGLOCALS") == nullptr;
	if (const char* lvl = std::getenv("ASEA_MIR_LEVEL")) config.mir_optimization_level = std::atoi(lvl);
	if (const char* lvl = std::getenv("ASEA_MIR_DEBUG")) config.debug.mir_debug_level = std::atoi(lvl);
	config.debug.dump_mir_code = std::getenv("ASEA_DUMP_MIR") != nullptr;
	config.debug.dump_c_code              = std::getenv("ASEA_DUMP_C") != nullptr;
	config.debug.blacklist_instructions   = options.blacklist;
	config.debug.fallback_after_instruction = options.fallback_after;

	// The JIT has to outlive the engine's use of it, so it is created before the engine releases it.
	angelsea::Jit* jit = nullptr;
	if (with_jit) {
		engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true);
#if ANGELSCRIPT_VERSION >= 23700
		engine->SetEngineProperty(asEP_JIT_INTERFACE_VERSION, 2);
#endif
		jit = new angelsea::Jit(config, *engine);
		engine->SetJITCompiler(jit);
	}

	if (!register_host(engine)) {
		result.error = "host registration failed";
	} else {
		CScriptBuilder builder;
		if (builder.StartNewModule(engine, "m") < 0 || builder.AddSectionFromFile(path.c_str()) < 0
		    || builder.BuildModule() < 0) {
			result.error = "build failed";
		} else {
			asIScriptModule*                module = engine->GetModule("m");
			if (jit != nullptr) {
				// ASEA_AOT=<base>: load <base>.dll/.so and <base>.externs built from a previous ASEA_EXPORT_C=<base> run
				if (const char* aot = std::getenv("ASEA_AOT")) {
#if defined(_WIN32)
					const std::string library = std::string{aot} + ".dll";
#else
					const std::string library = std::string{aot} + ".so";
#endif
					const std::size_t loaded = jit->LoadAot(*module, library.c_str(), (std::string{aot} + ".externs").c_str());
					std::fprintf(stderr, "LoadAot: %zu functions\n", loaded);
				}
				if (const char* export_path = std::getenv("ASEA_EXPORT_C")) {
					std::fprintf(stderr, "ExportModuleC: %s\n", jit->ExportModuleC(*module, export_path) ? "ok" : "failed");
				}
			}
			std::vector<asIScriptFunction*> entries;
			if (options.all) {
				// Scripts of the Catch2 suite hold several test functions instead of one `main`: run every global
				// `void f()`, in declaration order.
				for (asUINT i = 0; i < module->GetFunctionCount(); ++i) {
					asIScriptFunction* candidate = module->GetFunctionByIndex(i);
					if (candidate->GetParamCount() == 0 && candidate->GetReturnTypeId() == asTYPEID_VOID) {
						entries.push_back(candidate);
					}
				}
			} else if (asIScriptFunction* function = module->GetFunctionByDecl(options.entry.c_str())) {
				entries.push_back(function);
			}

			if (entries.empty()) {
				result.error = "entry point not found";
			}
			for (asIScriptFunction* function : entries) {
				asIScriptContext* context = engine->CreateContext();
				context->Prepare(function);
				const auto  started = std::chrono::steady_clock::now();
				const int   state   = context->Execute();
				result.milliseconds
				    += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
				std::string note;
				if (state == asEXECUTION_EXCEPTION) {
					const char* text = context->GetExceptionString();
					note = std::string(text ? text : "") + " @" + std::to_string(context->GetExceptionLineNumber());
				}
				context->Release();
				if (options.all) {
					// Everything goes into the output, so a difference names the function it happened in.
					g_out += std::string("## ") + function->GetName() + " state=" + std::to_string(state) + " " + note + "\n";
				} else {
					result.state = state;
					result.error = note;
				}
			}
			if (options.all && !entries.empty()) {
				result.state = 0;
			}
		}
	}

	result.out   = g_out;
	result.hints = g_hints;
	engine->ShutDownAndRelease();
	delete jit;
	return result;
}

std::vector<asEBCInstr> parse_list(std::string_view text) {
	std::vector<asEBCInstr> values;
	while (!text.empty()) {
		const auto comma = text.find(',');
		const auto part  = text.substr(0, comma);
		int        value = 0;
		std::from_chars(part.data(), part.data() + part.size(), value);
		values.push_back(asEBCInstr(value));
		text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
	}
	return values;
}
} // namespace

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// ASEA_CRASH_INFO=1: print where an access violation happened (module and offset), to map it with a symbolizer
static LONG WINAPI crash_filter(EXCEPTION_POINTERS* info) {
	const void* address = info->ExceptionRecord->ExceptionAddress;
	HMODULE     module  = nullptr;
	char        name[260] = "?";
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(address), &module)) {
		GetModuleFileNameA(module, name, sizeof(name));
	}
	std::fprintf(stderr, "CRASH code=%lx at %p module=%s offset=%llx\n", info->ExceptionRecord->ExceptionCode, address, name,
	             static_cast<unsigned long long>(static_cast<const char*>(address) - reinterpret_cast<const char*>(module)));
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char** argv) {
#if defined(_WIN32)
	if (std::getenv("ASEA_CRASH_INFO") != nullptr) {
		SetUnhandledExceptionFilter(crash_filter);
	}
#endif
	Options                  options;
	std::vector<std::string> scripts;
	for (int i = 1; i < argc; ++i) {
		const std::string_view arg = argv[i];
		if (arg == "--entry" && i + 1 < argc) {
			options.entry = argv[++i];
		} else if (arg == "--blacklist" && i + 1 < argc) {
			options.blacklist = parse_list(argv[++i]);
		} else if (arg == "--fallback-after" && i + 1 < argc) {
			options.fallback_after = asEBCInstr(std::atoi(argv[++i]));
		} else if (arg == "--only" && i + 1 < argc) {
			options.only = std::string_view(argv[++i]) == "vm" ? 1 : 2;
		} else if (arg == "--time" && i + 1 < argc) {
			options.time_runs = std::atoi(argv[++i]);
		} else if (arg == "--all") {
			options.all = true;
		} else if (arg == "--verbose") {
			options.verbose = true;
		} else if (std::filesystem::is_directory(std::filesystem::path(std::string(arg)))) {
			// A folder stands for every script in it, in name order, so no shell has to expand a pattern.
			std::vector<std::string> found;
			for (const auto& entry : std::filesystem::directory_iterator(std::string(arg))) {
				if (entry.path().extension() == ".as") {
					found.push_back(entry.path().string());
				}
			}
			std::sort(found.begin(), found.end());
			scripts.insert(scripts.end(), found.begin(), found.end());
		} else {
			scripts.emplace_back(arg);
		}
	}
	if (scripts.empty()) {
		std::cerr << "usage: angelsea-differential [--entry decl] [--blacklist n,n] [--fallback-after n] script.as...\n";
		return 2;
	}

	int mismatches = 0;
	for (const auto& script : scripts) {
		const Result vm  = run_once(script, options, options.only == 2);
		const Result jit = options.only == 0 ? run_once(script, options, true) : vm;
		const bool   valid = vm.error != "build failed" && vm.error != "entry point not found";
		const bool   same = valid && vm.out == jit.out && vm.state == jit.state && vm.error == jit.error;
		std::cout << (same ? "ok       " : valid ? "MISMATCH " : "INVALID  ") << script << "  (jit notes: " << jit.hints
		          << ")\n";
		if (same && options.time_runs > 0 && options.only == 0) {
			// Best of N, so a noisy machine does not decide the result.
			double vm_best = vm.milliseconds, jit_best = jit.milliseconds;
			for (int run = 1; run < options.time_runs; ++run) {
				vm_best  = std::min(vm_best, run_once(script, options, false).milliseconds);
				jit_best = std::min(jit_best, run_once(script, options, true).milliseconds);
			}
			std::printf("  time     vm %9.2f ms   jit %9.2f ms   x%.2f\n", vm_best, jit_best, vm_best / jit_best);
		}
		if (!same) {
			++mismatches;
			std::cout << "  vm : state=" << vm.state << " error=[" << vm.error << "] out=" << vm.out.size() << " bytes\n"
			          << "  jit: state=" << jit.state << " error=[" << jit.error << "] out=" << jit.out.size() << " bytes\n";
			if (std::getenv("ASEA_SHOW_OUT") != nullptr) {
				std::cout << "  vm out:  " << vm.out << "\n  jit out: " << jit.out << "\n";
			}
		}
	}
	std::cout << (mismatches == 0 ? "all scripts match" : "mismatches: " + std::to_string(mismatches)) << '\n';
	return mismatches == 0 ? 0 : 1;
}
