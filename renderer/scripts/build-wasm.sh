#!/usr/bin/env bash
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Builds the WASM artifact (plan Phase 3) through the dedicated superbuild
# in cpp/wasm-superbuild (Telegram Desktop's own build system has no
# Emscripten platform branch).
#
# Requirements:
#   - emsdk activated at the version Qt-wasm was built with (3.1.70 for
#     Qt 6.9.2); a mismatch is a hard configure error.
#   - Qt for WebAssembly + its matching host Qt, e.g.
#       aqt install-qt all_os wasm 6.9.2 wasm_singlethread --autodesktop \
#           -O "$HOME/qt-wasm"
#   - a completed native build (scripts/build-native.sh): TDesktop's code
#     generators must run on the host.
set -euo pipefail

renderer_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
upstream="${renderer_dir}/upstream/tdesktop"
build_dir="${renderer_dir}/build/wasm"
qt_root="${QT_WASM_ROOT:-$HOME/qt-wasm/6.9.2}"
native_build="${TTR_NATIVE_BUILD_DIR:-${renderer_dir}/build/native}"

command -v emcc >/dev/null || {
    echo "emcc not found; source \"$EMSDK/emsdk_env.sh\" first" >&2
    exit 1
}
[[ -d "${qt_root}/wasm_singlethread" ]] || {
    echo "Qt for WebAssembly not found at ${qt_root}/wasm_singlethread" >&2
    echo "set QT_WASM_ROOT or install it (see the header of this script)" >&2
    exit 1
}
[[ -x "${native_build}/Telegram/codegen/codegen/style/codegen_style" ]] || {
    echo "host code generators missing; run scripts/build-native.sh first" >&2
    exit 1
}

# Third-party sources the superbuild compiles for wasm and that are not
# vendored in this repository.
ada_dir="${renderer_dir}/cpp/wasm-superbuild/third_party/ada"
if [[ ! -f "${ada_dir}/ada.cpp" ]]; then
    echo "fetching ada singleheader..."
    mkdir -p "${ada_dir}"
    ada_version="v2.9.2"
    base="https://github.com/ada-url/ada/releases/download/${ada_version}"
    curl -fsSL "${base}/singleheader.zip" -o "${ada_dir}/singleheader.zip"
    (cd "${ada_dir}" && unzip -oq singleheader.zip && rm -f singleheader.zip)
fi
boost_dir="${renderer_dir}/cpp/wasm-superbuild/third_party/boost-headers"
if [[ ! -e "${boost_dir}/boost" ]]; then
    [[ -d /usr/include/boost ]] || {
        echo "Boost headers not found; install libboost-dev" >&2
        exit 1
    }
    mkdir -p "${boost_dir}"
    ln -sfn /usr/include/boost "${boost_dir}/boost"
fi

apply_patches() {
    for repo in tdesktop:"" lib_ui:Telegram/lib_ui lib_base:Telegram/lib_base \
            lib_crl:Telegram/lib_crl cmake_helpers:cmake; do
        local name="${repo%%:*}"
        local path="${upstream}/${repo#*:}"
        git -C "${path}" apply "${renderer_dir}/patches/${name}"/*.patch \
            2>/dev/null || true
    done
}
apply_patches

cmake -S "${renderer_dir}/cpp/wasm-superbuild" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${qt_root}/wasm_singlethread/lib/cmake/Qt6/qt.toolchain.cmake" \
    -DQT_CHAINLOAD_TOOLCHAIN_FILE="${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" \
    -DQT_HOST_PATH="${qt_root}/gcc_64" \
    -DTTR_NATIVE_BUILD_DIR="${native_build}"

cmake --build "${build_dir}" --target ttr_wasm_host "$@"

mkdir -p "${renderer_dir}/dist"
cp "${build_dir}/ttr-renderer/ttr-renderer.js" \
    "${build_dir}/ttr-renderer/ttr-renderer.wasm" \
    "${renderer_dir}/dist/"
echo "artifact: ${renderer_dir}/dist/ttr-renderer.{js,wasm}"
