#!/bin/bash -e
source "$(dirname "$0")/Source/ultra-shared/scripts/preamble.sh"

# Pre-seeded configure-check results harvested from a previous CMakeCache.txt.
# Skips the slow try_compile probes (mainly libarchive's) on a fresh Builds folder.
# Delete the seed file and reconfigure to regenerate it after a toolchain change.
SEED_ARGS=()
if [ -f "Tools/configure-seed-$TOOLCHAIN.cmake" ]; then
  SEED_ARGS=(-C "Tools/configure-seed-$TOOLCHAIN.cmake")
fi

mkdir -p Builds/logs

# Force branch-tracking deps to fetch the latest tip
rm -f Builds/*/CMakeFiles/fc-stamp/{juce,melatonin_inspector,melatonin_blur}/update.stamp

cmake --preset $TOOLCHAIN "${SEED_ARGS[@]}" 2>&1 | tee Builds/logs/configure.log

# The mac's daily build tree is the Ninja one, keep it in step with the xcode preset
if [ "$TOOLCHAIN" = "xcode" ]; then
    cmake -B Builds/mac -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | tee -a Builds/logs/configure.log
fi

# Skip git checkout lines quoting commit messages
if grep -viE '^HEAD is now at' Builds/logs/configure.log | grep -qiE 'CMake Warning|CMake Error|warning:|error:'; then
    read -p "Warnings in Builds/logs/configure.log, press enter to close"
fi
