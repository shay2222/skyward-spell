#!/usr/bin/env bash
# Builds the game to docs/skyward.wasm with nothing but clang (no SDK, no standard library).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p docs

CXX="${CXX:-clang++}"
FLAGS=(
  --target=wasm32 -std=c++20 -O2
  -nostdlib -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics
  -mbulk-memory -mnontrapping-fptoint -msign-ext
  -fvisibility=hidden
  -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
)
LINK=(
  -Wl,--no-entry
  -Wl,--export=__wasm_call_ctors
  -Wl,--export-memory
  -Wl,--initial-memory=100663296   # 96 MB: atlas, scratch canvas and level arenas
  -Wl,--max-memory=268435456
  -Wl,-z,stack-size=1048576
  -Wl,--strip-debug
)

"$CXX" "${FLAGS[@]}" "${LINK[@]}" src/*.cpp -o docs/skyward.wasm
ls -l docs/skyward.wasm | awk '{print "built docs/skyward.wasm,", $5, "bytes"}'
