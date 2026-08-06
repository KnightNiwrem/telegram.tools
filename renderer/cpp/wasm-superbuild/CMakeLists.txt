# Part of the telegram.tools rich-message renderer (GPL-3.0 with the
# OpenSSL exception, see renderer/LICENSE).
#
# WASM superbuild (plan Phase 3). Telegram Desktop's own build system has
# no Emscripten platform branch, so this top-level project reuses the
# pinned tree's cmake helpers and the upstream td_*.cmake source lists
# verbatim while:
#   - providing wasm-appropriate common_options (no pthread: the first
#     artifact targets Qt wasm_singlethread per the plan);
#   - adding only the externals the renderer subset needs;
#   - substituting impostor INTERFACE targets for libraries the render
#     path needs only headers from (webview, storage, ffmpeg, stripe,
#     lottie) — their symbols resolve to null exactly like the native
#     harness's ignore-unresolved strategy, with the smoke render as the
#     tripwire;
#   - driving TDesktop's code generators with the natively built binaries
#     (cross-compiled codegen cannot run at build time).
#
# Configure (after sourcing emsdk_env.sh):
#   emcmake cmake -S renderer/cpp/wasm-superbuild -B renderer/build/wasm \
#     -DCMAKE_BUILD_TYPE=Release \
#     -DQT_HOST_PATH=$HOME/qt-wasm/6.9.2/gcc_64 \
#     -DCMAKE_PREFIX_PATH=$HOME/qt-wasm/6.9.2/wasm_singlethread \
#     -DTTR_NATIVE_BUILD_DIR=$PWD/renderer/build/native
# See scripts/build-wasm.sh.

cmake_minimum_required(VERSION 3.25...3.31)

project(TtrWasmRenderer LANGUAGES C CXX)

if (NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "This superbuild targets Emscripten only; use scripts/build-native.sh "
        "for the native harness.")
endif()

set(TTR_NATIVE_BUILD_DIR "" CACHE PATH
    "Native build directory providing codegen_style/codegen_lang/codegen_emoji")
if (NOT TTR_NATIVE_BUILD_DIR)
    message(FATAL_ERROR
        "Set -DTTR_NATIVE_BUILD_DIR to a completed native build "
        "(scripts/build-native.sh); the code generators must run on the host.")
endif()

get_filename_component(ttr_renderer_loc ${CMAKE_CURRENT_SOURCE_DIR}/../.. REALPATH)
set(tdesktop_loc ${ttr_renderer_loc}/upstream/tdesktop)

get_filename_component(third_party_loc ${tdesktop_loc}/Telegram/ThirdParty REALPATH)
get_filename_component(submodules_loc ${tdesktop_loc}/Telegram REALPATH)
get_filename_component(cmake_helpers_loc ${tdesktop_loc}/cmake REALPATH)
get_filename_component(src_loc ${submodules_loc}/SourceFiles REALPATH)
get_filename_component(res_loc ${submodules_loc}/Resources REALPATH)

# Options consumed by variables.cmake / the libraries.
set(DESKTOP_APP_USE_PACKAGED ON CACHE BOOL "" FORCE)
set(DESKTOP_APP_DISABLE_CRASH_REPORTS ON CACHE BOOL "" FORCE)
set(DESKTOP_APP_DISABLE_AUTOUPDATE ON CACHE BOOL "" FORCE)
set(DESKTOP_APP_USE_HUNSPELL_ONLY ON CACHE BOOL "" FORCE)

include(${cmake_helpers_loc}/variables.cmake)

# cmake_dependent_option force-enables CLD3 on every non-Apple platform;
# language detection is dead weight in the renderer and would drag
# protobuf into the WASM build. Plain variable wins over the forced value.
set(DESKTOP_APP_USE_CLD3 OFF)
include(${cmake_helpers_loc}/nice_target_sources.cmake)
include(${cmake_helpers_loc}/target_compile_options_if_exists.cmake)
include(${cmake_helpers_loc}/target_link_frameworks.cmake)
include(${cmake_helpers_loc}/target_link_options_if_exists.cmake)
include(${cmake_helpers_loc}/init_target.cmake)
include(${cmake_helpers_loc}/generate_target.cmake)
include(${cmake_helpers_loc}/target_prepare_qrc.cmake)

