#!/usr/bin/env sh
set -eu

LLVM_CONFIG="${LLVM_CONFIG:-/opt/homebrew/opt/llvm/bin/llvm-config}"

if [ -x "$LLVM_CONFIG" ]; then
    cmake -S . -B build -G Ninja -DLLVM_DIR="$("$LLVM_CONFIG" --cmakedir)"
else
    cmake -S . -B build -G Ninja
fi
