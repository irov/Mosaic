#pragma once

#include <cstddef>
#include <cstdint>

namespace Mosaic
{
    namespace Detail
    {
        extern const uint32_t DefaultFontSize;
        extern const uint32_t DefaultFontPackedSize;
        extern const uint8_t DefaultFontPacked[];
        extern const uint32_t DefaultFontLicenseSize;
        extern const uint8_t DefaultFontLicense[];
    }

    // Size of the font compiled into the library, in bytes.
    [[nodiscard]] uint32_t defaultFontSize() noexcept;

    // Unpacks that font into a buffer the caller owns, so a host font provider can keep
    // it in whatever container its rasteriser expects instead of shipping a font of its own.
    [[nodiscard]] bool defaultFontData(void * _buffer, size_t _capacity);

    // Licence of the compiled in font. Redistributing the font means redistributing this
    // text with it, so a host that uses defaultFontData has to ship it too.
    [[nodiscard]] uint32_t defaultFontLicenseSize() noexcept;
    [[nodiscard]] const char * defaultFontLicense() noexcept;
}
