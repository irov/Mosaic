#pragma once

#include "Context.hpp"

namespace Mosaic
{
    namespace Detail
    {
        using TextContextLabels = Array<StringView, 7>;

        [[nodiscard]] size_t previousUtf8(StringView value, size_t position) noexcept;
        [[nodiscard]] size_t nextUtf8(StringView value, size_t position) noexcept;
        [[nodiscard]] size_t previousTextPosition(const Context::Node & node, StringView value, size_t position, bool password) noexcept;
        [[nodiscard]] size_t nextTextPosition(const Context::Node & node, StringView value, size_t position, bool password) noexcept;
        [[nodiscard]] String inputDisplayText(StringView value, bool password);
        [[nodiscard]] size_t inputDisplayOffset(StringView value, size_t position, bool password) noexcept;
        void selectWord(StringView value, size_t position, size_t & cursor, size_t & anchor) noexcept;
        [[nodiscard]] size_t previousWord(StringView value, size_t position) noexcept;
        [[nodiscard]] size_t nextWord(StringView value, size_t position) noexcept;
        [[nodiscard]] size_t moveVertical(const Context::Node & node, StringView value, size_t position, int lines) noexcept;
        void selectLine(const Context::Node & node, StringView value, size_t position, size_t & cursor, size_t & anchor) noexcept;
        [[nodiscard]] size_t visualLineBoundary(const Context::Node & node, StringView value, size_t position, bool end) noexcept;
        [[nodiscard]] size_t inputPositionAtPointer(Context * ui, const Context::Node & node, StringView value, bool password, const Vec2 & pointer);
        void updateTextScroll(Context::Node & node, TextEditorState & state, const Rect & bounds, bool focused);
        void autoScrollTextSelection(const Context::Node & node, TextEditorState & state, const Rect & bounds, const PointerState & pointer, float deltaTime);
        [[nodiscard]] bool numericText(StringView value) noexcept;
    } // namespace Detail
} // namespace Mosaic
