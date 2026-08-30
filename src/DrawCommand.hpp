#pragma once

#include "Mosaic/DrawTypes.hpp"

namespace Mosaic
{
    enum class DrawCommandType : uint8_t
    {
        Rect,
        RectBatch,
        QuadBatch,
        Box,
        RoundedRect,
        Gradient,
        Line,
        Polyline,
        Path,
        Image,
        CustomGeometry,
        TextGeometry,
        PushClip,
        PopClip
    };

    struct TexturedRectInstance
    {
        Rect bounds;
        Rect uv;
        Color color = {1.f, 1.f, 1.f, 1.f};
    };

    using TexturedRectInstanceVector = Vector<TexturedRectInstance>;
    using TexturedRectInstanceSpan = Span<const TexturedRectInstance>;

    struct DrawDataRange
    {
        size_t offset = 0;
        size_t count = 0;
    };

    struct RectDrawCommand
    {
        Rect bounds;
        Rect uv = {0.f, 0.f, 1.f, 1.f};
        Color color;
        float radius = 0.f;
    };

    struct RectBatchDrawCommand
    {
        DrawDataRange instances;
    };

    struct QuadBatchDrawCommand
    {
        DrawDataRange instances;
    };

    struct BoxDrawCommand
    {
        Rect bounds;
        BoxStyle style;
    };

    struct LineDrawCommand
    {
        Vec2 first;
        Vec2 second;
        Color color;
        float thickness = 1.f;
    };

    struct GradientDrawCommand
    {
        Rect bounds;
        Array<Color, 4> colors;
    };

    struct PolylineDrawCommand
    {
        DrawDataRange points;
        Color color;
        float thickness = 1.f;
        bool closed = false;
    };

    enum class PathShape : uint8_t
    {
        Circle,
        Arc,
        Ellipse,
        RegularPolygon,
        Polygon,
        GradientRing,
        QuadraticBezier,
        CubicBezier
    };

    struct PathDrawCommand
    {
        PathShape shape = PathShape::Polygon;
        DrawDataRange points;
        DrawDataRange coloredPoints;
        DrawDataRange colors;
        Vec2 center;
        Vec2 radii;
        Color color;
        float rotation = 0.f;
        float startAngle = 0.f;
        float endAngle = 0.f;
        float thickness = 1.f;
        uint32_t segments = 0;
        bool filled = false;
    };

    struct CustomGeometryDrawCommand
    {
        DrawDataRange vertices;
        DrawDataRange indices;
    };

    struct TextGeometryDrawCommand
    {
        TexturedRectInstanceSpan rectangles;
        uint64_t cacheKey = 0;
        Vec2 translation;
        Vec2 axisX = {1.f, 0.f};
        Vec2 axisY = {0.f, 1.f};
        Color tint;
    };

    struct DrawCommandStorage
    {
        using MutableRectInstanceSpan = Span<RectInstance>;
        using MutableQuadInstanceSpan = Span<QuadInstance>;
        using MutableVec2Span = Span<Vec2>;
        using MutableColoredPointSpan = Span<ColoredPoint>;
        using MutableVertexSpan = Span<Vertex>;

        RectInstanceVector rectangles;
        QuadInstanceVector quads;
        Vec2Vector points;
        ColoredPointVector coloredPoints;
        ColorVector colors;
        VertexVector vertices;
        IndexVector indices;

        void clear() noexcept
        {
            rectangles.clear();
            quads.clear();
            points.clear();
            coloredPoints.clear();
            colors.clear();
            vertices.clear();
            indices.clear();
        }

        void swap(DrawCommandStorage & other) noexcept
        {
            rectangles.swap(other.rectangles);
            quads.swap(other.quads);
            points.swap(other.points);
            coloredPoints.swap(other.coloredPoints);
            colors.swap(other.colors);
            vertices.swap(other.vertices);
            indices.swap(other.indices);
        }

        [[nodiscard]] RectInstanceSpan rectangleSpan(const DrawDataRange & range) const noexcept
        {
            return RectInstanceSpan(rectangles).subspan(range.offset, range.count);
        }

        [[nodiscard]] QuadInstanceSpan quadSpan(const DrawDataRange & range) const noexcept
        {
            return QuadInstanceSpan(quads).subspan(range.offset, range.count);
        }

        [[nodiscard]] Vec2Span pointSpan(const DrawDataRange & range) const noexcept
        {
            return Vec2Span(points).subspan(range.offset, range.count);
        }

        [[nodiscard]] ColoredPointSpan coloredPointSpan(const DrawDataRange & range) const noexcept
        {
            return ColoredPointSpan(coloredPoints).subspan(range.offset, range.count);
        }

