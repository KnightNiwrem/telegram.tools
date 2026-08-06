# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Qt for WebAssembly ships no WebP image plugin, so TDesktop's emoji
# sprites (Resources/emoji/emoji_N.webp) decode to null images and
# Ui::Emoji::Init() aborts on an empty sprite list. This regenerates the
# sprites as PNG at build time and emits a .qrc that maps them onto the
# same ":/gui/emoji/emoji_N.webp" resource paths the loader asks for, so
# no upstream code changes and the pixels are identical (WebP -> PNG of
# the same lossless raster).

find_program(TTR_DWEBP dwebp)
if (NOT TTR_DWEBP)
    message(FATAL_ERROR
        "dwebp not found; install the 'webp' package so the emoji sprites "
        "can be converted for Qt-wasm (no WebP plugin there).")
endif()

set(ttr_emoji_dir ${CMAKE_CURRENT_BINARY_DIR}/emoji-png)
set(ttr_emoji_qrc ${ttr_emoji_dir}/emoji_png.qrc)
file(MAKE_DIRECTORY ${ttr_emoji_dir})

set(ttr_emoji_entries "")
set(ttr_emoji_outputs "")
file(GLOB ttr_emoji_sources ${res_loc}/emoji/emoji_*.webp)
foreach (source ${ttr_emoji_sources})
    get_filename_component(name ${source} NAME_WLE)
    set(output ${ttr_emoji_dir}/${name}.png)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${TTR_DWEBP} -quiet ${source} -o ${output}
        DEPENDS ${source}
        COMMENT "Converting ${name}.webp for Qt-wasm")
    list(APPEND ttr_emoji_outputs ${output})
    string(APPEND ttr_emoji_entries
        "    <file alias=\"emoji/${name}.webp\">${output}</file>\n")
endforeach()

file(WRITE ${ttr_emoji_qrc}
    "<RCC>\n  <qresource prefix=\"/gui\">\n${ttr_emoji_entries}  </qresource>\n</RCC>\n")

add_custom_target(ttr_emoji_png DEPENDS ${ttr_emoji_outputs})
