#pragma once

#include "Context.hpp"

namespace Mosaic
{
    namespace Detail
    {
        [[nodiscard]] Context::PopupState * findPopup(Context * ui, Id id) noexcept;
        [[nodiscard]] const Context::PopupState * findPopup(const Context * ui, Id id) noexcept;
        void closePopupById(Context * ui, Id id) noexcept;
        [[nodiscard]] Id popupId(const Context * ui, const Key & key, Id owner = InvalidId) noexcept;
        [[nodiscard]] bool popupOwnerCanInteract(const Context * ui, Id node) noexcept;
        [[nodiscard]] Rect placePopup(const Context::Node & node, const Vec2 & size, const Rect & workArea) noexcept;
        void beginPopupFrame(Context * ui);
        void syncPopupBounds(Context * ui);
        void popupBackdrop(Context * ui, StringView label, bool * open, bool dismissOnClick, const Color & tint, const Rect & foregroundBounds, const SourceLocation & location);
        void makePopupOpaque(Context * ui, Context::Node & node);
    } // namespace Detail
} // namespace Mosaic
