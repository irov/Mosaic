#pragma once

#include "Mosaic/Types.hpp"

namespace Mosaic
{
    class Serializer
    {
    public:
        virtual ~Serializer() = default;
        virtual void begin(uint32_t version) = 0;
        virtual void write(StringView section, Id id, StringView key, StringView value) = 0;
        virtual void end() = 0;
    };

    class Deserializer
    {
    public:
        virtual ~Deserializer() = default;
        [[nodiscard]] virtual uint32_t version() const noexcept = 0;
        virtual void forEach(void (*visitor)(StringView section, Id id, StringView key, StringView value, void * data), void * data) const = 0;
    };
} // namespace Mosaic
