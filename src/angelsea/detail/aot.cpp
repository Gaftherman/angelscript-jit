// SPDX-License-Identifier: BSD-2-Clause

#include <angelsea/detail/aot.hpp>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace angelsea::detail {

void* aot_open_library(const char* path) {
#if defined(_WIN32)
	return reinterpret_cast<void*>(LoadLibraryA(path));
#else
	return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

void* aot_symbol(void* library, const char* name) {
#if defined(_WIN32)
	return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(library), name));
#else
	return dlsym(library, name);
#endif
}

unsigned long long aot_hash(std::string_view data) {
	unsigned long long h = 1469598103934665603ULL;
	for (const unsigned char c : data) {
		h ^= c;
		h *= 1099511628211ULL;
	}
	return h;
}

namespace {

bool is_ident_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }

std::string_view trim(std::string_view s) {
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
		s.remove_prefix(1);
	}
	while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
		s.remove_suffix(1);
	}
	return s;
}

/// Parsed form of `[extern] TYPE NAME[]; | TYPE NAME; | TYPE NAME(ARGS);`
struct Declaration {
	std::string name;
	std::string type;
	std::string args;
	enum class Kind { Array, Object, Function } kind = Kind::Object;
};

bool parse_declaration(std::string_view line, const std::unordered_set<std::string>& table, Declaration& out) {
	line = trim(line);
	if (line.empty() || line.back() != ';') {
		return false;
	}
	if (line.find('=') != std::string_view::npos || line.find('{') != std::string_view::npos
	    || line.find("typedef") == 0 || line.find("return") == 0 || line.find("goto") == 0) {
		return false;
	}
	line.remove_suffix(1);
	line = trim(line);

	const bool has_extern = line.find("extern ") == 0;
	if (has_extern) {
		line.remove_prefix(7);
		line = trim(line);
	}

	// the trailing part decides the kind: `(ARGS)`, `[]` or neither
	std::string args;
	auto        kind = Declaration::Kind::Object;
	if (!line.empty() && line.back() == ')') {
		const auto open = line.find('(');
		if (open == std::string_view::npos) {
			return false;
		}
		args = std::string{line.substr(open + 1, line.size() - open - 2)};
		line = trim(line.substr(0, open));
		kind = Declaration::Kind::Function;
	} else if (!line.empty() && line.back() == ']') {
		const auto open = line.rfind('[');
		if (open == std::string_view::npos) {
			return false;
		}
		line = trim(line.substr(0, open));
		kind = Declaration::Kind::Array;
	}

	std::size_t name_begin = line.size();
	while (name_begin > 0 && is_ident_char(line[name_begin - 1])) {
		--name_begin;
	}
	const std::string name{line.substr(name_begin)};
	std::string       type{trim(line.substr(0, name_begin))};
	if (name.empty() || type.empty() || table.count(name) == 0) {
		return false;
	}
	// a call statement such as `asea_free(ptr);` has no type before the name, and the header's runtime prototypes (which do
	// not start with `extern`) only start with a plain C type word
	if (!has_extern) {
		const bool typed = type.find("void") == 0 || type.find("int") == 0 || type.find("float") == 0
		                   || type.find("double") == 0 || type.find("const") == 0 || type.find("as") == 0;
		if (!typed || kind != Declaration::Kind::Function) {
			return false;
		}
	}
	out = {name, type, args, kind};
	return true;
}

std::string macro_for(const Declaration& d, std::size_t index) {
	const std::string slot = "asea_aot_ext[" + std::to_string(index) + "]";
	switch (d.kind) {
	case Declaration::Kind::Function: {
		// a calling convention keyword goes inside the pointer declarator: `RET (__fastcall *)(ARGS)`
		constexpr std::string_view fastcall = " __fastcall";
		std::string                type     = d.type;
		std::string                convention;
		if (type.size() > fastcall.size() && type.compare(type.size() - fastcall.size(), fastcall.size(), fastcall) == 0) {
			type.resize(type.size() - fastcall.size());
			convention = "__fastcall ";
		}
		return "#define " + d.name + " ((" + type + " (" + convention + "*)(" + d.args + "))" + slot + ")\n";
	}
	case Declaration::Kind::Array:    return "#define " + d.name + " (*(" + d.type + " (*)[1])" + slot + ")\n";
	case Declaration::Kind::Object:   break;
	}
	// `extern void NAME;` is only ever used through its address
	const std::string object_type = d.type == "void" ? "char" : d.type;
	return "#define " + d.name + " (*(" + object_type + "*)" + slot + ")\n";
}

} // namespace

std::string aot_redirect_externs(
    const std::string&              c_source,
    const std::vector<std::string>& table_symbols,
    const std::vector<AotFunction>& functions,
    int                             angelscript_version,
    std::vector<std::string>&       used_symbols
) {
	const std::unordered_set<std::string>      table{table_symbols.begin(), table_symbols.end()};
	std::unordered_map<std::string, std::size_t> index_of;
	used_symbols.clear();

	std::string out;
	out.reserve(c_source.size() + 4096);
	out += "#if defined(_WIN32)\n#define ASEA_AOT_EXPORT __declspec(dllexport)\n#else\n"
	       "#define ASEA_AOT_EXPORT __attribute__((visibility(\"default\")))\n#endif\n"
	       "extern ASEA_AOT_EXPORT void* asea_aot_ext[];\n";

	std::size_t pos = 0;
	while (pos < c_source.size()) {
		auto end = c_source.find('\n', pos);
		if (end == std::string::npos) {
			end = c_source.size();
		}
		const std::string_view line{c_source.data() + pos, end - pos};
		pos = end + 1;

		Declaration decl;
		if (!parse_declaration(line, table, decl)) {
			out.append(line);
			out += '\n';
			continue;
		}
		if (index_of.emplace(decl.name, used_symbols.size()).second) {
			out += macro_for(decl, used_symbols.size());
			used_symbols.push_back(decl.name);
		}
	}

	// the tables: sized at the end, when every symbol is known
	out += "\nASEA_AOT_EXPORT void* asea_aot_ext[" + std::to_string(used_symbols.empty() ? 1 : used_symbols.size()) + "];\n";
	out += "ASEA_AOT_EXPORT const unsigned asea_aot_ext_count = " + std::to_string(used_symbols.size()) + ";\n";
	out += "ASEA_AOT_EXPORT const int asea_aot_version = " + std::to_string(angelscript_version) + ";\n";
	out += "ASEA_AOT_EXPORT const char* const asea_aot_ext_names[] = {";
	for (const auto& name : used_symbols) {
		out += "\"" + name + "\",";
	}
	out += "0};\n";
	out += "typedef struct { const char* name; void* fn; } asea_aot_function_entry;\n";
	out += "ASEA_AOT_EXPORT const asea_aot_function_entry asea_aot_functions[] = {";
	for (const auto& fn : functions) {
		out += "{\"" + fn.c_name + "\",(void*)&" + fn.c_name + "},";
	}
	out += "{0,0}};\n";
	out += "ASEA_AOT_EXPORT const unsigned asea_aot_function_count = " + std::to_string(functions.size()) + ";\n";
	return out;
}

} // namespace angelsea::detail
