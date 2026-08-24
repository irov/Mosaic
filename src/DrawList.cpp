#include "Draw.hpp"

#include <algorithm>
#include <utility>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Color multiplyDrawAlpha(const Color & color, float alpha) noexcept
        {
            float resolvedAlpha = std::clamp(alpha, 0.f, 1.f);
            Color result = {color.r, color.g, color.b, color.a * resolvedAlpha};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool emptyDrawLine(const Vec2 & first, const Vec2 & second) noexcept
        {
            float x = second.x - first.x;
            float y = second.y - first.y;

            return x * x + y * y <= 0.00000001f;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Fill multiplyFillAlpha(Fill fill, float alpha) noexcept
        {
            fill.color = Detail::multiplyDrawAlpha(fill.color, alpha);
            for(uint32_t index = 0; index != fill.stopCount; ++index)
            {
                fill.stops[index].color = Detail::multiplyDrawAlpha(fill.stops[index].color, alpha);
            }

            return fill;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] BoxStyle multiplyBoxAlpha(BoxStyle style, float alpha) noexcept
        {
            style.fill = Detail::multiplyFillAlpha(style.fill, alpha);
            style.borderColor = Detail::multiplyDrawAlpha(style.borderColor, alpha);
            style.shadow.color = Detail::multiplyDrawAlpha(style.shadow.color, alpha);

            return style;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    void DrawList::clear() noexcept
    {
        m_commands.clear();
        m_alphaMultiplier = 1.f;
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::rect(const Rect & bounds, const Color & color, uint64_t renderKey)
    {
        if(bounds.empty() == true)
        {
            return;
        }

        if(color.a * m_alphaMultiplier <= 0.f)
        {
            return;
        }

        RectInstance instance = {bounds, Detail::multiplyDrawAlpha(color, m_alphaMultiplier)};

        if(m_commands.empty() == false)
        {
            DrawCommand & lastCommand = m_commands.back();

            if(lastCommand.type == DrawCommandType::RectBatch && lastCommand.renderKey == renderKey)
            {
                RectInstanceVector & destination = lastCommand.payload.rectBatch.instances;
                destination.push_back(instance);

                return;
            }
        }

        DrawCommand command(DrawCommandType::RectBatch);
        command.renderKey = renderKey;
        RectBatchDrawCommand & rectBatch = command.payload.rectBatch;
        RectInstanceVector & destination = rectBatch.instances;
        destination.push_back(instance);
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::rects(RectInstanceSpan instances, uint64_t renderKey)
    {
        for(const RectInstance & instance : instances)
        {
            DrawList::rect(instance.bounds, instance.color, renderKey);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::quads(QuadInstanceSpan instances, uint64_t renderKey)
    {
        if(instances.empty() == true)
        {
            return;
        }

        if(m_commands.empty() == false)
        {
            DrawCommand & lastCommand = m_commands.back();

            if(lastCommand.type == DrawCommandType::QuadBatch && lastCommand.renderKey == renderKey)
            {
                QuadInstanceVector & destination = lastCommand.payload.quadBatch.instances;
                destination.insert(destination.end(), instances.begin(), instances.end());

                return;
            }
        }

        DrawCommand command(DrawCommandType::QuadBatch);
        command.renderKey = renderKey;
        QuadBatchDrawCommand & quadBatch = command.payload.quadBatch;
        QuadInstanceVector & destination = quadBatch.instances;
        destination.assign(instances.begin(), instances.end());
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::box(const Rect & bounds, const BoxStyle & style, uint64_t renderKey)
    {
        if(bounds.empty() == true)
        {
            return;
        }

        DrawCommand command(DrawCommandType::Box);
        command.renderKey = renderKey;
        command.payload.box.bounds = bounds;
        command.payload.box.style = Detail::multiplyBoxAlpha(style, m_alphaMultiplier);
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::roundedRect(const Rect & bounds, float radius, const Color & color, uint64_t renderKey)
    {
        if(bounds.empty() == true)
        {
            return;
        }

        if(color.a * m_alphaMultiplier <= 0.f)
        {
            return;
        }

        DrawCommand command(DrawCommandType::RoundedRect);
        command.renderKey = renderKey;
        command.payload.rectangle.bounds = bounds;
        command.payload.rectangle.radius = radius;
        command.payload.rectangle.color = Detail::multiplyDrawAlpha(color, m_alphaMultiplier);
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::line(const Vec2 & first, const Vec2 & second, float thickness, const Color & color, uint64_t renderKey)
    {
        if(thickness <= 0.f)
        {
            return;
        }

        if(color.a * m_alphaMultiplier <= 0.f)
        {
            return;
        }

        if(Detail::emptyDrawLine(first, second) == true)
        {
            return;
        }

        DrawCommand command(DrawCommandType::Line);
        command.renderKey = renderKey;
        command.payload.line.first = first;
        command.payload.line.second = second;
        command.payload.line.thickness = thickness;
        command.payload.line.color = Detail::multiplyDrawAlpha(color, m_alphaMultiplier);
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::polyline(Vec2Span points, float thickness, const Color & color, bool closed, uint64_t renderKey)
    {
        size_t minimumPoints = closed ? 3 : 2;

        if(points.size() < minimumPoints)
        {
            return;
        }

        if(thickness <= 0.f)
        {
            return;
        }

        if(color.a * m_alphaMultiplier <= 0.f)
        {
            return;
        }

        DrawCommand command(DrawCommandType::Polyline);
        command.renderKey = renderKey;
        PolylineDrawCommand & polyline = command.payload.polyline;
        polyline.thickness = thickness;
        polyline.color = Detail::multiplyDrawAlpha(color, m_alphaMultiplier);
        polyline.closed = closed;
        polyline.points.assign(points.begin(), points.end());
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::image(const Rect & bounds, const Rect & uv, const Color & tint, uint64_t renderKey)
    {
        if(bounds.empty() == true)
        {
            return;
        }

        if(tint.a * m_alphaMultiplier <= 0.f)
        {
            return;
        }

        DrawCommand command(DrawCommandType::Image);
        command.renderKey = renderKey;
        command.payload.rectangle.bounds = bounds;
        command.payload.rectangle.uv = uv;
        command.payload.rectangle.color = Detail::multiplyDrawAlpha(tint, m_alphaMultiplier);
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::custom(VertexSpan vertices, IndexSpan indices, uint64_t renderKey)
    {
        if(vertices.empty() == true)
        {
            return;
        }

        if(indices.empty() == true)
        {
            return;
        }

        DrawCommand command(DrawCommandType::CustomGeometry);
        command.renderKey = renderKey;
        CustomGeometryDrawCommand & custom = command.payload.custom;
        custom.vertices.assign(vertices.begin(), vertices.end());
        for(Vertex & vertex : custom.vertices)
        {
            vertex.color = Detail::multiplyDrawAlpha(vertex.color, m_alphaMultiplier);
        }
        custom.indices.assign(indices.begin(), indices.end());
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::cached(VertexSpan vertices, IndexSpan indices, const Vec2 & translation, const Color & tint, uint64_t renderKey, const Vec2 & axisX, const Vec2 & axisY)
    {
        if(vertices.empty() == true)
        {
            return;
        }

        if(indices.empty() == true)
        {
            return;
        }

        if(tint.a * m_alphaMultiplier <= 0.f)
        {
            return;
        }

        DrawCommand command(DrawCommandType::CachedGeometry);
        command.renderKey = renderKey;
        command.payload.cached.vertices = vertices;
        command.payload.cached.indices = indices;
        command.payload.cached.translation = translation;
        command.payload.cached.axisX = axisX;
        command.payload.cached.axisY = axisY;
        command.payload.cached.tint = Detail::multiplyDrawAlpha(tint, m_alphaMultiplier);
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::pushClip(const Rect & clip, uint64_t renderKey)
    {
        DrawCommand command(DrawCommandType::PushClip);
        command.renderKey = renderKey;
        command.payload.rectangle.bounds = clip;
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::popClip(uint64_t renderKey)
    {
        DrawCommand command(DrawCommandType::PopClip);
        command.renderKey = renderKey;
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::setAlphaMultiplier(float alpha) noexcept
    {
        m_alphaMultiplier = std::clamp(alpha, 0.f, 1.f);
    }
    //////////////////////////////////////////////////////////////////////////
    float DrawList::alphaMultiplier() const noexcept
    {
        return m_alphaMultiplier;
    }
    //////////////////////////////////////////////////////////////////////////
    const DrawCommandVector & DrawList::commands() const noexcept
    {
        return m_commands;
    }
    //////////////////////////////////////////////////////////////////////////
    DrawCommandVector & DrawList::commands() noexcept
    {
        return m_commands;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
