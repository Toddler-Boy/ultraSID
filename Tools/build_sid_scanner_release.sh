#!/bin/bash -e
source "$(dirname "$0")/../Source/ultra-shared/scripts/preamble.sh"

# Release = the sid_scanner build (local renders run the Release exe)
mkdir -p Builds/logs
cmake --build --preset $TOOLCHAIN --config Release --target psiddrv 2>&1 | tee Builds/logs/build_sid_scanner_release.log
cmake --build --preset $TOOLCHAIN --config Release --target sid_scanner --parallel 2>&1 | tee -a Builds/logs/build_sid_scanner_release.log

if grep -qiE 'warning|error' Builds/logs/build_sid_scanner_release.log; then
    read -p "Warnings in Builds/logs/build_sid_scanner_release.log, press enter to close"
fi