        [[nodiscard]] ColorSpan colorSpan(const DrawDataRange & range) const noexcept
        {
            return ColorSpan(colors).subspan(range.offset, range.count);
        }

        [[nodiscard]] VertexSpan vertexSpan(const DrawDataRange & range) const noexcept
        {
            return VertexSpan(vertices).subspan(range.offset, range.count);
        }

        [[nodiscard]] IndexSpan indexSpan(const DrawDataRange & range) const noexcept
        {
            return IndexSpan(indices).subspan(range.offset, range.count);
        }

        [[nodiscard]] MutableRectInstanceSpan mutableRectangleSpan(const DrawDataRange & range) noexcept
        {
            return MutableRectInstanceSpan(rectangles).subspan(range.offset, range.count);
        }

        [[nodiscard]] MutableQuadInstanceSpan mutableQuadSpan(const DrawDataRange & range) noexcept
        {
            return MutableQuadInstanceSpan(quads).subspan(range.offset, range.count);
        }

        [[nodiscard]] MutableVec2Span mutablePointSpan(const DrawDataRange & range) noexcept
        {
            return MutableVec2Span(points).subspan(range.offset, range.count);
        }

        [[nodiscard]] MutableColoredPointSpan mutableColoredPointSpan(const DrawDataRange & range) noexcept
        {
            return MutableColoredPointSpan(coloredPoints).subspan(range.offset, range.count);
        }

        [[nodiscard]] MutableVertexSpan mutableVertexSpan(const DrawDataRange & range) noexcept
        {
            return MutableVertexSpan(vertices).subspan(range.offset, range.count);
        }
    };

    union DrawCommandPayload
    {
        RectDrawCommand rectangle;
        RectBatchDrawCommand rectBatch;
        QuadBatchDrawCommand quadBatch;
        BoxDrawCommand box;
        GradientDrawCommand gradient;
        LineDrawCommand line;
        PolylineDrawCommand polyline;
        PathDrawCommand path;
        CustomGeometryDrawCommand custom;
        TextGeometryDrawCommand textGeometry;

        DrawCommandPayload() noexcept
        {
        }

        ~DrawCommandPayload() noexcept
        {
        }
    };

    struct DrawCommand
    {
        explicit DrawCommand(DrawCommandType commandType = DrawCommandType::Rect) : type(commandType)
        {
            construct();
        }

        DrawCommand(const DrawCommand & other) : type(other.type), renderKey(other.renderKey)
        {
            copyFrom(other);
        }

        DrawCommand & operator=(const DrawCommand & other)
        {
            if(this != &other)
            {
                destroy();
                type = other.type;
                renderKey = other.renderKey;
                copyFrom(other);
            }

            return *this;
        }

        DrawCommand(DrawCommand && other) noexcept : type(other.type), renderKey(other.renderKey)
        {
            moveFrom(std::move(other));
        }

        DrawCommand & operator=(DrawCommand && other) noexcept
        {
            if(this != &other)
            {
                destroy();
                type = other.type;
                renderKey = other.renderKey;
                moveFrom(std::move(other));
            }

            return *this;
        }

        ~DrawCommand() noexcept
        {
            destroy();
        }

        DrawCommandType type = DrawCommandType::Rect;
        uint64_t renderKey = 0;
        DrawCommandPayload payload;

    private:
        void construct()
        {
            switch(type)
            {
            case DrawCommandType::Rect:
            case DrawCommandType::RoundedRect:
            case DrawCommandType::Image:
            case DrawCommandType::PushClip:
            case DrawCommandType::PopClip:
                ::new(&payload.rectangle) RectDrawCommand;
                break;
            case DrawCommandType::RectBatch:
                ::new(&payload.rectBatch) RectBatchDrawCommand;
                break;
            case DrawCommandType::QuadBatch:
                ::new(&payload.quadBatch) QuadBatchDrawCommand;
                break;
            case DrawCommandType::Box:
                ::new(&payload.box) BoxDrawCommand;
                break;
            case DrawCommandType::Gradient:
                ::new(&payload.gradient) GradientDrawCommand;
                break;
            case DrawCommandType::Line:
                ::new(&payload.line) LineDrawCommand;
                break;
            case DrawCommandType::Polyline:
                ::new(&payload.polyline) PolylineDrawCommand;
                break;
            case DrawCommandType::Path:
                ::new(&payload.path) PathDrawCommand;
                break;
            case DrawCommandType::CustomGeometry:
                ::new(&payload.custom) CustomGeometryDrawCommand;
                break;
            case DrawCommandType::TextGeometry:
                ::new(&payload.textGeometry) TextGeometryDrawCommand;
                break;
            }
        }

