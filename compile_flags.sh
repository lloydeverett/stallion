#!/bin/bash

set -euo pipefail
shopt -s nullglob

ROOT="$(realpath $(pwd)/)"
BOOST_INCLUDE_DIRS="$(echo $ROOT/build/_deps/boost-src/libs/*/include $ROOT/build/_deps/boost-src/libs/*/*/include)"

echo "\
-std=c++23
-I$ROOT/build/_deps/ftxui-src/include
$(printf "--" "-I%s\n" $BOOST_INCLUDE_DIRS)
" >compile_flags.txt

echo "wrote compile_flags.txt"

rm -f ./build/compile_commands.json
