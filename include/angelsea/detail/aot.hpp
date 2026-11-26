// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace angelsea::detail {

/// Layout of one entry of `asea_aot_functions` in the generated library.
struct AotFunctionEntry {
	const char* name;
	void*       fn;
};

/// One exported function: the C symbol of its JIT entry point.
struct AotFunction {
	std::string c_name;
};

/// Rewrites generated C so that no symbol is resolved at link time. Every declaration of a symbol of \p table_symbols
/// (extern arrays, objects and functions, runtime helper prototypes) is replaced by a macro that reads the symbol's
/// address from `asea_aot_ext[i]`, and the tables a loader needs are appended:
///  - `asea_aot_ext` / `asea_aot_ext_names` / `asea_aot_ext_count`: address slots and their symbol names,
///  - `asea_aot_functions` / `asea_aot_function_count`: entry points by symbol name,
///  - `asea_aot_version`: the AngelScript release the code was generated for.
/// \p used_symbols receives the names in table order (only those that were declared in \p c_source).
std::string aot_redirect_externs(
    const std::string&                   c_source,
    const std::vector<std::string>&      table_symbols,
    const std::vector<AotFunction>&      functions,
    int                                  angelscript_version,
    std::vector<std::string>&            used_symbols
);

/// Shared library helpers (LoadLibrary / dlopen). The library stays loaded for the life of the process.
void* aot_open_library(const char* path);
void* aot_symbol(void* library, const char* name);

/// FNV-1a over a string, used by the manifest to detect a stale export.
unsigned long long aot_hash(std::string_view data);

} // namespace angelsea::detail
