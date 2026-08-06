#!/usr/bin/env bash
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Builds the WASM artifact (plan Phase 3). Requires an activated Emscripten
# SDK (emcmake on PATH) and a Qt-for-WebAssembly installation whose version
# matches the pinned reference profile as closely as available; pass its
# path via QT_WASM_DIR. Single-threaded build: no COOP/COEP requirement.
set -euo pipefail

renderer_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
upstream="${renderer_dir}/upstream/tdesktop"
build_dir="${renderer_dir}/build/wasm"

: "${QT_WASM_DIR:?set QT_WASM_DIR to the Qt for WebAssembly prefix}"
command -v emcmake >/dev/null || {
    echo "emcmake not found; source emsdk_env.sh first" >&2
    exit 1
}

git -C "${upstream}" apply "${renderer_dir}"/patches/tdesktop/*.patch 2>/dev/null || true
trap 'git -C "${upstream}" apply --reverse "${renderer_dir}"/patches/tdesktop/*.patch 2>/dev/null || true' EXIT

emcmake cmake -S "${upstream}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${QT_WASM_DIR}" \
    -DTDESKTOP_API_TEST=ON \
    -DDESKTOP_APP_DISABLE_AUTOUPDATE=ON \
    -DDESKTOP_APP_DISABLE_CRASH_REPORTS=ON \
    -DTDESKTOP_RENDERER_HARNESS_DIR="${renderer_dir}/cpp"

cmake --build "${build_dir}" --target ttr_wasm_host "$@"

mkdir -p "${renderer_dir}/dist"
cp "${build_dir}/ttr-renderer.js" "${build_dir}/ttr-renderer.wasm" \
    "${renderer_dir}/dist/"
echo "artifact: ${renderer_dir}/dist/ttr-renderer.{js,wasm}"