# --- common_options: the Emscripten platform branch options.cmake lacks.
add_library(common_options INTERFACE)
add_library(desktop-app::common_options ALIAS common_options)

target_compile_definitions(common_options
INTERFACE
    $<$<CONFIG:Debug>:_DEBUG>
    QT_NO_KEYWORDS
    QT_NO_CAST_FROM_BYTEARRAY
    QT_DEPRECATED_WARNINGS_SINCE=0x051500
)

target_compile_options(common_options
INTERFACE
    $<$<NOT:$<CONFIG:Debug>>:-fno-strict-aliasing>
    -pipe
)

target_compile_options_if_exists(common_options
INTERFACE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-switch
    -Wno-missing-field-initializers
    -Wno-sign-compare
    -Wno-deprecated
    -Wno-deprecated-declarations
    -Wno-unqualified-std-cast-call
)

# Provided by cmake/external/CMakeLists.txt in upstream builds.
macro(add_checked_subdirectory name)
    if (NOT DEFINED desktop_app_skip_libs
        OR NOT ${name} IN_LIST desktop_app_skip_libs)
        add_subdirectory(${name})
    endif()
endmacro()

# --- Qt for WebAssembly (static; provides Qt:: targets).
include(${cmake_helpers_loc}/external/qt/package.cmake)
add_subdirectory(${cmake_helpers_loc}/external/qt external/qt)

# --- Externals the renderer subset genuinely needs.
add_subdirectory(${cmake_helpers_loc}/external/gsl external/gsl)
add_subdirectory(${cmake_helpers_loc}/external/expected external/expected)
add_subdirectory(${cmake_helpers_loc}/external/ranges external/ranges)
add_subdirectory(${cmake_helpers_loc}/external/xxhash external/xxhash)
add_subdirectory(${cmake_helpers_loc}/external/cmark_gfm external/cmark_gfm)
add_subdirectory(${cmake_helpers_loc}/external/microtex external/microtex)

# No TooManyCooks under single-threaded WASM (std::jthread/atomic_ref are
# unavailable and a work-stealing pool is meaningless with one thread);
# crl falls back to its Qt-event-loop backend.
set(CRL_FORCE_QT ON)
add_subdirectory(${cmake_helpers_loc}/external/crash_reports external/crash_reports)
add_subdirectory(${cmake_helpers_loc}/external/hunspell external/hunspell)
set(DESKTOP_APP_USE_PACKAGED OFF) # vendored lz4 for wasm, then restore
add_subdirectory(${cmake_helpers_loc}/external/lz4 external/lz4)
set(DESKTOP_APP_USE_PACKAGED ON)

# ada: build the vendored-from-source path is Linux-only cmake; compile the
# release single-header amalgamation directly for wasm.
set(ttr_ada_loc ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ada)
if (EXISTS ${ttr_ada_loc}/ada.cpp)
    add_library(external_ada_bundled STATIC ${ttr_ada_loc}/ada.cpp)
    init_target(external_ada_bundled "(external)")
    target_include_directories(external_ada_bundled SYSTEM PUBLIC ${ttr_ada_loc})
    add_library(external_ada INTERFACE IMPORTED GLOBAL)
    add_library(desktop-app::external_ada ALIAS external_ada)
    target_link_libraries(external_ada INTERFACE external_ada_bundled)
else()
    message(FATAL_ERROR
        "ada singleheader amalgamation missing; run scripts/build-wasm.sh "
        "which fetches it into cpp/wasm-superbuild/third_party/ada")
endif()

# openssl: only base/random.cpp uses it in the renderer subset, and the
# emscripten patch routes that to getentropy. Empty interface.
add_library(external_openssl INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_openssl ALIAS external_openssl)

# Boost.Regex header-only (v5) for libprisma; system Boost headers are
# portable C++, exposed through a clean dir so no other host headers leak
# into the Emscripten include path.
add_library(external_boost_regex INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_boost_regex ALIAS external_boost_regex)
target_compile_definitions(external_boost_regex INTERFACE
    BOOST_NO_INTRINSIC_WCHAR_T
    BOOST_REGEX_NO_W32
    BOOST_REGEX_STANDALONE)
target_include_directories(external_boost_regex SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost-headers)

