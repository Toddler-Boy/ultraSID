#!/bin/bash
# Build and run hardwareoutput_tune_test against libSidplayEZ. Usage: Tests/hardwareoutput_tune_test.sh <tune.sid>...
# BUILD_DIR overrides the object directory (default: ./build-writesink), CXX the compiler (default: clang++, gcc rejects the unnamed-enum aliases upstream uses).
set -e
CXX="${CXX:-clang++}"
EXTRA="${EXTRA:-}"
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../Source/libSidplayEZ/src"
out="${BUILD_DIR:-$here/../build-writesink}"
mkdir -p "$out"

flags="$EXTRA -std=c++2b -O2 -fno-math-errno -msse4.2 -I$src"
objs=()
while IFS= read -r f; do
	o="$out/$(echo "${f#$src/}" | tr '/' '_').o"
	if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$src/sidemu.h" -nt "$o" ] || [ "$src/player.h" -nt "$o" ]; then
		echo "CXX $f"
		( $CXX $flags -c "$f" -o "$o.tmp" && mv "$o.tmp" "$o" ) &
	fi
	objs+=("$o")
done < <(find "$src" -name '*.cpp' -not -path '*/EZ/*')
aud="$here/../Source/Audio"
lusb="$(pkg-config --cflags libusb-1.0)"
for f in "$aud/HardwareOutput.cpp" "$aud/USBSID/USBSID.cpp" "$aud/USBSID/USBSID_Manager.cpp"; do
	o="$out/$(basename "$f").o"
	( $CXX $flags $lusb -I"$aud" -Wno-everything -c "$f" -o "$o.tmp" && mv "$o.tmp" "$o" ) &
	objs+=("$o")
done
( $CXX $flags -c "$here/hardwareoutput_tune_test.cpp" -o "$out/hardwareoutput_tune_test.o.tmp" && mv "$out/hardwareoutput_tune_test.o.tmp" "$out/hardwareoutput_tune_test.o" ) &
wait
${LD:-g++} "${objs[@]}" "$out/hardwareoutput_tune_test.o" -o "$out/hardwareoutput_tune_test" -pthread $(pkg-config --libs libusb-1.0)
"$out/hardwareoutput_tune_test" "$@"
