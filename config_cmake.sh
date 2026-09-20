#!/bin/bash -e
source "$(dirname "$0")/Source/ultra-shared/scripts/preamble.sh"

# Pre-seeded configure-check results harvested from a previous CMakeCache.txt.
# Skips the slow try_compile probes (mainly libarchive's) on a fresh Builds folder.
# Delete the seed file and reconfigure to regenerate it after a toolchain change.
SEED_ARGS=()
if [ -f "Tools/configure-seed-$TOOLCHAIN.cmake" ]; then
  SEED_ARGS=(-C "Tools/configure-seed-$TOOLCHAIN.cmake")
fi

# Force branch-tracking deps to fetch the latest tip
rm -f "$BUILD_DIR"/CMakeFiles/fc-stamp/{juce,melatonin_inspector,melatonin_blur}/update.stamp

if [ "$TOOLCHAIN" = "xcode" ]; then
    cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | tee "$LOG_DIR/configure.log"
else
    cmake --preset $TOOLCHAIN "${SEED_ARGS[@]}" 2>&1 | tee "$LOG_DIR/configure.log"
fi

# Skip git checkout lines quoting commit messages
if grep -viE '^HEAD is now at' "$LOG_DIR/configure.log" | grep -qiE 'CMake Warning|CMake Error|warning:|error:'; then
    read -p "Warnings in $LOG_DIR/configure.log, press enter to close"
fi
