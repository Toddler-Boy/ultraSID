#!/bin/bash
# Build and run hardwareoutput_test. BUILD_DIR overrides the object directory (default: ./build-hwout).
set -e
CXX="${CXX:-g++}"
here="$(cd "$(dirname "$0")" && pwd)"
aud="$here/../Source/Audio"
out="${BUILD_DIR:-$here/../build-hwout}"
mkdir -p "$out"

flags="-std=c++20 -O1 -g $(pkg-config --cflags libusb-1.0) -I$aud"
srcs=("$aud/HardwareOutput.cpp" "$aud/USBSID/USBSID.cpp" "$aud/USBSID/USBSID_Manager.cpp" "$here/hardwareoutput_test.cpp")
objs=()
for f in "${srcs[@]}"; do
	o="$out/$(basename "$f").o"
	$CXX $flags -c "$f" -o "$o" &
	objs+=("$o")
done
wait
$CXX "${objs[@]}" -o "$out/hardwareoutput_test" -pthread $(pkg-config --libs libusb-1.0)
"$out/hardwareoutput_test"
