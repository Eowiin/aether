#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

extra_args=()

case "${1:-debug}" in
  debug) config=Debug; build_dir=Debug ;;
  release) config=Release; build_dir=Release ;;
  sanitize)
    config=Debug
    build_dir=Sanitize
    extra_args+=(-DAETHER_SANITIZE=ON)
    # LeakSanitizer se bloque à la sortie avec libasan de GCC sur macOS
    if [[ "$(uname)" == "Darwin" ]]; then
      export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
    fi
    ;;
  *) echo "Usage: ./build.sh [debug|release|sanitize]"; exit 1 ;;
esac

# Sur macOS, /usr/bin/c++ est AppleClang : on force g++ de Homebrew sauf si CXX est déjà défini
if [[ "$(uname)" == "Darwin" ]]; then
  export CXX="${CXX:-g++-16}"
fi

cmake -S . -B "build/$build_dir" -G Ninja -DCMAKE_BUILD_TYPE="$config" ${extra_args[@]+"${extra_args[@]}"}
cmake --build "build/$build_dir" --parallel
