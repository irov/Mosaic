#pragma once

#include "Mosaic/Scope.hpp"
#include "Mosaic/DrawTypes.hpp"

namespace Mosaic
{
    enum class CanvasLayer : uint8_t
    {
        Local,
        Background,
        Foreground
    };

    class Canvas final : public Scope
    {
    public:
        using Scope::Scope;
        [[nodiscard]] bool contentRect(Rect * const _out) const noexcept;
        [[nodiscard]] bool clipRect(Rect * const _out) const noexcept;
        [[nodiscard]] bool localPointerPosition(Vec2 * const _out) const noexcept;
        [[nodiscard]] bool focused() const noexcept;
        void setChannel(uint32_t channel) noexcept;
        void setLayer(CanvasLayer layer) noexcept;

        void rect(const Rect & bounds, const Color & color);
        void rects(RectInstanceSpan instances);
        void quads(QuadInstanceSpan instances);
        void box(const Rect & bounds, const BoxStyle & style);
        void roundedRect(const Rect & bounds, float radius, const Color & color);
        void gradient(const Rect & bounds, const Color & topLeft, const Color & topRight, const Color & bottomRight, const Color & bottomLeft);
        void line(const Vec2 & first, const Vec2 & second, float thickness, const Color & color);
        void polyline(Vec2Span points, float thickness, const Color & color, bool closed = false);
        void circle(const Vec2 & center, float radius, float thickness, const Color & color, uint32_t segments = 0);
        void circleFilled(const Vec2 & center, float radius, const Color & color, uint32_t segments = 0);
        void ellipse(const Vec2 & center, const Vec2 & radii, float rotation, float thickness, const Color & color, uint32_t segments = 0);
        void ellipseFilled(const Vec2 & center, const Vec2 & radii, float rotation, const Color & color, uint32_t segments = 0);
        void regularPolygon(const Vec2 & center, float radius, uint32_t sideCount, float rotation, float thickness, const Color & color);
        void regularPolygonFilled(const Vec2 & center, float radius, uint32_t sideCount, float rotation, const Color & color);
        void gradientRing(const Vec2 & center, float innerRadius, float outerRadius, float startAngle, ColorSpan colors);
        void triangle(const Vec2 & first, const Vec2 & second, const Vec2 & third, float thickness, const Color & color);
        void triangleFilled(const Vec2 & first, const Vec2 & second, const Vec2 & third, const Color & color);
        void convexPolygon(Vec2Span points, const Color & color);
        void gradientPolygon(ColoredPointSpan points);
        void concavePolygon(Vec2Span points, const Color & color);
        void bezierQuadratic(const Vec2 & first, const Vec2 & control, const Vec2 & second, float thickness, const Color & color, uint32_t segments = 0);
        void bezierCubic(const Vec2 & first, const Vec2 & firstControl, const Vec2 & secondControl, const Vec2 & second, float thickness, const Color & color, uint32_t segments = 0);
        [[nodiscard]] bool text(const Vec2 & position, StringView value, const Color & color, Vec2 * const _out);
        [[nodiscard]] bool textClipped(const Vec2 & position, StringView value, const Color & color, const Rect & clip, Vec2 * const _out);
        void pushClip(const Rect & bounds);
        void popClip();
        void image(TextureHandle texture, const Rect & bounds, const Rect & uv = {0.f, 0.f, 1.f, 1.f}, const Color & tint = {1.f, 1.f, 1.f, 1.f});
        void custom(VertexSpan vertices, IndexSpan indices, const RenderState & state = {});
    };
} // namespace Mosaic
