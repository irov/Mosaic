#include "Mosaic/DefaultFont.hpp"

#include "zlib.h"

namespace Mosaic
{
    uint32_t defaultFontSize() noexcept
    {
        return Detail::DefaultFontSize;
    }

    bool defaultFontData(void * _buffer, size_t _capacity)
    {
        if(_buffer == nullptr)
        {
            return false;
        }

        if(_capacity < Detail::DefaultFontSize)
        {
            return false;
        }

        z_stream stream = {};
        stream.next_in = const_cast<Bytef *>(static_cast<const Bytef *>(Detail::DefaultFontPacked));
        stream.avail_in = static_cast<uInt>(Detail::DefaultFontPackedSize);
        stream.next_out = static_cast<Bytef *>(_buffer);
        stream.avail_out = static_cast<uInt>(Detail::DefaultFontSize);

        // the font is packed by CMake, which only emits gzip, so the window has to be
        // widened for zlib to accept the gzip header rather than a bare deflate stream
        if(inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
        {
            return false;
        }

        int result = inflate(&stream, Z_FINISH);

        inflateEnd(&stream);

        if(result != Z_STREAM_END || stream.total_out != Detail::DefaultFontSize)
        {
            return false;
        }

        return true;
    }

    uint32_t defaultFontLicenseSize() noexcept
    {
        return Detail::DefaultFontLicenseSize;
    }

    const char * defaultFontLicense() noexcept
    {
        return reinterpret_cast<const char *>(Detail::DefaultFontLicense);
    }
}
