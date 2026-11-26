// SPDX-License-Identifier: BSD-2-Clause

#include <algorithm>
#include <angelscript.h>
#include <angelsea/detail/aot.hpp>
#include <angelsea/detail/bytecode2c.hpp>
#include <angelsea/detail/bytecodeinstruction.hpp>
#include <angelsea/detail/bytecodetools.hpp>
#include <angelsea/detail/debug.hpp>
#include <angelsea/detail/log.hpp>
#include <angelsea/detail/mirjit.hpp>
#include <angelsea/detail/runtime.hpp>
#include <as_generic.h>
#include <bit>
#include <cmath>
#include <fstream>
#if !ASEA_NO_MIR
#include <mir-gen.h>
#include <mir.h>
#endif
#include <string>
#include <unordered_map>

namespace angelsea::detail {

#if !ASEA_NO_MIR
Mir::Mir(MIR_alloc_t alloc, MIR_code_alloc_t code_alloc) : m_ctx(MIR_init2(alloc, code_alloc)) {}
Mir::~Mir() { MIR_finish(m_ctx); }
#endif

/// Installs callback as the function-level JIT entry point. Releases before 2.37 have no SetJITFunction; the engine
/// reads scriptData->jitFunction directly in asBC_JitEntry, so writing it is equivalent.
static int set_jit_function(asIScriptFunction& function, asJITFunction callback) {
#if ASEA_JIT_INTERFACE_V2
	return function.SetJITFunction(callback);
#else
	auto& script_function = static_cast<asCScriptFunction&>(function);
	if (script_function.scriptData == nullptr) {
		return asERROR;
	}
	script_function.scriptData->jitFunction = callback;
	return asSUCCESS;
#endif
}

#if !ASEA_NO_MIR
C2Mir::C2Mir(Mir& mir) : m_ctx(mir) { c2mir_init(m_ctx); }
C2Mir::~C2Mir() { c2mir_finish(m_ctx); }

static void bind_runtime(Mir& mir) {
#define ASEA_BIND_MIR(name) MIR_load_external(mir, #name, std::bit_cast<void*>(&(name)))
	ASEA_BIND_MIR(asea_call_script_function);
	ASEA_BIND_MIR(asea_call_system_function);
	ASEA_BIND_MIR(asea_call_object_method);
	ASEA_BIND_MIR(asea_prepare_script_stack);
	ASEA_BIND_MIR(asea_prepare_script_stack_and_vars);
	ASEA_BIND_MIR(asea_debug_message);
	ASEA_BIND_MIR(asea_debug_int);
	ASEA_BIND_MIR(asea_set_internal_exception);
	ASEA_BIND_MIR(asea_clean_args);
	ASEA_BIND_MIR(asea_cast);
	ASEA_BIND_MIR(asea_resolve_virtual_function);
	ASEA_BIND_MIR(asea_alloc);
	ASEA_BIND_MIR(asea_free);
	ASEA_BIND_MIR(asea_new_script_object);
	ASEA_BIND_MIR(asea_powi);
	ASEA_BIND_MIR(asea_powu);
	ASEA_BIND_MIR(asea_powi64);
	ASEA_BIND_MIR(asea_powu64);
	MIR_load_external(mir, "pow", std::bit_cast<void*>(+[](double a, double b) { return pow(a, b); }));
	MIR_load_external(mir, "powf", std::bit_cast<void*>(+[](float a, float b) { return powf(a, b); }));
	MIR_load_external(mir, "memcpy", std::bit_cast<void*>(+[](void* dst, const void* src, std::size_t size) {
		                  return std::memcpy(dst, src, size);
	                  }));
	ASEA_BIND_MIR(memset);
	MIR_load_external(mir, "fmod", std::bit_cast<void*>(+[](double a, double b) { return fmod(a, b); }));
	MIR_load_external(mir, "fmodf", std::bit_cast<void*>(+[](float a, float b) { return fmodf(a, b); }));
#undef ASEA_BIND_MIR
}
#endif // !ASEA_NO_MIR

void jit_entry_function_counter(asSVMRegisters* regs, asPWORD lazy_fn_raw) {
	if (lazy_fn_raw != 1) { // value 1 can be passed in direct JIT calls; ignore it
		auto& lazy_fn = *std::bit_cast<LazyMirFunction*>(lazy_fn_raw);

		if (lazy_fn.hits_before_compile == 0) {
			if (lazy_fn.jit_engine->translate_lazy_function(lazy_fn)) {
				// let jitentry rerun in case compilation updated the jit function
				return;
			}
		} else {
			--lazy_fn.hits_before_compile;
		}
	}

	regs->programPointer += 1 + AS_PTR_SIZE; // skip the jitentry
}

void jit_entry_await_async(asSVMRegisters* regs, asPWORD pending_fn_raw) {
	if (pending_fn_raw != 1) { // value 1 can be passed in direct JIT calls; ignore it
		auto& lazy_fn = *std::bit_cast<AsyncMirFunction*>(pending_fn_raw);

		if (lazy_fn.compiled.ready.load()) {
			lazy_fn.jit_engine->link_ready_functions();
			return; // let jitentry rerun
		}
	}

	regs->programPointer += 1 + AS_PTR_SIZE; // skip the jitentry
}

MirJit::MirJit(const JitConfig& config, asIScriptEngine& engine) :
    m_config(config),
    m_engine(&engine),
    m_c_generator{m_config, *m_engine},
    m_ignore_unregister{nullptr},
    m_registered_engine_globals{false} {
#if !ASEA_NO_MIR
	bind_runtime(m_mir);
#endif
}

MirJit::~MirJit() {
	std::unique_lock lk{m_termination_mutex};
	while (m_terminating_threads > 0) {
		m_termination_cv.wait(lk);
	}
}

void MirJit::register_function(asIScriptFunction& script_function) {
#if ASEA_NO_MIR
	// No runtime compiler on this target: functions stay in the VM until load_aot installs library code.
	(void)script_function;
	return;
#else
	if (!m_registered_engine_globals) {
		bind_engine_globals(*script_function.GetEngine());
		m_registered_engine_globals = true;
	}

	auto [lazy_mir_it, not_already_registered] = m_lazy_functions.emplace(
	    &script_function,
	    LazyMirFunction{
	        .jit_engine          = this,
	        .fn_config           = std::nullopt,
	        .script_function     = &script_function,
	        .hits_before_compile = config().triggers.hits_before_func_compile
	    }
	);

	if (!not_already_registered) {
		return;
	}

	LazyMirFunction* lazy_fn = &lazy_mir_it->second;

	if (!m_fn_config_manual_discovery && m_request_fn_config_callback) {
		lazy_fn->fn_config = m_request_fn_config_callback(*lazy_fn->script_function);
	}

	if (!m_fn_config_manual_discovery && m_config.triggers.eager) {
		// with manual discovery compilation will be triggered in discover_fn_config
		std::ignore = translate_lazy_function(*lazy_fn);
	} else {
		setup_jit_callback(script_function, jit_entry_function_counter, lazy_fn, false);
	}
#endif
}

void MirJit::unregister_function(asIScriptFunction& script_function) {
	if (&script_function == m_ignore_unregister) {
		return;
	}

	m_lazy_functions.erase(&script_function);

	auto async_it = m_async_codegen_functions.find(&script_function);
	if (async_it != m_async_codegen_functions.end()) {
		std::lock_guard lk{m_async_finalize_mutex};
		auto            async_fn = std::move(async_it->second);
		m_async_codegen_functions.erase(&script_function);

		// is the async callback still running?
		// (we're locking m_async_destruct_mutex, so this is safe)
		if (!async_fn->compiled.ready.load()) {
			m_async_cancelled_functions.emplace_back(std::move(async_fn));
		}
		// if not, let the async_fn die
	}

	{
		std::lock_guard lk{m_async_finalize_mutex};
		m_async_finished_functions.erase(&script_function);
	}

	// can't unload modules from MIR AFAIK
}

void MirJit::bind_engine_globals([[maybe_unused]] asIScriptEngine& engine) {
#if !ASEA_NO_MIR
	MIR_load_external(m_mir, "asea_engine", &engine);

	if (m_config.experimental_direct_generic_call) {
		// gross hack to initialize the vtable ptr of asCGeneric properly in JITted functions
		// unsure how to support that for AOT from the C side
		asCGeneric generic{nullptr, nullptr, nullptr, nullptr};
		void*      generic_vtable = *std::bit_cast<void**>(&generic);
		MIR_load_external(m_mir, "asea_generic_vtable", generic_vtable);
	}
#endif
}

struct InputData {
	TranspiledCode* code;
	std::size_t     current_block = 0;
	const char*     current_ptr;

