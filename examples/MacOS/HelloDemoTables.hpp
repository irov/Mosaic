#pragma once

#include "Mosaic/Mosaic.hpp"

namespace MosaicExample
{
    namespace Detail
    {
        [[nodiscard]] bool drawReferenceTableDemo(Mosaic::Context * ui, Mosaic::StringView label, const Mosaic::TableOptions & baseOptions);
    } // namespace Detail
} // namespace MosaicExample
