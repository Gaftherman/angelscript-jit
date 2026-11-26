#!/bin/sh
# Runs inside an i386/debian container with the repository mounted read-only at /src.
# MIR has no 32-bit backend, so the JIT is built without it (ASEA_NO_MIR, automatic on 32-bit targets) and only the
# AOT path is checked: every script of the corpus is exported, built with gcc, loaded and compared with the VM.
# usage: docker run --rm -v "$PWD:/src:ro" i386/debian:12 sh /src/tests/aot/ci-i386.sh
set -e
apt-get update >/dev/null
DEBIAN_FRONTEND=noninteractive apt-get install -y cmake g++ ninja-build python3 git >/dev/null
git config --global --add safe.directory '*'
echo "target: $(getconf LONG_BIT)-bit"
cmake -S /src -B /tmp/b -G Ninja -DCMAKE_BUILD_TYPE=Release -DASEA_ENABLE_TESTING=ON
cmake --build /tmp/b --target angelsea-differential
cd /src
CC=gcc EXT=so sh tests/aot/run.sh /tmp/b/tests/angelsea-differential /tmp/aot -fPIC -O2 -fno-strict-aliasing
