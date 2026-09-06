#pragma once

#include "Context.hpp"
#include "DrawList.hpp"

namespace Mosaic
{
    namespace Detail
    {
        struct SliderGeometry
        {
            Rect control;
            Rect track;
            Rect label;
            float maximumGrabSide = 16.f;
        };

        struct ComboGeometry
        {
            Rect control;
            Rect preview;
            Rect arrow;
            Rect label;
        };

        struct ColorEditGeometry
        {
            Rect control;
            Rect label;
            Rect preview;
            Array<Rect, 4> channels;
            uint8_t channelCount = 0;
        };

        [[nodiscard]] const char * nodeKindName(NodeKind kind) noexcept;
        [[nodiscard]] bool usesAncestorVisualClip(NodeKind kind) noexcept;
        [[nodiscard]] float snapToPixel(float value, float scale) noexcept;
        [[nodiscard]] Color mixColor(const Color & from, const Color & to, float amount) noexcept;
        [[nodiscard]] Color colorWithAlpha(const Color & color, float alpha) noexcept;
        [[nodiscard]] Vec2 mixVector(const Vec2 & from, const Vec2 & to, float amount) noexcept;
        [[nodiscard]] float itemLabelWidth(const Context::Node & node) noexcept;
        [[nodiscard]] SliderGeometry sliderGeometry(const Context::Node & node, const Rect & bounds) noexcept;
        [[nodiscard]] ComboGeometry comboGeometry(const Context::Node & node, const Rect & bounds) noexcept;
        [[nodiscard]] ColorEditGeometry colorEditGeometry(const Context::Node & node, const Rect & bounds) noexcept;
        void updateVisualState(Context::Persistent & state, const Context::Node & node, float deltaTime) noexcept;
        void drawFrame(DrawList & drawList, const Rect & bounds, float radius, float borderWidth, const Color & fill, const Color & border, uint64_t renderKey);
        void drawScrollbar(DrawList & drawList, const Rect & track, const Rect & thumb, bool vertical, const Theme & style, float hover, float active, uint64_t renderKey);
        [[nodiscard]] Vec2 textCursorPosition(Context * ui, const Context::Node & node, size_t position);
        void drawTextSelection(Context * ui, DrawList & drawList, const Context::Node & node, const Color & color, uint64_t renderKey);
    } // namespace Detail
} // namespace Mosaic
