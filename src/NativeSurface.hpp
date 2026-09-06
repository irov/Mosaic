#pragma once

#include "Context.hpp"

namespace Mosaic::Detail
{
    void syncNativeSurfaces(Context * ui);
    void destroyNativeSurfaces(Context * ui) noexcept;
}
