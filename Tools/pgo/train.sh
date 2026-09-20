#!/bin/bash -e
source "$(dirname "$0")/../../Source/ultra-shared/scripts/preamble.sh"

# PGO training for the emulation core: instrumented Release build of
# sidplay_ab_test, one run over the test tunes, profile merged into
# Tools/pgo/sidplayez-<mangling>.profdata with the libSidplayEZ commit it was
# trained on beside it, then a plain reconfigure so every later build picks
# the profile up. Profiles are keyed by mangled name, so Windows (msvc) and the
# mac (itanium, serves Linux too) each train their own: run this on both after
# engine work, commit the four files.

case "$TOOLCHAIN" in
    vs)    profile=Tools/pgo/sidplayez-msvc.profdata
           exe=$BUILD_DIR/sidplay_ab_test_artefacts/Release/sidplay_ab_test.exe
           profdata=$(ls "/c/Program Files/Microsoft Visual Studio/"*/*/VC/Tools/Llvm/x64/bin/llvm-profdata.exe 2>/dev/null | head -1) ;;
    xcode) profile=Tools/pgo/sidplayez-itanium.profdata
           exe=$BUILD_DIR/sidplay_ab_test_artefacts/Release/sidplay_ab_test
           profdata=$(xcrun -f llvm-profdata) ;;
    *)     echo "Train on Windows or the mac"; exit 1 ;;
esac
[ -x "$profdata" ] || { echo "llvm-profdata not found"; exit 1; }
[ -f "$BUILD_DIR/CMakeCache.txt" ] || { echo "No configured build in $BUILD_DIR"; exit 1; }

raw=$BUILD_DIR/pgo
mkdir -p "$raw"
rm -f "$raw"/*.profraw

cmake -B "$BUILD_DIR" -DULTRA_PGO_GENERATE=ON > "$LOG_DIR/pgo_configure.log" 2>&1
cmake --build "$BUILD_DIR" --config Release --target sidplay_ab_test --parallel > "$LOG_DIR/pgo_build.log" 2>&1

# 2 = the instrumented binary tripped the performance check, expected.
# Six threads: the profile counters are shared, more threads only fight over them
rc=0
LLVM_PROFILE_FILE="$raw/%p.profraw" "$exe" -j6 > "$LOG_DIR/pgo_train.log" 2>&1 || rc=$?
[ "$rc" -eq 0 ] || [ "$rc" -eq 2 ] || { echo "training run failed ($rc), see $LOG_DIR/pgo_train.log"; exit 1; }

"$profdata" merge -o "$profile" "$raw"/*.profraw
git -C Source/libSidplayEZ rev-parse HEAD > "$profile.commit"
cmake -B "$BUILD_DIR" -DULTRA_PGO_GENERATE=OFF > "$LOG_DIR/pgo_configure.log" 2>&1

echo "Profile written to $profile, rebuild Release to use it"
