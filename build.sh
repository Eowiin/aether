#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

case "${1:-debug}" in
  debug) config=Debug ;;
  release) config=Release ;;
  *) echo "Usage: ./build.sh [debug|release]"; exit 1 ;;
esac

# Sur macOS, /usr/bin/c++ est AppleClang : on force g++ de Homebrew sauf si CXX est déjà défini
if [[ "$(uname)" == "Darwin" ]]; then
  export CXX="${CXX:-g++-16}"
fi

cmake -S . -B "build/$config" -G Ninja -DCMAKE_BUILD_TYPE="$config"
cmake --build "build/$config" --parallel
