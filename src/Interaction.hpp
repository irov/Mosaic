#pragma once

#include "Context.hpp"

namespace Mosaic
{
    namespace Detail
    {
        [[nodiscard]] bool inputLayerBlocked(const Context * ui, const Context::Node & node) noexcept;
        [[nodiscard]] Response itemBehavior(Context * ui, size_t index, const ItemBehaviorOptions & options, const Rect * interactionBounds = nullptr);
        void updateNavigation(Context * ui);
        void updateInputCapture(Context * ui) noexcept;
        [[nodiscard]] bool shortcutTriggered(Context * ui, size_t node, const Shortcut & shortcut, const ShortcutOptions & options) noexcept;
    } // namespace Detail
} // namespace Mosaic
