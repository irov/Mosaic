#pragma once

#include "Mosaic/Types.hpp"

namespace Mosaic
{
    class DockModel;
    struct Context;
    struct DockSpaceOptions;

    namespace Detail
    {
        void closeScope(Context * ui, uint64_t token) noexcept;
        void finishTabBar(Context * ui, Id tabBar) noexcept;
        [[nodiscard]] DockModel * dockModel(Context * ui, uint32_t group, bool create) noexcept;
        [[nodiscard]] const DockModel * dockModel(const Context * ui, uint32_t group) noexcept;
        [[nodiscard]] const DockSpaceOptions * dockSpaceOptions(const Context * ui, uint32_t group) noexcept;
        void setDockArea(Context * ui, uint32_t group, const Rect & bounds) noexcept;
        [[nodiscard]] bool dockArea(const Context * ui, uint32_t group, Rect * const _out) noexcept;
        [[nodiscard]] size_t textLogTreeDepth(const Context * ui, size_t node, size_t root) noexcept;
    } // namespace Detail
} // namespace Mosaic