	explicit InputData(TranspiledCode& code) : code{&code}, current_ptr{code.source_bits[0]} {}
};

static int c2mir_getc_callback(void* user_data) {
	InputData& info = *static_cast<InputData*>(user_data);
	while (*info.current_ptr == '\0') {
		if (info.current_block >= info.code->source_bits.size() - 1) {
			return EOF;
		}

		++info.current_block;
		info.current_ptr = info.code->source_bits[info.current_block];
	}

	std::int8_t c = *info.current_ptr;
	++info.current_ptr;
	return c;
}

/// Hash of the instruction stream (opcode and size of each instruction): operands hold run-specific pointers, so they stay
/// out of it.
static unsigned long long opcode_hash(asIScriptFunction& fn) {
	std::string stream;
	for (InsRef ins : get_bytecode(fn)) {
		stream += std::to_string(int(ins.opcode())) + ':' + std::to_string(ins.size()) + ';';
	}
	return aot_hash(stream);
}

static std::vector<std::string> split_tabs(const std::string& line) {
	std::vector<std::string> columns;
	std::size_t              begin = 0;
	for (;;) {
		const auto end = line.find('\t', begin);
		columns.push_back(line.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
		if (end == std::string::npos) {
			break;
		}
		begin = end + 1;
	}
	return columns;
}

std::size_t MirJit::load_aot(asIScriptModule& module, const char* library_path, const char* manifest_path) {
	void* library = aot_open_library(library_path);
	if (library == nullptr) {
		return 0;
	}
	const auto* version      = static_cast<const int*>(aot_symbol(library, "asea_aot_version"));
	void**      ext          = static_cast<void**>(aot_symbol(library, "asea_aot_ext"));
	const auto* ext_names    = static_cast<const char* const*>(aot_symbol(library, "asea_aot_ext_names"));
	const auto* ext_count    = static_cast<const unsigned*>(aot_symbol(library, "asea_aot_ext_count"));
	const auto* fn_table     = static_cast<const AotFunctionEntry*>(aot_symbol(library, "asea_aot_functions"));
	const auto* fn_count     = static_cast<const unsigned*>(aot_symbol(library, "asea_aot_function_count"));
	if (version == nullptr || ext == nullptr || ext_names == nullptr || ext_count == nullptr || fn_table == nullptr
	    || fn_count == nullptr || *version != ANGELSCRIPT_VERSION) {
		return 0;
	}

	struct ManifestExtern {
		std::string kind, key, id;
	};
	struct ManifestFunction {
		std::string decl, hash, entries;
	};
	std::unordered_map<std::string, ManifestExtern>   externs;
	std::unordered_map<std::string, ManifestFunction> manifest_functions;
	{
		std::ifstream in(manifest_path, std::ios::binary);
		if (!in) {
			return 0;
		}
		std::string line;
		while (std::getline(in, line)) {
			const auto columns = split_tabs(line);
			if (columns.size() < 3) {
				continue;
			}
			if (columns[1] == "function") {
				manifest_functions[columns[0]]
				    = {columns[2], columns.size() > 3 ? columns[3] : "", columns.size() > 4 ? columns[4] : ""};
			} else if (columns[1] != "version") {
				externs[columns[0]] = {columns[1], columns[2], columns.size() > 3 ? columns[3] : ""};
			}
		}
	}

	// every function and method of the module, by declaration
	std::unordered_map<std::string, asIScriptFunction*> module_functions;
	for (asUINT i = 0; i < module.GetFunctionCount(); ++i) {
		asIScriptFunction* f = module.GetFunctionByIndex(i);
		module_functions[f->GetDeclaration(true, true, true)] = f;
	}
	for (asUINT t = 0; t < module.GetObjectTypeCount(); ++t) {
		asITypeInfo* type = module.GetObjectTypeByIndex(t);
		for (asUINT m = 0; m < type->GetMethodCount(); ++m) {
			asIScriptFunction* f = type->GetMethodByIndex(m, false);
			module_functions[f->GetDeclaration(true, true, true)] = f;
		}
	}

	// every engine function by declaration, preferring the ones of this module
	auto*                                               engine = static_cast<asCScriptEngine*>(m_engine);
	std::unordered_map<std::string, asCScriptFunction*> engine_functions;
	for (asUINT i = 0; i < engine->scriptFunctions.GetLength(); ++i) {
		asCScriptFunction* f = engine->scriptFunctions[i];
		if (f == nullptr) {
			continue;
		}
		const std::string decl = f->GetDeclaration(true, true, true);
		if (f->GetModule() == &module || engine_functions.find(decl) == engine_functions.end()) {
			engine_functions[decl] = f;
		}
	}

	void* generic_vtable = nullptr;
	{
		asCGeneric generic{nullptr, nullptr, nullptr, nullptr};
		generic_vtable = *std::bit_cast<void**>(&generic);
	}
	const std::unordered_map<std::string, void*> builtins = {
	    {"asea_call_script_function", std::bit_cast<void*>(&asea_call_script_function)},
	    {"asea_call_system_function", std::bit_cast<void*>(&asea_call_system_function)},
	    {"asea_call_object_method", std::bit_cast<void*>(&asea_call_object_method)},
	    {"asea_prepare_script_stack", std::bit_cast<void*>(&asea_prepare_script_stack)},
	    {"asea_prepare_script_stack_and_vars", std::bit_cast<void*>(&asea_prepare_script_stack_and_vars)},
	    {"asea_debug_message", std::bit_cast<void*>(&asea_debug_message)},
	    {"asea_debug_int", std::bit_cast<void*>(&asea_debug_int)},
	    {"asea_set_internal_exception", std::bit_cast<void*>(&asea_set_internal_exception)},
	    {"asea_clean_args", std::bit_cast<void*>(&asea_clean_args)},
	    {"asea_cast", std::bit_cast<void*>(&asea_cast)},
	    {"asea_resolve_virtual_function", std::bit_cast<void*>(&asea_resolve_virtual_function)},
	    {"asea_alloc", std::bit_cast<void*>(&asea_alloc)},
	    {"asea_free", std::bit_cast<void*>(&asea_free)},
	    {"asea_new_script_object", std::bit_cast<void*>(&asea_new_script_object)},
	    {"asea_powi", std::bit_cast<void*>(&asea_powi)},
	    {"asea_powu", std::bit_cast<void*>(&asea_powu)},
	    {"asea_powi64", std::bit_cast<void*>(&asea_powi64)},
	    {"asea_powu64", std::bit_cast<void*>(&asea_powu64)},
	    {"asea_generic_vtable", generic_vtable},
	    {"asea_engine", m_engine},
	};

	// resolve into a scratch table so that a failure leaves the library untouched
	std::vector<void*> resolved(*ext_count, nullptr);
	for (unsigned i = 0; i < *ext_count; ++i) {
		const std::string name = ext_names[i];
		if (auto b = builtins.find(name); b != builtins.end()) {
			resolved[i] = b->second;
			continue;
		}
		const auto m = externs.find(name);
		if (m == externs.end()) {
			return 0;
		}
		const auto& [kind, key, export_id] = m->second;
		void* value             = nullptr;
		if (kind == "bytecode") {
			if (auto f = module_functions.find(key); f != module_functions.end()) {
				value = f->second->GetByteCode();
			} else if (auto e = engine_functions.find(key); e != engine_functions.end()) {
				value = e->second->GetByteCode(); // callee of another module or an application generated function
			}
		} else if (kind == "global") {
			for (asUINT g = 0; g < module.GetGlobalVarCount() && value == nullptr; ++g) {
				const char* g_name = nullptr;
				const char* g_ns   = nullptr;
				module.GetGlobalVar(g, &g_name, &g_ns);
				if (std::string{g_ns != nullptr ? g_ns : ""} + "::" + g_name == key) {
					// the address of the property's storage, as the engine's address map has it: for an object variable
					// that is the slot that holds the object pointer, which GetAddressOfGlobalVar would dereference
					value = ASEA_MODULE_GLOBALS(static_cast<asCModule&>(module)).Get(g)->GetAddressOfValue();
				}
			}
		} else if (kind == "string") {
#if ANGELSCRIPT_VERSION >= 23200 // asIStringFactory appeared in 2.32; before it string constants cannot be exported
			asIStringFactory* factory = engine->stringFactory;
			if (factory != nullptr) {
				std::string data;
				for (std::size_t c = 0; c + 1 < key.size(); c += 2) {
					data += char(std::stoi(key.substr(c, 2), nullptr, 16));
				}
				value = const_cast<void*>(factory->GetStringConstant(data.data(), asUINT(data.size())));
			}
#endif
		} else if (kind == "script_function" || kind == "system_function") {
			if (auto f = engine_functions.find(key); f != engine_functions.end()) {
				if (std::getenv("ASEA_AOT_DEBUG") != nullptr) {
					std::fprintf(stderr, "aot %s %s export_id=%s id=%d\n", name.c_str(), key.c_str(), export_id.c_str(), f->second->id);
				}
				value = kind == "script_function"
				            ? static_cast<void*>(f->second)
				            : (f->second->sysFuncIntf != nullptr ? std::bit_cast<void*>(f->second->sysFuncIntf->func)
				                                                 : nullptr);
			}
		} else if (kind == "type") {
			value = m_engine->GetTypeInfoByDecl(key.c_str());
		}
		if (value == nullptr) {
			return 0;
		}
		resolved[i] = value;
	}

	// plan the installs before touching anything
	struct Install {
		asIScriptFunction*       fn;
		void*                    entry;
		std::vector<asPWORD>     entries;
	};
	std::vector<Install> installs;
	for (unsigned i = 0; i < *fn_count; ++i) {
		const auto m = manifest_functions.find(fn_table[i].name);
		if (m == manifest_functions.end()) {
			continue;
		}
		const auto f = module_functions.find(m->second.decl);
		if (f == module_functions.end() || std::to_string(opcode_hash(*f->second)) != m->second.hash) {
			continue; // stale export: the function keeps running in the VM or in MIR
		}
		if (const char* only = std::getenv("ASEA_AOT_ONLY"); only != nullptr && m->second.decl.find(only) == std::string::npos) {
			continue; // diagnosis: install only the functions whose declaration contains this text
		}
		Install install{f->second, fn_table[i].fn, {}};
		for (std::size_t begin = 0; begin < m->second.entries.size();) {
			const auto end = m->second.entries.find(',', begin);
			install.entries.push_back(
			    std::stoull(m->second.entries.substr(begin, end == std::string::npos ? std::string::npos : end - begin))
			);
			begin = end == std::string::npos ? m->second.entries.size() : end + 1;
		}
		installs.push_back(std::move(install));
	}
	if (installs.empty()) {
		return 0;
	}

	for (unsigned i = 0; i < *ext_count; ++i) {
		ext[i] = resolved[i];
	}
	for (const auto& install : installs) {
		std::size_t k = 0;
		for (InsRef ins : get_bytecode(*install.fn)) {
			if (ins.opcode() == asBC_JitEntry && k < install.entries.size()) {
				ins.pword0() = install.entries[k++];
			}
		}
		m_lazy_functions.erase(install.fn);
		m_ignore_unregister = install.fn;
		set_jit_function(*install.fn, std::bit_cast<asJITFunction>(install.entry));
		m_ignore_unregister = nullptr;
	}
	return installs.size();
}

bool MirJit::export_module_c(asIScriptModule& module, std::string& c_source, std::string& manifest) {
	std::vector<asIScriptFunction*> functions;
	for (asUINT i = 0; i < module.GetFunctionCount(); ++i) {
		functions.push_back(module.GetFunctionByIndex(i));
	}
	for (asUINT t = 0; t < module.GetObjectTypeCount(); ++t) {
		asITypeInfo* type = module.GetObjectTypeByIndex(t);
		for (asUINT m = 0; m < type->GetMethodCount(); ++m) {
			functions.push_back(type->GetMethodByIndex(m, false));
		}
	}

	BytecodeToC                           generator{m_config, *m_engine, "asea_aot"};
	std::unordered_map<std::string, bool> seen_externs;
	std::string                           functions_manifest;
	std::string                           externs_manifest;
	std::size_t                           translated = 0;
	std::vector<AotFunction>              exported;

	generator.prepare_new_context();
	generator.set_map_extern_callback([&](const char* c_name, const BytecodeToC::ExternMapping& mapping, void*) {
		if (!seen_externs.emplace(c_name, true).second) {
			return;
		}
		const char* kind = "";
		std::string key;
		std::string id; // engine function id at export time, only to diagnose a wrong resolution
		std::visit(
		    [&](const auto& m) {
			    using T = std::decay_t<decltype(m)>;
			    if constexpr (std::is_same_v<T, BytecodeToC::ExternBytecodeDefinition>) {
				    kind = "bytecode";
				    key  = m.fn->GetDeclaration(true, true, true);
			    } else if constexpr (std::is_same_v<T, BytecodeToC::ExternGlobalVariable>) {
				    kind = "global";
				    if (m.property != nullptr) {
					    key = std::string{m.property->nameSpace != nullptr ? m.property->nameSpace->name.AddressOf() : ""}
					          + "::" + m.property->name.AddressOf();
				    }
			    } else if constexpr (std::is_same_v<T, BytecodeToC::ExternStringConstant>) {
				    kind = "string";
				    // hex of the characters, read back through the registered string factory
#if ANGELSCRIPT_VERSION >= 23200
				    asIStringFactory* factory = static_cast<asCScriptEngine*>(m_engine)->stringFactory;
				    if (factory != nullptr) {
					    asUINT length = 0;
					    factory->GetRawStringData(m.ptr, nullptr, &length);
					    std::string data(length, '\0');
					    factory->GetRawStringData(m.ptr, data.data(), &length);
					    static constexpr char digits[] = "0123456789abcdef";
					    for (const unsigned char c : data) {
						    key += digits[c >> 4];
						    key += digits[c & 15];
					    }
				    }
#else
				    std::ignore = m;
#endif
			    } else if constexpr (std::is_same_v<T, BytecodeToC::ExternScriptFunction>) {
				    kind = "script_function";
				    id   = std::to_string(m.id);
				    if (asIScriptFunction* f = m_engine->GetFunctionById(m.id)) {
					    key = f->GetDeclaration(true, true, true);
				    }
			    } else if constexpr (std::is_same_v<T, BytecodeToC::ExternSystemFunction>) {
				    kind = "system_function";
				    id   = std::to_string(m.id);
				    if (asIScriptFunction* f = m_engine->GetFunctionById(m.id)) {
					    key = f->GetDeclaration(true, true, true);
				    }
			    } else if constexpr (std::is_same_v<T, BytecodeToC::ExternTypeInfo>) {
				    kind = "type";
				    if (m.object_type != nullptr) {
					    key = m_engine->GetTypeDeclaration(m.object_type->GetTypeId(), true);
				    }
			    }
		    },
		    mapping
		);
		externs_manifest += std::string{c_name} + '\t' + kind + '\t' + key + '\t' + id + '\n';
	});

	for (asIScriptFunction* fn : functions) {
		if (fn == nullptr || fn->GetFuncType() != asFUNC_SCRIPT) {
			continue;
		}
		asUINT bytecode_length = 0;
		fn->GetByteCode(&bytecode_length);
		if (bytecode_length == 0 || bytecode_length * sizeof(asDWORD) > m_config.max_bytecode_bytes) {
			continue;
		}
		std::string c_name;
		generator.set_map_function_callback([&](asIScriptFunction&, const std::string& name) { c_name = name; });
		// translation renumbers the JitEntry arguments of the live bytecode: put them back so the function still runs
		std::vector<std::pair<asPWORD*, asPWORD>> jit_entry_args;
		for (InsRef ins : get_bytecode(*fn)) {
			if (ins.opcode() == asBC_JitEntry) {
				jit_entry_args.emplace_back(&ins.pword0(), ins.pword0());
			}
		}
		generator.translate_function(module.GetName(), *fn, FnConfig{});
		// the numbers the translator gave to the entries, which the loader writes back in the same order
		std::string entries;
		for (InsRef ins : get_bytecode(*fn)) {
			if (ins.opcode() == asBC_JitEntry) {
				entries += (entries.empty() ? "" : ",") + std::to_string(ins.pword0());
			}
		}
		for (const auto& [slot, value] : jit_entry_args) {
			*slot = value;
		}
		functions_manifest += c_name + "\tfunction\t" + fn->GetDeclaration(true, true, true) + '\t'
		                      + std::to_string(opcode_hash(*fn)) + '\t' + entries + '\n';
		exported.push_back(AotFunction{c_name});
		++translated;
	}

	if (translated == 0) {
		return false;
	}

	TranspiledCode code = generator.finalize_context();
	std::string    raw;
	for (const char* block : code.source_bits) {
		raw += block;
	}

	std::vector<std::string> table_symbols = {
	    "asea_call_script_function", "asea_call_system_function", "asea_call_object_method",
	    "asea_prepare_script_stack", "asea_prepare_script_stack_and_vars", "asea_debug_message", "asea_debug_int",
	    "asea_set_internal_exception", "asea_clean_args", "asea_cast", "asea_resolve_virtual_function", "asea_alloc",
	    "asea_free", "asea_new_script_object", "asea_powi", "asea_powu", "asea_powi64", "asea_powu64",
	    "asea_generic_vtable", "asea_engine",
	};
	for (const auto& [name, unused] : seen_externs) {
		table_symbols.push_back(name);
	}
	std::vector<std::string> used;
	c_source = aot_redirect_externs(raw, table_symbols, exported, ANGELSCRIPT_VERSION, used);

	manifest = "version\tversion\t" + std::to_string(ANGELSCRIPT_VERSION) + "\n" + functions_manifest + externs_manifest;
	return true;
}

bool MirJit::translate_lazy_function(LazyMirFunction& fn) {
	if (m_fn_config_manual_discovery && !fn.fn_config.has_value()) {
		return false;
	}

	// TODO: maybe split the disable_jit and bytecode size checks to a different function as early as when FnConfig is
	// known so that it can happen earlier than lazy function translation being triggered
	FnConfig fn_config = fn.fn_config.has_value() ? *fn.fn_config : FnConfig{};

	std::string c_name;
	m_c_generator.set_map_function_callback([&]([[maybe_unused]] asIScriptFunction& received_fn,
	                                            const std::string&                  name) {
		angelsea_assert(&received_fn == fn.script_function);
		c_name = name;
	});

	asIScriptModule* script_module = fn.script_function->GetModule();

	const char* name = script_module != nullptr ? script_module->GetName() : "<anon>";

	std::vector<std::pair<std::string, void*>> deferred_bindings;

	if (fn_config.disable_jit) {
		setup_jit_callback(*fn.script_function, nullptr, nullptr, true);
		m_lazy_functions.erase(fn.script_function);
		return false;
	}

	asUINT bytecode_length;
	fn.script_function->GetByteCode(&bytecode_length);
	if (bytecode_length * sizeof(asDWORD) > m_config.max_bytecode_bytes) {
		if (!fn_config.ignore_perf_warnings) {
			log(m_config,
			    *m_engine,
			    *fn.script_function,
			    LogSeverity::ASEA_WARNING,
			    "Function not considered for JIT compilation because it is too complex");
		}
		setup_jit_callback(*fn.script_function, nullptr, nullptr, true);
		m_lazy_functions.erase(fn.script_function);
		return false;
	}

	// TODO: b2c in thread as well
	m_c_generator.prepare_new_context();
	m_c_generator.set_map_extern_callback([&](const char*                                        c_name,
	                                          [[maybe_unused]] const BytecodeToC::ExternMapping& mapping,
	                                          void* raw_value) { deferred_bindings.emplace_back(c_name, raw_value); });
	m_c_generator.translate_function(name, *fn.script_function, fn_config);

	if (m_c_generator.get_fallback_count() > 0) {
		log(config(),
		    engine(),
		    LogSeverity::ASEA_PERF_HINT,
		    "Number of fallbacks for module \"{}\": {}",
		    name,
		    m_c_generator.get_fallback_count());
	}

	std::vector<std::pair<asPWORD*, asPWORD>> jit_entry_args;
	for (InsRef ins : get_bytecode(*fn.script_function)) {
		if (ins.opcode() == asBC_JitEntry) {
			jit_entry_args.emplace_back(&ins.pword0(), ins.pword0());
		}
	}

	auto [async_fn_it, success] = m_async_codegen_functions.try_emplace(
	    fn.script_function,
	    std::unique_ptr<AsyncMirFunction>{new AsyncMirFunction{
	        .jit_engine        = this,
	        .script_function   = fn.script_function,
	        .jit_entry_args    = std::move(jit_entry_args),
	        .deferred_bindings = std::move(deferred_bindings),
	        .c_name            = c_name,
	        .c_source          = m_c_generator.finalize_context(),
	        .pretty_name       = name,
	        .compiled          = {}
	    }}
	);
	auto& async_fn = *async_fn_it->second;

	if (config().debug.dump_c_code || (config().debug.allow_function_metadata_debug && fn_config.dump_c)) {
		angelsea_assert(config().debug.dump_c_code_file != nullptr);
		for (const char* block : async_fn.c_source.source_bits) {
			std::ignore = fputs(block, config().debug.dump_c_code_file);
		}
		std::ignore = fflush(config().debug.dump_c_code_file);
	}
	setup_jit_callback(*fn.script_function, jit_entry_await_async, &async_fn, true);

	m_lazy_functions.erase(fn.script_function);

	{
		std::unique_lock lk{m_termination_mutex};
		++m_terminating_threads;
	}
	if (m_compile_callback) {
		m_compile_callback(
		    +[](void* ud) {
			    auto& async_fn = *static_cast<AsyncMirFunction*>(ud);
			    async_fn.jit_engine->codegen_async_function(async_fn);
		    },
		    &async_fn
		);
	} else {
		codegen_async_function(async_fn);
	}

	return true;
}

void MirJit::codegen_async_function(AsyncMirFunction& fn) {
#if !ASEA_NO_MIR
	Mir compile_mir;
	{
		C2Mir c2mir{compile_mir};

		std::array macros{
		    // Trigger the various definitions and macros of the generated header
		    c2mir_macro_command{.def_p = int(true), .name = "ASEA_SUPPORT", .def = "1"},
#ifdef _MSC_VER
		    c2mir_macro_command{.def_p = int(true), .name = "ASEA_ABI_MSVC", .def = "1"},
#endif
		};

		c2mir_options c_options{
		    .message_file       = config().debug.c2mir_diagnostic_file,
		    .debug_p            = int(false),
		    .verbose_p          = int(false),
		    .ignore_warnings_p  = int(false),
		    .no_prepro_p        = int(false),
		    .prepro_only_p      = int(false),
		    .syntax_only_p      = int(false),
		    .pedantic_p         = int(false), // seems to break compile..?
		    .asm_p              = int(false),
		    .object_p           = int(false),
		    .module_num         = 0, // ?
		    .prepro_output_file = nullptr,
		    .output_file_name   = nullptr,
		    .macro_commands_num = macros.size(),
		    .include_dirs_num   = 0,
		    .macro_commands     = macros.data(),
		    .include_dirs       = nullptr,
		};

		InputData input_data(fn.c_source);
		if (c2mir_compile(compile_mir, &c_options, c2mir_getc_callback, &input_data, fn.pretty_name.c_str(), nullptr)
		    == 0) {
			log(config(), engine(), LogSeverity::ASEA_ERROR, "Failed to compile C for \"{}\"", fn.pretty_name.c_str());
			angelsea_assert(false); // FIXME: error handling
		}

		fn.compiled.module = DLIST_TAIL(MIR_module_t, *MIR_get_module_list(compile_mir));

		// trigger MIR linking and codegen
		//
		// this MUST in all circumstances be a full compile as the called code should never ever call into MIR code from
		// thunks, which would not be thread safe!
		//
		// this code is on the edge of reasonableness, because I don't think MIR really intends you to execute code in a
		// context that is having new stuff generated for it. at the same time, it doesn't seem like MIR ever
		// manipulates data actively pointed at by already generated code.
		// so long as lazy generation thunks are not being used, it looks like it should be ok, and it doesn't cause
		// issues with a commercial app.
		//
		// TODO: move this to its own task in a way that allows more threading
		// TODO: pool of MIR contexts to allow parallelizing further?
		// TODO: moving parts of the opt pipeline so more stuff can be done in the original context in MIR? might be
		// easy, might be complicated.
		{
			std::lock_guard lk{m_mir_lock};
			MIR_change_module_ctx(compile_mir, fn.compiled.module, m_mir);
			MIR_load_module(m_mir, fn.compiled.module);

			MIR_item_t mir_entry_fn;
			bool       found = false;
			for (MIR_item_t mir_fn = DLIST_HEAD(MIR_item_t, fn.compiled.module->items); mir_fn != nullptr;
			     mir_fn            = DLIST_NEXT(MIR_item_t, mir_fn)) {
				if (mir_fn->item_type == MIR_func_item && std::string_view{mir_fn->u.func->name} == fn.c_name) {
					found        = true;
					mir_entry_fn = mir_fn;
					break;
				}
			}

			if (!found) {
				log(config(), engine(), LogSeverity::ASEA_ERROR, "Function compile failed!");
				setup_jit_callback(*fn.script_function, nullptr, nullptr, true);
				m_async_codegen_functions.erase(fn.script_function);
				return;
			}

			MIR_gen_init(m_mir);

			MIR_gen_set_debug_file(m_mir, config().debug.mir_diagnostic_file);
			MIR_gen_set_debug_level(m_mir, config().debug.mir_debug_level);

			MIR_gen_set_optimize_level(m_mir, config().mir_optimization_level);

			for (const auto& [c_name, raw_value] : fn.deferred_bindings) {
				MIR_load_external(m_mir, c_name.c_str(), raw_value);
			}

			MIR_link(m_mir, MIR_set_gen_interface, nullptr);

			fn.compiled.jit_function = std::bit_cast<asJITFunction>(MIR_gen(m_mir, mir_entry_fn));

			if (config().debug.dump_mir_code) {
				angelsea_assert(config().debug.dump_mir_code_file != nullptr);
				MIR_output(m_mir, config().debug.dump_mir_code_file);
			}

			MIR_gen_finish(m_mir);

			if (config().hack_mir_minimize) {
				MIR_minimize_module(m_mir, fn.compiled.module);
				MIR_minimize(m_mir);
			}
		}
	} // must destroy C2Mir before MirJit potentially gets destroyed
#endif // !ASEA_NO_MIR

	{
		std::unique_lock lk{m_async_finalize_mutex};
		if (auto destruct_it = std::find_if(
		        m_async_cancelled_functions.begin(),
		        m_async_cancelled_functions.end(),
		        [&](const auto& ptr) { return ptr.get() == &fn; }
		    );
		    destruct_it != m_async_cancelled_functions.end()) {
			m_async_cancelled_functions.erase(destruct_it);
		} else {
			fn.compiled.ready.store(true);

			// move self to finished list
			auto it = m_async_codegen_functions.find(fn.script_function);
			m_async_finished_functions.emplace(fn.script_function, std::move(it->second));
			m_async_codegen_functions.erase(it);
		}
	}

	std::unique_lock lk{m_termination_mutex};
	--m_terminating_threads;
	m_termination_cv.notify_one();
}

void MirJit::link_ready_functions() {
	// FIXME: locking more than necessary
	std::lock_guard lk{m_async_finalize_mutex};
	for (auto& [script_fn, finished_fn] : m_async_finished_functions) {
		link_function(*finished_fn);
	}
	m_async_finished_functions.clear();
}

void MirJit::link_function(AsyncMirFunction& fn) {
	for (auto [ptr, arg] : fn.jit_entry_args) {
		*ptr = arg;
	}

	m_ignore_unregister             = fn.script_function;
	[[maybe_unused]] const auto err = set_jit_function(*fn.script_function, fn.compiled.jit_function);
	angelsea_assert(err == asSUCCESS);
	m_ignore_unregister = nullptr;
}

void MirJit::setup_jit_callback(asIScriptFunction& function, asJITFunction callback, void* ud, bool ignore_unregister) {
	for (InsRef ins : get_bytecode(function)) {
		if (ins.opcode() == asBC_JitEntry) {
			ins.pword0() = std::bit_cast<asPWORD>(ud);
		}
	}

	if (ignore_unregister) {
		m_ignore_unregister = &function;
	}
	set_jit_function(function, callback);
	if (ignore_unregister) {
		m_ignore_unregister = nullptr;
	}
}

void MirJit::discover_fn_config() {
	if (!m_request_fn_config_callback) {
		return;
	}

	for (auto& [script_fn, lazy_fn] : m_lazy_functions) {
		if (lazy_fn.fn_config.has_value()) {
			continue;
		}

		lazy_fn.fn_config = m_request_fn_config_callback(*script_fn);
	}

	if (m_config.triggers.eager) {
		while (!m_lazy_functions.empty()) {
			// this should always pop it from lazy_functions at this stage
			std::ignore = translate_lazy_function(m_lazy_functions.begin()->second);
		}
	}
}

} // namespace angelsea::detail
