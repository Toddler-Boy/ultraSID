#!/bin/bash -e
# Build (on demand) and run the libSidplayEZ golden-output regression +
# performance test from the repo root. Arguments are forwarded to the tool:
#   ./Tests/test_sidplay.sh [hvscRoot] [seconds]
# (roots live in Tests/data-roots.txt, machine-specific; the argument overrides $HVSC$)
# Exit codes: 0 = PASS, 1 = audio mismatch / load failure, 2 = perf regression.

source "$(dirname "$0")/../Source/ultra-shared/scripts/preamble.sh"

cmake --build --preset $TOOLCHAIN --config Release --target sidplay_ab_test

if [ "$TOOLCHAIN" == "vs" ]; then
  EXE="Builds/vs/Release/sidplay_ab_test.exe"
else
  EXE="Builds/$TOOLCHAIN/sidplay_ab_test"
fi

exec "$EXE" "$@"
