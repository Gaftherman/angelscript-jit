// SPDX-License-Identifier: BSD-2-Clause

#include "angelsea/detail/mirjit.hpp"
#include <angelsea/jit.hpp>
#include <fstream>
#include <string>
#include <utility>

namespace angelsea {

Jit::Jit(const JitConfig& config, asIScriptEngine& engine) :
    m_compiler(std::make_unique<detail::MirJit>(config, engine)) {}

Jit::~Jit() {} // NOLINT: we'd rather not force the user to see the detail/ TUs, so for unique_ptr purposes we need the
               // destructor in this TU

#if ASEA_JIT_INTERFACE_V2
void Jit::NewFunction(asIScriptFunction* scriptFunc) { m_compiler->register_function(*scriptFunc); }

void Jit::CleanFunction(asIScriptFunction* scriptFunc, asJITFunction jitFunc) {
	m_compiler->unregister_function(*scriptFunc);
	std::ignore = jitFunc;
}
#else
int Jit::CompileFunction(asIScriptFunction* scriptFunc, asJITFunction* output) {
	// register_function installs the counting entry point itself (it writes the function's jitFunction), which is the
	// same field the engine passes as `output`
	std::ignore = output;
	m_compiler->register_function(*scriptFunc);
	return 0;
}

void Jit::ReleaseJITFunction(asJITFunction func) { std::ignore = func; }
#endif

void Jit::SetCompileCallback(std::function<void(CompileFunc*, void*)> callback) {
	m_compiler->set_compile_callback(std::move(callback));
}

void Jit::SetFnConfigRequestCallback(std::function<FnConfig(asIScriptFunction&)> callback, bool manual_discovery) {
	m_compiler->set_fn_config_request_callback(std::move(callback), manual_discovery);
}

bool Jit::ExportModuleC(asIScriptModule& module, const char* path) {
	std::string c_source, manifest;
	if (!m_compiler->export_module_c(module, c_source, manifest)) {
		return false;
	}
	const std::string base{path};
	for (const auto& [ext, text] : {std::pair{".c", &c_source}, std::pair{".externs", &manifest}}) {
		std::ofstream out(base + ext, std::ios::binary);
		out << *text;
		if (!out) {
			return false;
		}
	}
	return true;
}

std::size_t Jit::LoadAot(asIScriptModule& module, const char* library, const char* manifest) {
	return m_compiler->load_aot(module, library, manifest);
}

void Jit::DiscoverFnConfig() { m_compiler->discover_fn_config(); }

} // namespace angelsea