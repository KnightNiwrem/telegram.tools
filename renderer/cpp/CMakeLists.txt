# Part of the telegram.tools rich-message renderer (GPL-3.0 with the
# OpenSSL exception, see renderer/LICENSE).
#
# This directory is added from the pinned TDesktop tree's
# Telegram/CMakeLists.txt via the patch stack in renderer/patches (option
# TDESKTOP_RENDERER_HARNESS). It cannot be configured standalone: the
# targets link against tdesktop::td_iv / tdesktop::td_ui and the
# desktop-app libraries defined by that tree. See scripts/build-native.sh.

# src_loc / res_loc are set by Telegram/CMakeLists.txt before this
# directory is added.
set(ttr_dir ${CMAKE_CURRENT_SOURCE_DIR})

add_library(ttr_core OBJECT)
init_target(ttr_core)

target_sources(ttr_core PRIVATE
    ${ttr_dir}/adapters/canonical_page.cpp
    ${ttr_dir}/adapters/canonical_page.h
    ${ttr_dir}/core/harness_app.cpp
    ${ttr_dir}/core/harness_app.h
    ${ttr_dir}/core/link_stubs.cpp
    ${ttr_dir}/core/render_service.cpp
    ${ttr_dir}/core/render_service.h
    ${ttr_dir}/core/sessionless_media_runtime.cpp
    ${ttr_dir}/core/sessionless_media_runtime.h
    ${ttr_dir}/core/version.h
    # Compiled from the pinned tree: plain data type used by map blocks
    # (its click-handler half is stubbed in core/link_stubs.cpp).
    ${src_loc}/data/data_location.cpp
    ${src_loc}/data/data_location.h
)

target_include_directories(ttr_core PUBLIC ${ttr_dir} ${src_loc})

target_link_libraries(ttr_core
PUBLIC
    tdesktop::td_iv
    tdesktop::td_ui
    tdesktop::td_lang
    desktop-app::lib_ui
    desktop-app::lib_base
    desktop-app::lib_spellcheck
    desktop-app::external_qt
)

if (EMSCRIPTEN)
    add_executable(ttr_wasm_host ${ttr_dir}/wasm-host/ttr_abi.cpp
        ${ttr_dir}/wasm-host/ttr_abi.h)
    init_target(ttr_wasm_host)
    target_link_libraries(ttr_wasm_host PRIVATE ttr_core)
    target_link_options(ttr_wasm_host PRIVATE
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createTtrModule
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_RUNTIME_METHODS=cwrap,UTF8ToString
        -sEXPORTED_FUNCTIONS=_malloc,_free
    )
    set_target_properties(ttr_wasm_host PROPERTIES OUTPUT_NAME ttr-renderer)
else()
    add_executable(ttr_native_host ${ttr_dir}/native-host/main.cpp)
    init_target(ttr_native_host)
    target_link_libraries(ttr_native_host PRIVATE ttr_core)

    # Emoji sprite resources required by Ui::Emoji::Init(), mirrored from
    # Telegram/cmake/tests.cmake (test_text target).
    nice_target_sources(ttr_native_host ${res_loc}
    PRIVATE
        qrc/emoji_1.qrc
        qrc/emoji_2.qrc
        qrc/emoji_3.qrc
        qrc/emoji_4.qrc
        qrc/emoji_5.qrc
        qrc/emoji_6.qrc
        qrc/emoji_7.qrc
        qrc/emoji_8.qrc
    )
    set_target_properties(ttr_native_host
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    target_prepare_qrc(ttr_native_host)
endif()
