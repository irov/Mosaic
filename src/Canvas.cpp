#include "Mosaic/Canvas.hpp"
#include "Mosaic/Mosaic.hpp"

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        void copyFillStops(Fill & fill, FillStopSpan stops) noexcept
        {
            fill.stopCount = static_cast<uint32_t>(std::min(stops.size(), FillStopCapacity));
            for(uint32_t index = 0; index != fill.stopCount; ++index)
            {
                fill.stops[index] = stops[index];
                fill.stops[index].offset = std::clamp(fill.stops[index].offset, 0.f, 1.f);
            }
            for(uint32_t index = 1; index != fill.stopCount; ++index)
            {
                FillStop value = fill.stops[index];
                uint32_t insertion = index;
                while(insertion != 0 && fill.stops[insertion - 1].offset > value.offset)
                {
                    fill.stops[insertion] = fill.stops[insertion - 1];
                    --insertion;
                }
                fill.stops[insertion] = value;
            }
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Fill solidFill(const Color & color) noexcept
    {
        Fill fill;
        fill.color = color;

        return fill;
    }
    //////////////////////////////////////////////////////////////////////////
    Fill linearFill(const Vec2 & from, const Vec2 & to, FillStopSpan stops, FillSpread spread) noexcept
    {
        Fill fill;
        fill.type = FillType::Linear;
        fill.spread = spread;
        fill.from = from;
        fill.to = to;
        Detail::copyFillStops(fill, stops);

        return fill;
    }
    //////////////////////////////////////////////////////////////////////////
    Fill radialFill(const Vec2 & center, float radius, FillStopSpan stops, FillSpread spread) noexcept
    {
        Fill fill;
        fill.type = FillType::Radial;
        fill.spread = spread;
        fill.from = center;
        fill.to = {radius, 0.f};
        Detail::copyFillStops(fill, stops);

        return fill;
    }
    //////////////////////////////////////////////////////////////////////////
    Fill conicFill(const Vec2 & center, float startAngle, FillStopSpan stops, FillSpread spread) noexcept
    {
        Fill fill;
        fill.type = FillType::Conic;
        fill.spread = spread;
        fill.from = center;
        fill.to = {startAngle, 0.f};
        Detail::copyFillStops(fill, stops);

        return fill;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::contentRect(Rect * const _out) const noexcept
    {
        if(m_context == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        Rect bounds;
        if(Mosaic::debugBounds(m_context, m_id, &bounds) == false)
        {
            return false;
        }

        *_out = {0.f, 0.f, bounds.width, bounds.height};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::clipRect(Rect * const _out) const noexcept
    {
        if(m_context == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        Rect bounds;
        if(Mosaic::debugBounds(m_context, m_id, &bounds) == false)
        {
            return false;
        }

        Rect clip;
        if(Mosaic::debugClip(m_context, m_id, &clip) == false)
        {
            return false;
        }

        *_out = {clip.x - bounds.x, clip.y - bounds.y, clip.width, clip.height};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::localPointerPosition(Vec2 * const _out) const noexcept
    {
        if(m_context == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        Rect content;
        if(Mosaic::debugBounds(m_context, m_id, &content) == false)
        {
            return false;
        }

        Vec2 pointer;
        if(Mosaic::pointerPosition(m_context, &pointer) == false)
        {
            return false;
        }

        *_out = pointer - Vec2{content.x, content.y};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::focused() const noexcept
    {
        auto returnedValue = m_context != nullptr && Mosaic::isFocused(m_context, m_id);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::setChannel(uint32_t channel) noexcept
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasSetChannel(m_context, m_id, channel);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::setLayer(CanvasLayer layer) noexcept
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasSetLayer(m_context, m_id, layer);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::rect(const Rect & bounds, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasRect(m_context, m_id, bounds, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::rects(RectInstanceSpan instances)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasRects(m_context, m_id, instances);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::quads(QuadInstanceSpan instances)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasQuads(m_context, m_id, instances);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::box(const Rect & bounds, const BoxStyle & style)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasBox(m_context, m_id, bounds, style);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::roundedRect(const Rect & bounds, float radius, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasRoundedRect(m_context, m_id, bounds, radius, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::gradient(const Rect & bounds, const Color & topLeft, const Color & topRight, const Color & bottomRight, const Color & bottomLeft)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasGradient(m_context, m_id, bounds, topLeft, topRight, bottomRight, bottomLeft);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::line(const Vec2 & first, const Vec2 & second, float thickness, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasLine(m_context, m_id, first, second, thickness, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::polyline(Vec2Span points, float thickness, const Color & color, bool closed)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasPolyline(m_context, m_id, points, thickness, color, closed);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::circle(const Vec2 & center, float radius, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasCircle(m_context, m_id, center, radius, thickness, color, segments);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::circleFilled(const Vec2 & center, float radius, const Color & color, uint32_t segments)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasCircleFilled(m_context, m_id, center, radius, color, segments);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::ellipse(const Vec2 & center, const Vec2 & radii, float rotation, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasEllipse(m_context, m_id, center, radii, rotation, thickness, color, segments);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::ellipseFilled(const Vec2 & center, const Vec2 & radii, float rotation, const Color & color, uint32_t segments)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasEllipseFilled(m_context, m_id, center, radii, rotation, color, segments);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::regularPolygon(const Vec2 & center, float radius, uint32_t sideCount, float rotation, float thickness, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasRegularPolygon(m_context, m_id, center, radius, sideCount, rotation, thickness, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::regularPolygonFilled(const Vec2 & center, float radius, uint32_t sideCount, float rotation, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasRegularPolygonFilled(m_context, m_id, center, radius, sideCount, rotation, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::gradientRing(const Vec2 & center, float innerRadius, float outerRadius, float startAngle, ColorSpan colors)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasGradientRing(m_context, m_id, center, innerRadius, outerRadius, startAngle, colors);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::triangle(const Vec2 & first, const Vec2 & second, const Vec2 & third, float thickness, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasTriangle(m_context, m_id, first, second, third, thickness, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::triangleFilled(const Vec2 & first, const Vec2 & second, const Vec2 & third, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasTriangleFilled(m_context, m_id, first, second, third, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::convexPolygon(Vec2Span points, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasConvexPolygon(m_context, m_id, points, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::gradientPolygon(ColoredPointSpan points)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasGradientPolygon(m_context, m_id, points);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::concavePolygon(Vec2Span points, const Color & color)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasConcavePolygon(m_context, m_id, points, color);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::bezierQuadratic(const Vec2 & first, const Vec2 & control, const Vec2 & second, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasBezierQuadratic(m_context, m_id, first, control, second, thickness, color, segments);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::bezierCubic(const Vec2 & first, const Vec2 & firstControl, const Vec2 & secondControl, const Vec2 & second, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasBezierCubic(m_context, m_id, first, firstControl, secondControl, second, thickness, color, segments);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::text(const Vec2 & position, StringView value, const Color & color, Vec2 * const _out)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool returnedValue = Mosaic::canvasText(m_context, m_id, position, value, color, _out);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::textClipped(const Vec2 & position, StringView value, const Color & color, const Rect & clip, Vec2 * const _out)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        Mosaic::canvasPushClip(m_context, m_id, clip);
        bool result = Mosaic::canvasText(m_context, m_id, position, value, color, _out);
        Mosaic::canvasPopClip(m_context, m_id);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::pushClip(const Rect & bounds)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasPushClip(m_context, m_id, bounds);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::popClip()
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasPopClip(m_context, m_id);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::image(TextureHandle texture, const Rect & bounds, const Rect & uv, const Color & tint)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasImage(m_context, m_id, texture, bounds, uv, tint);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Canvas::custom(VertexSpan vertices, IndexSpan indices, const RenderState & state)
    {
        if(m_context != nullptr)
        {
            Mosaic::canvasCustom(m_context, m_id, vertices, indices, state);
        }
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
