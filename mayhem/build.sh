#!/usr/bin/env bash
#
# mayhem/build.sh — build MaterialX with sanitizers + coverage, the fuzz harnesses, the standalone
# reproducers, and (separately, with normal flags) the upstream test suite + the oracle KAT probe.
# Runs offline: MaterialX needs no network resources for this configuration.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

COMMON_CMAKE=(
  -G Ninja
  -DMATERIALX_BUILD_RENDER=ON
  -DMATERIALX_BUILD_GEN_GLSL=ON -DMATERIALX_BUILD_GEN_OSL=ON -DMATERIALX_BUILD_GEN_MDL=ON
  -DMATERIALX_BUILD_GEN_MSL=ON
  -DMATERIALX_BUILD_PYTHON=OFF -DMATERIALX_BUILD_JS=OFF -DMATERIALX_BUILD_VIEWER=OFF
  -DMATERIALX_BUILD_GRAPH_EDITOR=OFF -DMATERIALX_BUILD_DOCS=OFF -DMATERIALX_BUILD_OIIO=OFF
  -DMATERIALX_BUILD_OCIO=OFF -DMATERIALX_BUILD_USE_CCACHE=OFF -DMATERIALX_TEST_RENDER=OFF
  -DMATERIALX_INSTALL_PYTHON=OFF -DMATERIALX_INSTALL_RESOURCES=OFF
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX"
)
LIBS=(MaterialXGenOsl MaterialXGenMdl MaterialXGenMsl MaterialXGenGlsl MaterialXGenHw
      MaterialXGenShader MaterialXRender MaterialXFormat MaterialXCore)

# 1) Sanitized + coverage-instrumented libraries (the fuzzed code), installed under build/fuzz-install.
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
cmake -S . -B build/fuzz "${COMMON_CMAKE[@]}" -DMATERIALX_BUILD_TESTS=OFF -DMATERIALX_BUILD_RENDER_PLATFORMS=OFF \
      -DCMAKE_BUILD_TYPE= -DCMAKE_C_FLAGS="$FUZZ_FLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_FLAGS" \
      -DCMAKE_INSTALL_PREFIX="$SRC/build/fuzz-install"
cmake --build build/fuzz -j"$MAYHEM_JOBS"
cmake --install build/fuzz >/dev/null

# Baked-in data the generators read at runtime (found via the executable's location, /mayhem).
[ -d "$SRC/libraries" ]

# 2) Harnesses: fuzzer binary + standalone reproducer each, with LSan disabled at build time.
INC=(-I"$SRC/build/fuzz-install/include" -I"$SRC/mayhem")
LINK=(-Wl,--start-group)
for l in "${LIBS[@]}"; do LINK+=("$SRC/build/fuzz-install/lib/lib$l.a"); done
LINK+=(-Wl,--end-group -lpthread -ldl)

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o build/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build/standalone_main.o

for t in fuzz_xml; do
  $CXX -std=c++17 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "${INC[@]}" \
       "$SRC/mayhem/$t.cpp" build/lsan_off.o "${LINK[@]}" -o "/mayhem/$t"
  $CXX -std=c++17 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS "${INC[@]}" \
       "$SRC/mayhem/$t.cpp" build/standalone_main.o build/lsan_off.o "${LINK[@]}" -o "/mayhem/$t-standalone"
done

# 3) Upstream test suite + KAT probe with the project's NORMAL flags (no sanitizers, no -gdwarf-3).
cmake -S . -B build/tests "${COMMON_CMAKE[@]}" -DMATERIALX_BUILD_TESTS=ON -DMATERIALX_BUILD_RENDER_PLATFORMS=ON \
      -DMATERIALX_TEST_RENDER_GLSL=OFF \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
      -DCMAKE_INSTALL_PREFIX="$SRC/build/tests-install"
cmake --build build/tests -j"$MAYHEM_JOBS"
cmake --install build/tests >/dev/null

TLINK=(-Wl,--start-group)
for l in "${LIBS[@]}"; do TLINK+=("$SRC/build/tests-install/lib/lib$l.a"); done
TLINK+=(-Wl,--end-group -lpthread -ldl)
$CXX -std=c++17 -O1 $COVERAGE_FLAGS -I"$SRC/build/tests-install/include" \
     "$SRC/mayhem/kat.cpp" "${TLINK[@]}" -o /mayhem/mx_kat
