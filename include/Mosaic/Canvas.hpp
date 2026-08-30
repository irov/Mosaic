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
        bool setLayer(CanvasLayer layer) noexcept;

        bool rect(const Rect & bounds, const Color & color);
        bool rects(RectInstanceSpan instances);
        bool quads(QuadInstanceSpan instances);
        bool box(const Rect & bounds, const BoxStyle & style);
        bool roundedRect(const Rect & bounds, float radius, const Color & color);
        bool gradient(const Rect & bounds, const Color & topLeft, const Color & topRight, const Color & bottomRight, const Color & bottomLeft);
        bool line(const Vec2 & first, const Vec2 & second, float thickness, const Color & color);
        bool polyline(Vec2Span points, float thickness, const Color & color, bool closed = false);
        bool circle(const Vec2 & center, float radius, float thickness, const Color & color, uint32_t segments = 0);
        bool circleFilled(const Vec2 & center, float radius, const Color & color, uint32_t segments = 0);
        bool arc(const Vec2 & center, float radius, float startAngle, float endAngle, float thickness, const Color & color, uint32_t segments = 0);
        bool arcFilled(const Vec2 & center, float radius, float startAngle, float endAngle, const Color & color, uint32_t segments = 0);
        bool ellipse(const Vec2 & center, const Vec2 & radii, float rotation, float thickness, const Color & color, uint32_t segments = 0);
        bool ellipseFilled(const Vec2 & center, const Vec2 & radii, float rotation, const Color & color, uint32_t segments = 0);
        bool regularPolygon(const Vec2 & center, float radius, uint32_t sideCount, float rotation, float thickness, const Color & color);
        bool regularPolygonFilled(const Vec2 & center, float radius, uint32_t sideCount, float rotation, const Color & color);
        bool gradientRing(const Vec2 & center, float innerRadius, float outerRadius, float startAngle, ColorSpan colors);
        bool triangle(const Vec2 & first, const Vec2 & second, const Vec2 & third, float thickness, const Color & color);
        bool triangleFilled(const Vec2 & first, const Vec2 & second, const Vec2 & third, const Color & color);
        bool convexPolygon(Vec2Span points, const Color & color);
        bool gradientPolygon(ColoredPointSpan points);
        bool concavePolygon(Vec2Span points, const Color & color);
        bool bezierQuadratic(const Vec2 & first, const Vec2 & control, const Vec2 & second, float thickness, const Color & color, uint32_t segments = 0);
        bool bezierQuadraticFilled(const Vec2 & first, const Vec2 & control, const Vec2 & second, const Color & color, uint32_t segments = 0);
        bool bezierCubic(const Vec2 & first, const Vec2 & firstControl, const Vec2 & secondControl, const Vec2 & second, float thickness, const Color & color, uint32_t segments = 0);
        [[nodiscard]] bool text(const Vec2 & position, StringView value, const Color & color, Vec2 * const _out);
        [[nodiscard]] bool textClipped(const Vec2 & position, StringView value, const Color & color, const Rect & clip, Vec2 * const _out);
        bool pushClip(const Rect & bounds);
        bool popClip();
        bool image(TextureHandle texture, const Rect & bounds, const Rect & uv = {0.f, 0.f, 1.f, 1.f}, const Color & tint = {1.f, 1.f, 1.f, 1.f}, SamplerFilter sampler = SamplerFilter::Linear);
        bool custom(VertexSpan vertices, IndexSpan indices, const RenderState & state = {});
    };
} // namespace Mosaic
