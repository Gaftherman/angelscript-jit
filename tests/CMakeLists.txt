# SPDX-License-Identifier: BSD-2-Clause

set(CATCH2_ROOT "vendor/Catch2")
add_subdirectory(${CATCH2_ROOT})

# Configure AngelScript add-ons
set(ANGELSCRIPT_ADDON_ROOT "${ASEA_ANGELSCRIPT_ROOT}/add_on")
add_library(angelscript-addons STATIC
    ${ANGELSCRIPT_ADDON_ROOT}/scriptarray/scriptarray.cpp
    ${ANGELSCRIPT_ADDON_ROOT}/scriptbuilder/scriptbuilder.cpp
    ${ANGELSCRIPT_ADDON_ROOT}/scriptstdstring/scriptstdstring.cpp
)
target_include_directories(angelscript-addons PUBLIC ${ANGELSCRIPT_ADDON_ROOT})
target_link_libraries(angelscript-addons PRIVATE asea_angelscript)

add_executable(angelsea-tests
	nanobench-impl.cpp
	abi.cpp
	booleans.cpp
	branching.cpp
	classmanip.cpp
	common.cpp
	configtest.cpp
	enums.cpp
	floatmath.cpp
	funcdefs.cpp
	functions.cpp
	globals.cpp
	integermath.cpp
	megatests.cpp
	recursion.cpp
	typedefs.cpp
)

target_include_directories(angelsea-tests PRIVATE vendor/nanobench/src/include)
target_link_libraries(angelsea-tests PRIVATE asea_angelscript_internal angelsea angelscript-addons Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${CATCH2_ROOT}/extras)
include(CTest)
include(Catch)
catch_discover_tests(
	angelsea-tests
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Differential host: the same script on the plain VM and on angelsea, outputs must match.
add_executable(angelsea-differential differential/main.cpp)
target_link_libraries(angelsea-differential PRIVATE asea_angelscript_internal angelsea angelscript-addons)
# The scripts of the Catch2 suite hold several test functions each, so --all runs every `void f()`. abi.as and
# bc_pga_var_test.as need functions that only the Catch2 host registers, and fib.as has no entry point of its own.
set(differential_scripts bfint classinheritance devirt enums funcdefs functions globals globalswithclasses
	refprimitives sharedfuncs stringmanip switch typedefs unalignedfields userclasses vec3f deeprecursion nullcalls misc_ops pow_ops)
list(TRANSFORM differential_scripts PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/")
list(TRANSFORM differential_scripts APPEND ".as")
add_custom_target(angelsea-bench COMMAND angelsea-differential --time 3 ${CMAKE_CURRENT_SOURCE_DIR}/bench/fib.as ${CMAKE_CURRENT_SOURCE_DIR}/bench/loops.as ${CMAKE_CURRENT_SOURCE_DIR}/bench/sieve.as ${CMAKE_CURRENT_SOURCE_DIR}/bench/nbody.as ${CMAKE_CURRENT_SOURCE_DIR}/bench/objects.as ${CMAKE_CURRENT_SOURCE_DIR}/bench/strings.as ${CMAKE_CURRENT_SOURCE_DIR}/bench/matmul.as ${CMAKE_CURRENT_SOURCE_DIR}/bench/virtual.as ${CMAKE_CURRENT_SOURCE_DIR}/bench/closures.as DEPENDS angelsea-differential USES_TERMINAL)
add_test(NAME differential COMMAND angelsea-differential --all ${differential_scripts})
