#!/bin/bash -e
# Build (on demand) and run the FX / EQ golden-output regression +
# performance test from the repo root.
# Exit codes: 0 = PASS, 1 = audio mismatch, 2 = perf regression.

source "$(dirname "$0")/../Source/ultra-shared/scripts/preamble.sh"

cmake --build --preset $TOOLCHAIN --config Release --target fx_ab_test

if [ "$TOOLCHAIN" == "vs" ]; then
  EXE="Builds/vs/Release/fx_ab_test.exe"
else
  EXE="Builds/$TOOLCHAIN/fx_ab_test"
fi

exec "$EXE" "$@"
