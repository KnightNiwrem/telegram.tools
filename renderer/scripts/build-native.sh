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
build_dir="${renderer_dir}/build/native"

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
        libxcb-screensaver0-dev libglibmm-2.68-dev
    shift || true
fi

apply_patches() {
    git -C "${upstream}" apply --check "${renderer_dir}"/patches/*.patch \
        2>/dev/null || {
        echo "patches already applied or do not apply cleanly" >&2
        git -C "${upstream}" apply --reverse --check \
            "${renderer_dir}"/patches/*.patch 2>/dev/null \
            && echo "(already applied; continuing)" \
            || exit 1
        return
    }
    git -C "${upstream}" apply "${renderer_dir}"/patches/*.patch
}

revert_patches() {
    git -C "${upstream}" apply --reverse --check \
        "${renderer_dir}"/patches/*.patch 2>/dev/null \
        && git -C "${upstream}" apply --reverse \
            "${renderer_dir}"/patches/*.patch \
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
    -DTDESKTOP_RENDERER_HARNESS_DIR="${renderer_dir}/cpp"

cmake --build "${build_dir}" --target ttr_native_host "$@"

echo "native host: ${build_dir}/ttr_native_host"
