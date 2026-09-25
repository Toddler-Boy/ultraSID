#!/bin/bash
# Build and run writesink_test against libSidplayEZ. Usage: Tests/writesink_test.sh <tune.sid>...
# BUILD_DIR overrides the object directory (default: Builds/writesink), CXX the compiler (default: clang++, gcc rejects the unnamed-enum aliases upstream uses).
set -e
CXX="${CXX:-clang++}"
EXTRA="${EXTRA:-}"
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../Source/libSidplayEZ/src"
out="${BUILD_DIR:-$here/../Builds/writesink}"
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
( $CXX $flags -c "$here/writesink_test.cpp" -o "$out/writesink_test.o.tmp" && mv "$out/writesink_test.o.tmp" "$out/writesink_test.o" ) &
wait
${LD:-g++} "${objs[@]}" "$out/writesink_test.o" -o "$out/writesink_test" -pthread
"$out/writesink_test" "$@"
