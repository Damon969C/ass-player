#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-windows}"
QT_PREFIX="${QT_WINDOWS_PREFIX:-}"
QT_HOST_PREFIX="${QT_HOST_PREFIX:-/usr}"

if ! command -v cmake >/dev/null 2>&1; then
  printf '缺少 cmake，请先安装 cmake。\n' >&2
  exit 1
fi

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
  printf '缺少 x86_64-w64-mingw32-g++，请先安装 mingw-w64。\n' >&2
  exit 1
fi

if [ -z "$QT_PREFIX" ]; then
  printf '缺少 QT_WINDOWS_PREFIX，请指向 Windows x86_64 目标的 Qt6 安装前缀。\n' >&2
  printf '示例: QT_WINDOWS_PREFIX=/opt/qt6-windows-static bash qt6-refactor/scripts/cross-build-windows.sh\n' >&2
  exit 1
fi

if [ ! -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ] && [ ! -f "$QT_PREFIX/lib64/cmake/Qt6/Qt6Config.cmake" ]; then
  printf 'QT_WINDOWS_PREFIX 未找到 Qt6Config.cmake: %s\n' "$QT_PREFIX" >&2
  printf '该前缀必须是 Windows x86_64 目标 Qt6，而不是 Linux 本机 Qt6。\n' >&2
  exit 1
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT_WINDOWS_PREFIX="$QT_PREFIX" \
  -DQT_HOST_PATH="$QT_HOST_PREFIX" \
  -DQT_HOST_PATH_CMAKE_DIR="$QT_HOST_PREFIX/lib/x86_64-linux-gnu/cmake" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/cmake/windows-mingw-toolchain.cmake" \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX;$QT_PREFIX/lib/cmake"

cmake --build "$BUILD_DIR" --config Release --parallel
