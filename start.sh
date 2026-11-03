#!/usr/bin/env bash
set -euo pipefail

# 从脚本所在目录构建，允许从其他工作目录启动。
project_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$project_directory"

if [[ -n "${LY27_PRESET:-}" ]]; then
    build_preset="$LY27_PRESET"
else
    available_presets="$(cmake --list-presets)"
    case "$available_presets" in
        *'"local-dev"'*) build_preset="local-dev" ;;
        *) build_preset="dev" ;;
    esac
fi

# 增量构建后启动，避免修改界面后误运行旧程序。
cmake --preset "$build_preset" -DLY27_BUILD_DESKTOP=ON
cmake --build --preset "$build_preset" --target ly27_desktop --parallel 2
exec "$project_directory/build/$build_preset/bin/ly27_desktop" "$@"
