#!/bin/bash -e
source "$(dirname "$0")/../Source/ultra-shared/scripts/preamble.sh"

# Release = the sidid_search build (signature vetting runs the Release exe)
mkdir -p Builds/logs
cmake --build --preset $TOOLCHAIN --config Release --target sidid_search --parallel 2>&1 | tee Builds/logs/build_sidid_search_release.log

if grep -qiE 'warning|error' Builds/logs/build_sidid_search_release.log; then
    read -p "Warnings in Builds/logs/build_sidid_search_release.log, press enter to close"
fi
