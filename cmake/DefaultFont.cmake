# Downloads the default font and packs it into a generated source file. The font is
# never stored in the repository: it is fetched once into the build tree, compressed
# with the gzip support built into CMake and emitted as a byte array.

function(mosaic_pack_default_font url sha256 output_source output_variable)
    get_filename_component(font_name "${url}" NAME)

    set(font_dir "${CMAKE_CURRENT_BINARY_DIR}/DefaultFont")
    set(font_file "${font_dir}/${font_name}")
    set(font_packed "${font_dir}/${font_name}.gz")

    file(MAKE_DIRECTORY "${font_dir}")

    if(NOT EXISTS "${font_file}")
        message(STATUS "Mosaic downloading default font ${font_name}")

        file(DOWNLOAD "${url}" "${font_file}"
            EXPECTED_HASH SHA256=${sha256}
            TLS_VERIFY ON
            STATUS font_download_status
        )

        list(GET font_download_status 0 font_download_code)

        if(NOT font_download_code EQUAL 0)
            list(GET font_download_status 1 font_download_message)
            file(REMOVE "${font_file}")
            message(FATAL_ERROR "Mosaic could not download the default font from ${url}: ${font_download_message}")
        endif()
    endif()

    file(SIZE "${font_file}" font_size)

    file(ARCHIVE_CREATE
        OUTPUT "${font_packed}"
        PATHS "${font_file}"
        FORMAT raw
        COMPRESSION GZip
        COMPRESSION_LEVEL 9
    )

    file(SIZE "${font_packed}" font_packed_size)
    file(READ "${font_packed}" font_hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," font_bytes "${font_hex}")
    string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){16})" "\\1\n            " font_bytes "${font_bytes}")

    set(font_source "#include \"Mosaic/DefaultFont.hpp\"

namespace Mosaic
{
    namespace Detail
    {
        const uint32_t DefaultFontSize = ${font_size}U;
        const uint32_t DefaultFontPackedSize = ${font_packed_size}U;

        const uint8_t DefaultFontPacked[${font_packed_size}] =
        {
            ${font_bytes}
        };
    }
}
")

    if(EXISTS "${output_source}")
        file(READ "${output_source}" font_previous)
    else()
        set(font_previous "")
    endif()

    if(NOT font_previous STREQUAL font_source)
        file(WRITE "${output_source}" "${font_source}")
    endif()

    message(STATUS "Mosaic default font ${font_name}: ${font_size} bytes packed to ${font_packed_size}")

    set(${output_variable} "${output_source}" PARENT_SCOPE)
endfunction()
