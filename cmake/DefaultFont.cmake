# Downloads the default font and packs it into a generated source file. The font is
# never stored in the repository: it is fetched once into the build tree, compressed
# with the gzip support built into CMake and emitted as a byte array.
#
# Which font that is lives here, so that every project embedding Mosaic packs the same
# one without restating the url and the hash.

set(MOSAIC_DEFAULT_FONT_URL "https://raw.githubusercontent.com/googlefonts/roboto-2/main/src/hinted/Roboto-Medium.ttf"
    CACHE STRING "Font downloaded and packed into the library as the default font")
set(MOSAIC_DEFAULT_FONT_SHA256 "2879a5ecb7fbfa13a7fc3e2cdd7fecbf73aa45e91b541dfdfa2c442eed0aac21"
    CACHE STRING "Expected SHA256 of the downloaded default font")
set(MOSAIC_DEFAULT_FONT_LICENSE_URL "https://raw.githubusercontent.com/googlefonts/roboto-2/main/LICENSE"
    CACHE STRING "Licence downloaded and packed into the library alongside the default font")

function(mosaic_pack_default_font output_source output_variable)
    set(url "${MOSAIC_DEFAULT_FONT_URL}")
    set(sha256 "${MOSAIC_DEFAULT_FONT_SHA256}")
    set(license_url "${MOSAIC_DEFAULT_FONT_LICENSE_URL}")

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

    set(license_file "${font_dir}/LICENSE")

    if(NOT EXISTS "${license_file}")
        message(STATUS "Mosaic downloading default font licence")

        file(DOWNLOAD "${license_url}" "${license_file}"
            TLS_VERIFY ON
            STATUS license_download_status
        )

        list(GET license_download_status 0 license_download_code)

        if(NOT license_download_code EQUAL 0)
            list(GET license_download_status 1 license_download_message)
            file(REMOVE "${license_file}")
            message(FATAL_ERROR "Mosaic could not download the default font licence from ${license_url}: ${license_download_message}")
        endif()
    endif()

    file(READ "${license_file}" license_hex HEX)
    string(LENGTH "${license_hex}" license_hex_length)
    math(EXPR license_size "${license_hex_length} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," license_bytes "${license_hex}")
    string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){16})" "\\1\n            " license_bytes "${license_bytes}")

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

        const uint32_t DefaultFontLicenseSize = ${license_size}U;

        const uint8_t DefaultFontLicense[${license_size}] =
        {
            ${license_bytes}
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

    message(STATUS "Mosaic default font ${font_name}: ${font_size} bytes packed to ${font_packed_size}, licence ${license_size} bytes")

    set(${output_variable} "${output_source}" PARENT_SCOPE)
endfunction()
