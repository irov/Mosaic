#pragma once

#include "DrawCommand.hpp"

namespace Mosaic
{
    class DrawList final
    {
    public:
        void setStorage(DrawCommandStorage * storage) noexcept;
        void clear() noexcept;
        void rect(const Rect & bounds, const Color & color, uint64_t renderKey);
        void rects(RectInstanceSpan instances, uint64_t renderKey);
        void quads(QuadInstanceSpan instances, uint64_t renderKey);
        void box(const Rect & bounds, const BoxStyle & style, uint64_t renderKey);
        void roundedRect(const Rect & bounds, float radius, const Color & color, uint64_t renderKey);
        void roundedImage(const Rect & bounds, const Rect & uv, float radius, const Color & tint, uint64_t renderKey);
        void line(const Vec2 & first, const Vec2 & second, float thickness, const Color & color, uint64_t renderKey);
        void polyline(Vec2Span points, float thickness, const Color & color, bool closed, uint64_t renderKey);
        void quadraticBezier(const Vec2 & first, const Vec2 & control, const Vec2 & second, float thickness, const Color & color, uint64_t renderKey);
        void image(const Rect & bounds, const Rect & uv, const Color & tint, uint64_t renderKey);
        void custom(VertexSpan vertices, IndexSpan indices, uint64_t renderKey);
        void textGeometry(uint64_t cacheKey, TexturedRectInstanceSpan rectangles, const Vec2 & translation, const Color & tint, uint64_t renderKey, const Vec2 & axisX = {1.f, 0.f}, const Vec2 & axisY = {0.f, 1.f});
        void pushClip(const Rect & clip, uint64_t renderKey);
        void popClip(uint64_t renderKey);
        void setAlphaMultiplier(float alpha) noexcept;
        [[nodiscard]] float alphaMultiplier() const noexcept;
        [[nodiscard]] const DrawCommandVector & commands() const noexcept;
        [[nodiscard]] DrawCommandVector & commands() noexcept;

    private:
        DrawCommandVector m_commands;
        DrawCommandStorage * m_storage = nullptr;
        float m_alphaMultiplier = 1.f;
    };
} // namespace Mosaic