include(${submodules_loc}/cmake/lib_prisma.cmake)

# --- Impostor interfaces: headers available, implementations deliberately
# absent (never reached by the sessionless render path).
function(ttr_impostor name)
    add_library(${name} INTERFACE IMPORTED GLOBAL)
    add_library(desktop-app::${name} ALIAS ${name})
    foreach (dir ${ARGN})
        target_include_directories(${name} INTERFACE ${dir})
    endforeach()
endfunction()

ttr_impostor(lib_webview ${submodules_loc}/lib_webview)
ttr_impostor(lib_storage ${submodules_loc}/lib_storage)
ttr_impostor(lib_stripe)
ttr_impostor(lib_lottie ${submodules_loc}/lib_lottie)
# lib_ffmpeg's wrapper sources live under SourceFiles/ffmpeg (already an
# include dir of td_ui); the impostor only satisfies the link edge.
ttr_impostor(lib_ffmpeg)

# zlib/libjpeg via the Emscripten ports: the flag makes emcc fetch and
# link them.
add_library(external_zlib INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_zlib ALIAS external_zlib)
target_compile_options(external_zlib INTERFACE -sUSE_ZLIB=1)
target_link_options(external_zlib INTERFACE -sUSE_ZLIB=1)

add_library(external_jpeg INTERFACE IMPORTED GLOBAL)
add_library(desktop-app::external_jpeg ALIAS external_jpeg)
target_compile_options(external_jpeg INTERFACE -sUSE_LIBJPEG=1)
target_link_options(external_jpeg INTERFACE -sUSE_LIBJPEG=1)
ttr_impostor(external_kcoreaddons
    ${third_party_loc}/kcoreaddons/src/lib/io
    ${third_party_loc}/kcoreaddons/src/lib/util
    ${cmake_helpers_loc}/external/kcoreaddons/headers/public)
target_compile_definitions(external_kcoreaddons INTERFACE KCOREADDONS_EXPORT=)
ttr_impostor(external_webrtc)
ttr_impostor(external_rlottie)

# tdesktop:: aliases used by td_*.cmake for the impostors.
add_library(tdesktop_lib_tgcalls INTERFACE IMPORTED GLOBAL)
add_library(tdesktop::lib_tgcalls ALIAS tdesktop_lib_tgcalls)

# --- Host code generators from the native build (cannot cross-run).
foreach (generator style lang emoji)
    set(binary ${TTR_NATIVE_BUILD_DIR}/Telegram/codegen/codegen/${generator}/codegen_${generator})
    if (NOT EXISTS ${binary})
        message(FATAL_ERROR "missing host generator: ${binary}")
    endif()
    add_executable(codegen_${generator} IMPORTED GLOBAL)
    set_target_properties(codegen_${generator}
        PROPERTIES IMPORTED_LOCATION ${binary})
endforeach()

# --- desktop-app libraries, unmodified upstream CMakeLists.
add_subdirectory(${submodules_loc}/lib_rpl lib_rpl)
add_subdirectory(${submodules_loc}/lib_crl lib_crl)
add_subdirectory(${submodules_loc}/lib_base lib_base)
add_subdirectory(${submodules_loc}/lib_ui lib_ui)
add_subdirectory(${submodules_loc}/lib_spellcheck lib_spellcheck)
add_subdirectory(${submodules_loc}/lib_tl lib_tl)

# --- tdesktop targets via the upstream cmake source lists.
set(TDESKTOP_RENDERER_HARNESS_DIR ${ttr_renderer_loc}/cpp)

# td_*.cmake use includes relative to the Telegram/ source dir.
list(APPEND CMAKE_MODULE_PATH ${submodules_loc})

include(${submodules_loc}/cmake/generate_lang.cmake)
include(${submodules_loc}/cmake/generate_scheme.cmake)
include(${submodules_loc}/cmake/td_lang.cmake)
include(${submodules_loc}/cmake/td_scheme.cmake)
include(${submodules_loc}/cmake/td_ui.cmake)
include(${submodules_loc}/cmake/td_iv.cmake)

# --- The renderer itself (defines ttr_core + ttr_wasm_host under
# EMSCRIPTEN).
add_subdirectory(${ttr_renderer_loc}/cpp ttr-renderer)
