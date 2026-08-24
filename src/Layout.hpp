#pragma once

#include "Context.hpp"

namespace Mosaic
{
    namespace Detail
    {
        [[nodiscard]] bool isContainer(NodeKind kind) noexcept;
        [[nodiscard]] bool clipsDescendants(NodeKind kind) noexcept;
        [[nodiscard]] bool isFloatingRootChild(NodeKind kind) noexcept;
        [[nodiscard]] bool isHorizontal(NodeKind kind, Orientation orientation) noexcept;
        [[nodiscard]] bool scrollsContent(const Context::Node & node) noexcept;
        [[nodiscard]] bool hasInsets(const EdgeInsets & insets) noexcept;
        [[nodiscard]] Rect inset(const Rect & rectangle, const EdgeInsets & value) noexcept;
        [[nodiscard]] float scrollbarScrollAtPointer(const Rect & track, const Rect & thumb, float scrollExtent, bool vertical, float dragOffset, const Vec2 & position) noexcept;
    } // namespace Detail
} // namespace Mosaic
