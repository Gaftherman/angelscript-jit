#!/bin/sh
# AOT check: export every script of the corpus, build it with clang, load it and compare with the VM.
# usage: run.sh <angelsea-differential> <work dir> [extra clang flags]
# CC=cl builds with MSVC (run from a vcvars shell; EXT=dll).
# On Windows pass -DASEA_ABI_MSVC=1 and set EXT=dll; on Linux EXT=so and -fPIC.
diff_bin=$1; work=$2; shift 2
ext=${EXT:-so}
cc=${CC:-clang}
here=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$work"
fail=0
for f in "$here"/scripts/*.as "$here"/bench/*.as; do
	b=$(basename "$f" .as)
	"$diff_bin" --all "$f" >/dev/null 2>&1 || { echo "skip $b (does not run with the JIT alone)"; continue; }
	rm -f "$work/$b".*
	ASEA_EXPORT_C="$work/$b" "$diff_bin" --all "$f" >/dev/null 2>&1
	[ -f "$work/$b.c" ] || continue
	if [ "$cc" = cl ]; then
		# the x64 branch of the generated ABI ladder needs this flag; a 32-bit target must not set it
		msvc_abi=; [ "${VSCMD_ARG_TGT_ARCH:-x64}" = x64 ] && msvc_abi=/DASEA_ABI_MSVC=1
		build() { (cd "$work" && MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*" cl /nologo /TC /O2 /LD "/FI$(cd "$here/aot" && pwd -W)/msvc_shim.h" /DASEA_SUPPORT=1 $msvc_abi "$b.c" "/Fe:$b.$ext" >"$b.err" 2>&1); }
	else
		build() { $cc -shared -O2 -DASEA_SUPPORT=1 -Wno-everything "$@" "$work/$b.c" -o "$work/$b.$ext" 2>"$work/$b.err"; }
	fi
	if ! build "$@"; then
		echo "BUILD FAIL $b"; fail=1; continue
	fi
	out=$(ASEA_HITS=999999999 ASEA_AOT="$work/$b" "$diff_bin" --all "$f" 2>&1)
	loaded=$(echo "$out" | sed -n 's/LoadAot: \([0-9]*\) functions/\1/p' | head -1)
	if echo "$out" | grep -q "MISMATCH" || ! echo "$out" | grep -q "all scripts match"; then
		echo "MISMATCH $b"; fail=1
	else
		echo "ok $b (aot functions: ${loaded:-0})"
	fi
done
exit $fail
