#include "Mosaic/Canvas.hpp"
#include "Mosaic/Mosaic.hpp"

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        void copyFillStops(Fill & fill, FillStopSpan stops) noexcept
        {
            if(stops.size() > FillStopCapacity)
            {
                fill.stopCount = static_cast<uint32_t>(FillStopCapacity + 1U);

                return;
            }

            fill.stopCount = static_cast<uint32_t>(stops.size());
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
    bool Canvas::setLayer(CanvasLayer layer) noexcept
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasSetLayer(m_context, m_id, layer);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::rect(const Rect & bounds, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasRect(m_context, m_id, bounds, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::rects(RectInstanceSpan instances)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasRects(m_context, m_id, instances);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::quads(QuadInstanceSpan instances)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasQuads(m_context, m_id, instances);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::box(const Rect & bounds, const BoxStyle & style)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasBox(m_context, m_id, bounds, style);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::roundedRect(const Rect & bounds, float radius, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasRoundedRect(m_context, m_id, bounds, radius, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::gradient(const Rect & bounds, const Color & topLeft, const Color & topRight, const Color & bottomRight, const Color & bottomLeft)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasGradient(m_context, m_id, bounds, topLeft, topRight, bottomRight, bottomLeft);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::line(const Vec2 & first, const Vec2 & second, float thickness, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasLine(m_context, m_id, first, second, thickness, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::polyline(Vec2Span points, float thickness, const Color & color, bool closed)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasPolyline(m_context, m_id, points, thickness, color, closed);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::circle(const Vec2 & center, float radius, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasCircle(m_context, m_id, center, radius, thickness, color, segments);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::circleFilled(const Vec2 & center, float radius, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasCircleFilled(m_context, m_id, center, radius, color, segments);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::arc(const Vec2 & center, float radius, float startAngle, float endAngle, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasArc(m_context, m_id, center, radius, startAngle, endAngle, thickness, color, segments);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::arcFilled(const Vec2 & center, float radius, float startAngle, float endAngle, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasArcFilled(m_context, m_id, center, radius, startAngle, endAngle, color, segments);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::ellipse(const Vec2 & center, const Vec2 & radii, float rotation, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasEllipse(m_context, m_id, center, radii, rotation, thickness, color, segments);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::ellipseFilled(const Vec2 & center, const Vec2 & radii, float rotation, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasEllipseFilled(m_context, m_id, center, radii, rotation, color, segments);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::regularPolygon(const Vec2 & center, float radius, uint32_t sideCount, float rotation, float thickness, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasRegularPolygon(m_context, m_id, center, radius, sideCount, rotation, thickness, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::regularPolygonFilled(const Vec2 & center, float radius, uint32_t sideCount, float rotation, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasRegularPolygonFilled(m_context, m_id, center, radius, sideCount, rotation, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::gradientRing(const Vec2 & center, float innerRadius, float outerRadius, float startAngle, ColorSpan colors)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasGradientRing(m_context, m_id, center, innerRadius, outerRadius, startAngle, colors);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::triangle(const Vec2 & first, const Vec2 & second, const Vec2 & third, float thickness, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasTriangle(m_context, m_id, first, second, third, thickness, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::triangleFilled(const Vec2 & first, const Vec2 & second, const Vec2 & third, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasTriangleFilled(m_context, m_id, first, second, third, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::convexPolygon(Vec2Span points, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasConvexPolygon(m_context, m_id, points, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::gradientPolygon(ColoredPointSpan points)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasGradientPolygon(m_context, m_id, points);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::concavePolygon(Vec2Span points, const Color & color)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasConcavePolygon(m_context, m_id, points, color);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::bezierQuadratic(const Vec2 & first, const Vec2 & control, const Vec2 & second, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasBezierQuadratic(m_context, m_id, first, control, second, thickness, color, segments);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::bezierQuadraticFilled(const Vec2 & first, const Vec2 & control, const Vec2 & second, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasBezierQuadraticFilled(m_context, m_id, first, control, second, color, segments);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::bezierCubic(const Vec2 & first, const Vec2 & firstControl, const Vec2 & secondControl, const Vec2 & second, float thickness, const Color & color, uint32_t segments)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasBezierCubic(m_context, m_id, first, firstControl, secondControl, second, thickness, color, segments);

        return result;
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

        if(Mosaic::canvasPushClip(m_context, m_id, clip) == false)
        {
            return false;
        }

        bool textResult = Mosaic::canvasText(m_context, m_id, position, value, color, _out);
        bool clipResult = Mosaic::canvasPopClip(m_context, m_id);
        bool result = textResult == true && clipResult == true;

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::pushClip(const Rect & bounds)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasPushClip(m_context, m_id, bounds);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::popClip()
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasPopClip(m_context, m_id);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::image(TextureHandle texture, const Rect & bounds, const Rect & uv, const Color & tint, SamplerFilter sampler)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasImage(m_context, m_id, texture, bounds, uv, tint, sampler);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Canvas::custom(VertexSpan vertices, IndexSpan indices, const RenderState & state)
    {
        if(m_context == nullptr)
        {
            return false;
        }

        bool result = Mosaic::canvasCustom(m_context, m_id, vertices, indices, state);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
