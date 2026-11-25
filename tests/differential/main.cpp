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

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
};

struct Options {
	std::string               entry = "void main()";
	std::vector<asEBCInstr>   blacklist;
	asEBCInstr                fallback_after = asEBCInstr(-1);
	bool                      verbose        = false;
	bool                      all            = false; // run every global `void f()` instead of one entry point
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
	config.triggers.eager                 = true;
	config.experimental_stack_elision     = std::getenv("ASEA_NO_ELISION") == nullptr;
	config.log_targets.performance_hint   = asMSGTYPE_INFORMATION;
	config.log_targets.verbose            = asEMsgType(-1);
	config.c.human_readable               = std::getenv("ASEA_HUMAN") != nullptr;
	config.hack_ignore_exceptions         = std::getenv("ASEA_STRICT_EXC") == nullptr;
	config.debug.dump_c_code              = std::getenv("ASEA_DUMP_C") != nullptr;
	config.debug.blacklist_instructions   = options.blacklist;
	config.debug.fallback_after_instruction = options.fallback_after;

	// The JIT has to outlive the engine's use of it, so it is created before the engine releases it.
	angelsea::Jit* jit = nullptr;
	if (with_jit) {
		engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true);
		engine->SetEngineProperty(asEP_JIT_INTERFACE_VERSION, 2);
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
				const int   state = context->Execute();
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

int main(int argc, char** argv) {
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
		} else if (arg == "--all") {
			options.all = true;
		} else if (arg == "--verbose") {
			options.verbose = true;
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
		if (!same) {
			++mismatches;
			std::cout << "  vm : state=" << vm.state << " error=[" << vm.error << "] out=" << vm.out.size() << " bytes\n"
			          << "  jit: state=" << jit.state << " error=[" << jit.error << "] out=" << jit.out.size() << " bytes\n";
		}
	}
	std::cout << (mismatches == 0 ? "all scripts match" : "mismatches: " + std::to_string(mismatches)) << '\n';
	return mismatches == 0 ? 0 : 1;
}