        void moveFrom(DrawCommand && other)
        {
            switch(type)
            {
            case DrawCommandType::Rect:
            case DrawCommandType::RoundedRect:
            case DrawCommandType::Image:
            case DrawCommandType::PushClip:
            case DrawCommandType::PopClip:
                ::new(&payload.rectangle) RectDrawCommand(std::move(other.payload.rectangle));
                break;
            case DrawCommandType::RectBatch:
                ::new(&payload.rectBatch) RectBatchDrawCommand(std::move(other.payload.rectBatch));
                break;
            case DrawCommandType::QuadBatch:
                ::new(&payload.quadBatch) QuadBatchDrawCommand(std::move(other.payload.quadBatch));
                break;
            case DrawCommandType::Box:
                ::new(&payload.box) BoxDrawCommand(std::move(other.payload.box));
                break;
            case DrawCommandType::Gradient:
                ::new(&payload.gradient) GradientDrawCommand(std::move(other.payload.gradient));
                break;
            case DrawCommandType::Line:
                ::new(&payload.line) LineDrawCommand(std::move(other.payload.line));
                break;
            case DrawCommandType::Polyline:
                ::new(&payload.polyline) PolylineDrawCommand(std::move(other.payload.polyline));
                break;
            case DrawCommandType::Path:
                ::new(&payload.path) PathDrawCommand(std::move(other.payload.path));
                break;
            case DrawCommandType::CustomGeometry:
                ::new(&payload.custom) CustomGeometryDrawCommand(std::move(other.payload.custom));
                break;
            case DrawCommandType::TextGeometry:
                ::new(&payload.textGeometry) TextGeometryDrawCommand(std::move(other.payload.textGeometry));
                break;
            }
        }

        void copyFrom(const DrawCommand & other)
        {
            switch(type)
            {
            case DrawCommandType::Rect:
            case DrawCommandType::RoundedRect:
            case DrawCommandType::Image:
            case DrawCommandType::PushClip:
            case DrawCommandType::PopClip:
                ::new(&payload.rectangle) RectDrawCommand(other.payload.rectangle);
                break;
            case DrawCommandType::RectBatch:
                ::new(&payload.rectBatch) RectBatchDrawCommand(other.payload.rectBatch);
                break;
            case DrawCommandType::QuadBatch:
                ::new(&payload.quadBatch) QuadBatchDrawCommand(other.payload.quadBatch);
                break;
            case DrawCommandType::Box:
                ::new(&payload.box) BoxDrawCommand(other.payload.box);
                break;
            case DrawCommandType::Gradient:
                ::new(&payload.gradient) GradientDrawCommand(other.payload.gradient);
                break;
            case DrawCommandType::Line:
                ::new(&payload.line) LineDrawCommand(other.payload.line);
                break;
            case DrawCommandType::Polyline:
                ::new(&payload.polyline) PolylineDrawCommand(other.payload.polyline);
                break;
            case DrawCommandType::Path:
                ::new(&payload.path) PathDrawCommand(other.payload.path);
                break;
            case DrawCommandType::CustomGeometry:
                ::new(&payload.custom) CustomGeometryDrawCommand(other.payload.custom);
                break;
            case DrawCommandType::TextGeometry:
                ::new(&payload.textGeometry) TextGeometryDrawCommand(other.payload.textGeometry);
                break;
            }
        }

        void destroy() noexcept
        {
            switch(type)
            {
            case DrawCommandType::RectBatch:
                payload.rectBatch.~RectBatchDrawCommand();
                break;
            case DrawCommandType::QuadBatch:
                payload.quadBatch.~QuadBatchDrawCommand();
                break;
            case DrawCommandType::Polyline:
                payload.polyline.~PolylineDrawCommand();
                break;
            case DrawCommandType::Path:
                payload.path.~PathDrawCommand();
                break;
            case DrawCommandType::CustomGeometry:
                payload.custom.~CustomGeometryDrawCommand();
                break;
            case DrawCommandType::Rect:
            case DrawCommandType::Box:
            case DrawCommandType::RoundedRect:
            case DrawCommandType::Gradient:
            case DrawCommandType::Line:
            case DrawCommandType::Image:
            case DrawCommandType::TextGeometry:
            case DrawCommandType::PushClip:
            case DrawCommandType::PopClip:
                break;
            }
        }
    };

    using DrawCommandVector = Vector<DrawCommand>;

} // namespace Mosaic
