#!/usr/bin/env bash
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Configures the pinned TDesktop tree with the harness hook applied and
# builds the sessionless native host (plan Phase 2). Requires: g++/clang,
# cmake, ninja, Qt 6 dev packages, and the TDesktop Linux dependency set
# (see upstream docs/building-linux.md; on Ubuntu the packages installed by
# this script's --deps flag suffice for the harness targets).
set -euo pipefail

renderer_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
upstream="${renderer_dir}/upstream/tdesktop"
build_dir="${TTR_NATIVE_BUILD_DIR:-${renderer_dir}/build/native}"

# Overridable toolchain locations (defaults match the distro-Qt dev
# host; CI points both at an aqtinstall Qt so the host Qt version
# matches the pinned Qt-wasm exactly — see .github/workflows).
#   TTR_QT_HOST      Qt prefix, e.g. $HOME/qt-wasm/6.9.2/gcc_64
#   QSB_EXECUTABLE   qsb path (default /usr/lib/qt6/bin/qsb, or
#                    $TTR_QT_HOST/bin/qsb when TTR_QT_HOST is set)
qt_host="${TTR_QT_HOST:-}"
if [[ -n "${qt_host}" ]]; then
    qsb="${QSB_EXECUTABLE:-${qt_host}/bin/qsb}"
else
    qsb="${QSB_EXECUTABLE:-/usr/lib/qt6/bin/qsb}"
fi

# --codegen-only builds just the host code generators the WASM
# superbuild imports (codegen_style/lang/emoji); configure is identical.
targets=(ttr_native_host)
if [[ "${1:-}" == "--codegen-only" ]]; then
    targets=(codegen_style codegen_lang codegen_emoji)
    shift || true
fi

if [[ "${1:-}" == "--deps" ]]; then
    sudo apt-get install -y \
        g++ cmake ninja-build pkg-config \
        qt6-base-dev qt6-base-private-dev qt6-svg-dev \
        qt6-image-formats-plugins \
        libssl-dev zlib1g-dev libopus-dev \
        libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
        libswresample-dev libopenal-dev liblz4-dev libxxhash-dev \
        libminizip-dev protobuf-compiler libprotobuf-dev \
        libxcb1-dev libxcb-keysyms1-dev libxcb-record0-dev \
        libxcb-screensaver0-dev libglibmm-2.68-dev \
        libboost-dev libboost-regex-dev libboost-program-options-dev \
        libprotoc-dev libabsl-dev libavfilter-dev libavdevice-dev \
        libjpeg-dev libwebp-dev libpng-dev libtiff-dev liblzma-dev \
        libhunspell-dev libgtk-3-dev libgirepository1.0-dev libepoxy-dev \
        libfontconfig-dev libx11-xcb-dev libxcomposite-dev libxdamage-dev \
        libxext-dev libxfixes-dev libxrender-dev libxrandr-dev libxtst-dev \
        qt6-shadertools-dev autoconf automake libtool
    # Not packaged on Ubuntu; build from source into /usr/local:
    #   ada-url/ada (v2.9.2) and xiph/rnnoise. Ubuntu's protobuf ships no
    #   CMake package config, and tg_owt/tde2e (calls stack, never linked
    #   by the harness) need shim configs — see renderer/BUILD-STATUS.md.
    shift || true
fi

lib_ui_patches() {
    local mode="${1:-}"
    git -C "${upstream}/Telegram/lib_ui" apply ${mode} \
        "${renderer_dir}"/patches/lib_ui/*.patch 2>/dev/null || true
}

apply_patches() {
    lib_ui_patches
    git -C "${upstream}" apply --check "${renderer_dir}"/patches/tdesktop/*.patch \
        2>/dev/null || {
        echo "patches already applied or do not apply cleanly" >&2
        git -C "${upstream}" apply --reverse --check \
            "${renderer_dir}"/patches/tdesktop/*.patch 2>/dev/null \
            && echo "(already applied; continuing)" \
            || exit 1
        return
    }
    git -C "${upstream}" apply "${renderer_dir}"/patches/tdesktop/*.patch
}

revert_patches() {
    lib_ui_patches --reverse
    git -C "${upstream}" apply --reverse --check \
        "${renderer_dir}"/patches/tdesktop/*.patch 2>/dev/null \
        && git -C "${upstream}" apply --reverse \
            "${renderer_dir}"/patches/tdesktop/*.patch \
        || true
}

apply_patches
trap revert_patches EXIT

cmake -S "${upstream}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DTDESKTOP_API_TEST=ON \
    -DDESKTOP_APP_DISABLE_AUTOUPDATE=ON \
    -DDESKTOP_APP_DISABLE_CRASH_REPORTS=ON \
    -DDESKTOP_APP_USE_PACKAGED=ON \
    -DQSB_EXECUTABLE="${qsb}" \
    ${qt_host:+-DCMAKE_PREFIX_PATH="${qt_host}"} \
    -DTDESKTOP_RENDERER_HARNESS_DIR="${renderer_dir}/cpp"

cmake --build "${build_dir}" --target "${targets[@]}" "$@"

if [[ "${targets[0]}" == "ttr_native_host" ]]; then
    echo "native host: ${build_dir}/ttr_native_host"
else
    echo "host code generators: ${build_dir}/Telegram/codegen/codegen/"
fi
