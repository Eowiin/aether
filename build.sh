#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

case "${1:-debug}" in
  debug) config=Debug ;;
  release) config=Release ;;
  *) echo "Usage: ./build.sh [debug|release]"; exit 1 ;;
esac

cmake -S . -B "build/$config" -G Ninja -DCMAKE_BUILD_TYPE="$config"
cmake --build "build/$config" --parallel
