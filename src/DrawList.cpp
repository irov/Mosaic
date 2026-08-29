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
    void DrawList::setStorage(DrawCommandStorage * storage) noexcept
    {
        m_storage = storage;
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::clear() noexcept
    {
        m_commands.clear();
        m_alphaMultiplier = 1.f;
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::rect(const Rect & bounds, const Color & color, uint64_t renderKey)
    {
        if(m_storage == nullptr)
        {
            return;
        }

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
                DrawDataRange & range = lastCommand.payload.rectBatch.instances;

                if(range.offset + range.count == m_storage->rectangles.size())
                {
                    m_storage->rectangles.push_back(instance);
                    ++range.count;

                    return;
                }

            }
        }

        DrawCommand command(DrawCommandType::RectBatch);
        command.renderKey = renderKey;
        RectBatchDrawCommand & rectBatch = command.payload.rectBatch;
        rectBatch.instances.offset = m_storage->rectangles.size();
        rectBatch.instances.count = 1;
        m_storage->rectangles.push_back(instance);
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
        if(m_storage == nullptr)
        {
            return;
        }

        if(instances.empty() == true)
        {
            return;
        }

        if(m_commands.empty() == false)
        {
            DrawCommand & lastCommand = m_commands.back();

            if(lastCommand.type == DrawCommandType::QuadBatch && lastCommand.renderKey == renderKey)
            {
                DrawDataRange & range = lastCommand.payload.quadBatch.instances;

                if(range.offset + range.count == m_storage->quads.size())
                {
                    m_storage->quads.insert(m_storage->quads.end(), instances.begin(), instances.end());
                    range.count += instances.size();

                    return;
                }
            }
        }

        DrawCommand command(DrawCommandType::QuadBatch);
        command.renderKey = renderKey;
        QuadBatchDrawCommand & quadBatch = command.payload.quadBatch;
        quadBatch.instances.offset = m_storage->quads.size();
        quadBatch.instances.count = instances.size();
        m_storage->quads.insert(m_storage->quads.end(), instances.begin(), instances.end());
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
    void DrawList::roundedImage(const Rect & bounds, const Rect & uv, float radius, const Color & tint, uint64_t renderKey)
    {
        if(bounds.empty() == true)
        {
            return;
        }

        if(tint.a * m_alphaMultiplier <= 0.f)
        {
            return;
        }

        DrawCommand command(DrawCommandType::RoundedRect);
        command.renderKey = renderKey;
        command.payload.rectangle.bounds = bounds;
        command.payload.rectangle.uv = uv;
        command.payload.rectangle.radius = std::max(0.f, radius);
        command.payload.rectangle.color = Detail::multiplyDrawAlpha(tint, m_alphaMultiplier);
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
        if(m_storage == nullptr)
        {
            return;
        }

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
        polyline.points.offset = m_storage->points.size();
        polyline.points.count = points.size();
        m_storage->points.insert(m_storage->points.end(), points.begin(), points.end());
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::quadraticBezier(const Vec2 & first, const Vec2 & control, const Vec2 & second, float thickness, const Color & color, uint64_t renderKey)
    {
        if(m_storage == nullptr)
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

        DrawCommand command(DrawCommandType::Path);
        command.renderKey = renderKey;
        PathDrawCommand & path = command.payload.path;
        path.shape = PathShape::QuadraticBezier;
        path.points.offset = m_storage->points.size();
        path.points.count = 3;
        m_storage->points.push_back(first);
        m_storage->points.push_back(control);
        m_storage->points.push_back(second);
        path.color = Detail::multiplyDrawAlpha(color, m_alphaMultiplier);
        path.thickness = thickness;
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
        if(m_storage == nullptr)
        {
            return;
        }

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
        custom.vertices.offset = m_storage->vertices.size();
        custom.vertices.count = vertices.size();
        for(const Vertex & source : vertices)
        {
            Vertex vertex = source;
            vertex.color = Detail::multiplyDrawAlpha(vertex.color, m_alphaMultiplier);
            m_storage->vertices.push_back(vertex);
        }
        custom.indices.offset = m_storage->indices.size();
        custom.indices.count = indices.size();
        m_storage->indices.insert(m_storage->indices.end(), indices.begin(), indices.end());
        m_commands.emplace_back(std::move(command));
    }
    //////////////////////////////////////////////////////////////////////////
    void DrawList::textGeometry(uint64_t cacheKey, TexturedRectInstanceSpan rectangles, const Vec2 & translation, const Color & tint, uint64_t renderKey, const Vec2 & axisX, const Vec2 & axisY)
    {
        if(rectangles.empty() == true)
        {
            return;
        }

        if(tint.a * m_alphaMultiplier <= 0.f)
        {
            return;
        }

        DrawCommand command(DrawCommandType::TextGeometry);
        command.renderKey = renderKey;
        TextGeometryDrawCommand & geometry = command.payload.textGeometry;
        geometry.cacheKey = cacheKey;
        geometry.rectangles = rectangles;
        geometry.translation = translation;
        geometry.axisX = axisX;
        geometry.axisY = axisY;
        geometry.tint = Detail::multiplyDrawAlpha(tint, m_alphaMultiplier);
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
