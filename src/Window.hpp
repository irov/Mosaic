#pragma once

#include "Context.hpp"

namespace Mosaic
{
    namespace Detail
    {
        using DockTargetRectArray = Array<Rect, 5>;

        enum WindowResizeEdge : uint8_t
        {
            WindowResizeNone = 0,
            WindowResizeLeft = 1U << 0U,
            WindowResizeRight = 1U << 1U,
            WindowResizeTop = 1U << 2U,
            WindowResizeBottom = 1U << 3U
        };

        [[nodiscard]] Rect constrainWindowBounds(const Rect & bounds, const Rect & available) noexcept;
        [[nodiscard]] DockTargetRectArray dockTargetRects(const Rect & bounds) noexcept;
        [[nodiscard]] bool dockPlacementAt(const DockTargetRectArray & targets, const Vec2 & position, DockPlacement * const _out) noexcept;
        [[nodiscard]] Rect dockPreviewBounds(const Rect & bounds, DockPlacement placement, float ratio = 0.5f) noexcept;
        [[nodiscard]] Rect windowTitleBarBounds(const Context::Node & window, const Rect & bounds) noexcept;
        [[nodiscard]] bool dockTabBounds(const Context * ui, const Context::Node & window, const DockNode & dockNode, size_t tabIndex, const Rect & titleBar, Rect * const _out) noexcept;
        [[nodiscard]] uint8_t windowResizeEdgesAt(const Rect & bounds, const Vec2 & position, float thickness) noexcept;
        [[nodiscard]] Rect resizedWindowBounds(const Rect & start, uint8_t edges, const Vec2 & delta, const Vec2 & minimum, const Vec2 & maximum, const Rect & available) noexcept;
    } // namespace Detail
} // namespace Mosaic
