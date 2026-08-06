#!/usr/bin/env bash
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Prepares an Ubuntu host (24.04+, GitHub Actions runners included) to
# configure and build the native renderer harness. This captures, as a
# script, the manual machine setup recorded in BUILD-STATUS.md:
#
#   - the apt dependency set (TDesktop's Linux deps minus Qt),
#   - ada (v2.9.2) and rnnoise (v0.2) built from source — Ubuntu ships
#     no packages for either,
#   - shim CMake package configs for protobuf / tg_owt / tde2e. The
#     shims satisfy *configure only*: they cover targets the renderer
#     harness never builds or links. The Telegram application itself
#     cannot be built against them.
#
# Qt is intentionally NOT installed here. On CI, Qt 6.9.2 (host +
# WebAssembly) comes from aqtinstall so it matches the pinned Qt-wasm
# version; pass --distro-qt to install Ubuntu's Qt 6 dev packages
# instead for a developer machine whose distro Qt is new enough.
#
# Usage: setup-ci-host.sh [--distro-qt]
# Env:
#   TTR_PREFIX     install prefix for ada/rnnoise/shims (default /usr/local)
#   TTR_SKIP_APT   set to 1 to skip the apt-get step
set -euo pipefail

prefix="${TTR_PREFIX:-/usr/local}"
sudo=""
[[ ${EUID} -ne 0 ]] && sudo="sudo"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

# --- apt dependencies -------------------------------------------------------
if [[ "${TTR_SKIP_APT:-0}" != "1" ]]; then
    ${sudo} apt-get update
    ${sudo} apt-get install -y \
        g++ cmake ninja-build ccache pkg-config git curl wget unzip python3 \
        autoconf automake libtool \
        libssl-dev zlib1g-dev libopus-dev \
        libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
        libswresample-dev libavfilter-dev libavdevice-dev libopenal-dev \
        liblz4-dev libxxhash-dev libminizip-dev \
        protobuf-compiler libprotobuf-dev libprotoc-dev libabsl-dev \
        libboost-dev libboost-regex-dev libboost-program-options-dev \
        libjpeg-dev libwebp-dev libpng-dev libtiff-dev liblzma-dev \
        webp \
        libudev-dev \
        libhunspell-dev libgtk-3-dev libgirepository1.0-dev libepoxy-dev \
        libglibmm-2.68-dev libfontconfig-dev \
        libxcb1-dev libxcb-keysyms1-dev libxcb-record0-dev \
        libxcb-screensaver0-dev libx11-xcb-dev libxcomposite-dev \
        libxdamage-dev libxext-dev libxfixes-dev libxrender-dev \
        libxrandr-dev libxtst-dev
    if [[ "${1:-}" == "--distro-qt" ]]; then
        ${sudo} apt-get install -y \
            qt6-base-dev qt6-base-private-dev qt6-svg-dev \
            qt6-image-formats-plugins qt6-shadertools-dev
    fi
fi

# --- ada v2.9.2 (URL parser; no Ubuntu package) -----------------------------
if [[ ! -f "${prefix}/lib/cmake/ada/ada-config.cmake" ]]; then
    echo "building ada v2.9.2 into ${prefix}..."
    git clone --depth 1 --branch v2.9.2 \
        https://github.com/ada-url/ada "${work}/ada"
    cmake -S "${work}/ada" -B "${work}/ada/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${prefix}" \
        -DADA_TESTING=OFF -DADA_TOOLS=OFF -DADA_BENCHMARKS=OFF
    cmake --build "${work}/ada/build"
    ${sudo} cmake --install "${work}/ada/build"
else
    echo "ada: already installed at ${prefix}"
fi

# --- rnnoise v0.2 (noise suppression; no Ubuntu package) --------------------
# Only lib_webrtc's headers reference it in the harness build; the
# library itself is never linked by the renderer, but configure and the
# spellcheck/webrtc target compiles need it present.
if [[ ! -f "${prefix}/lib/pkgconfig/rnnoise.pc" ]]; then
    echo "building rnnoise v0.2 into ${prefix}..."
    git clone --depth 1 --branch v0.2 \
        https://github.com/xiph/rnnoise "${work}/rnnoise"
    (
        cd "${work}/rnnoise"
        ./autogen.sh   # also fetches the model data
        ./configure --prefix="${prefix}"
        make -j"$(nproc)"
        ${sudo} make install
    )
else
    echo "rnnoise: already installed at ${prefix}"
fi

# --- shim CMake package configs ---------------------------------------------
# Ubuntu's protobuf ships no CMake config files; tg_owt/tde2e are the
# calls/encryption stacks the harness never builds or links. Byte-for-byte
# the shims used on the original dev host (see BUILD-STATUS.md).
shims="${work}/shims"
mkdir -p "${shims}"/{protobuf,tg_owt,tde2e}

cat > "${shims}/protobuf/protobufConfig.cmake" <<'EOF'
# Shim: Ubuntu's protobuf 3.21 packages ship no CMake package config.
# Provides the imported targets tdesktop's cld3 external expects.
if(NOT TARGET protobuf::protoc)
  add_executable(protobuf::protoc IMPORTED)
  set_target_properties(protobuf::protoc PROPERTIES
    IMPORTED_LOCATION /usr/bin/protoc)
endif()
if(NOT TARGET protobuf::libprotobuf-lite)
  add_library(protobuf::libprotobuf-lite SHARED IMPORTED)
  set_target_properties(protobuf::libprotobuf-lite PROPERTIES
    IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libprotobuf-lite.so
    INTERFACE_INCLUDE_DIRECTORIES /usr/include)
endif()
if(NOT TARGET protobuf::libprotobuf)
  add_library(protobuf::libprotobuf SHARED IMPORTED)
  set_target_properties(protobuf::libprotobuf PROPERTIES
    IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libprotobuf.so
    INTERFACE_INCLUDE_DIRECTORIES /usr/include)
endif()
set(protobuf_FOUND TRUE)
EOF

cat > "${shims}/tg_owt/tg_owtConfig.cmake" <<'EOF'
# Dev-host shim: satisfies configure for WebRTC targets the renderer
# harness never builds or links. Building the Telegram app against this
# shim is NOT possible.
if(NOT TARGET tg_owt::tg_owt)
  add_library(tg_owt::tg_owt INTERFACE IMPORTED)
endif()
set(tg_owt_FOUND TRUE)
EOF

cat > "${shims}/tde2e/tde2eConfig.cmake" <<'EOF'
# Dev-host shim: satisfies configure for call-encryption targets the
# renderer harness never builds or links. Building the Telegram app
# against this shim is NOT possible.
if(NOT TARGET tde2e::tde2e)
  add_library(tde2e::tde2e INTERFACE IMPORTED)
endif()
set(tde2e_FOUND TRUE)
EOF

${sudo} mkdir -p "${prefix}/lib/cmake"
${sudo} cp -r "${shims}/protobuf" "${shims}/tg_owt" "${shims}/tde2e" \
    "${prefix}/lib/cmake/"
echo "shim package configs installed under ${prefix}/lib/cmake"

echo "host setup complete (prefix: ${prefix})"
