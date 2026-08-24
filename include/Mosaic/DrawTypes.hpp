#pragma once

#include "Mosaic/Style.hpp"

namespace Mosaic
{
    inline constexpr size_t FillStopCapacity = 32;

    enum class FillType : uint8_t
    {
        Solid,
        Linear,
        Radial,
        Conic
    };

    enum class FillSpread : uint8_t
    {
        Clamp,
        Repeat,
        Mirror
    };

    struct FillStop
    {
        float offset = 0.f;
        Color color;
    };

    using FillStopArray = Array<FillStop, FillStopCapacity>;
    using FillStopSpan = Span<const FillStop>;

    struct Fill
    {
        FillType type = FillType::Solid;
        FillSpread spread = FillSpread::Clamp;
        Vec2 from;
        Vec2 to;
        Color color;
        FillStopArray stops;
        uint32_t stopCount = 0;
    };

    struct CornerRadii
    {
        float topLeft = 0.f;
        float topRight = 0.f;
        float bottomRight = 0.f;
        float bottomLeft = 0.f;
    };

    struct Shadow
    {
        Vec2 offset;
        float spread = 0.f;
        float blur = 0.f;
        Color color = {0.f, 0.f, 0.f, 0.f};
    };

    struct BoxStyle
    {
        Fill fill;
        CornerRadii radii;
        float borderWidth = 0.f;
        Color borderColor = {0.f, 0.f, 0.f, 0.f};
        float feather = 0.f;
        Shadow shadow;
        uint8_t cornerQuality = 0;
    };

    struct RectInstance
    {
        Rect bounds;
        Color color;
    };

    using RectInstanceVector = Vector<RectInstance>;
    using RectInstanceSpan = Span<const RectInstance>;

    struct RenderState
    {
        TextureHandle texture = 0;
        Rect clip;
        BlendMode blend = BlendMode::PremultipliedAlpha;
        RenderTargetHandle renderTarget = 0;
        uint32_t variant = 0;
        float clipRadius = 0.f;
        [[nodiscard]] bool operator==(const RenderState &) const noexcept = default;
    };

    using RenderStateVector = Vector<RenderState>;

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
        CachedGeometry,
        PushClip,
        PopClip
    };

    struct Vertex
    {
        Vec2 position;
        Color color;
        Vec2 uv;
    };

    using VertexVector = Vector<Vertex>;
    using VertexSpan = Span<const Vertex>;
    using VertexQuad = Array<Vertex, 4>;
    using IndexVector = Vector<uint32_t>;
    using IndexSpan = Span<const uint32_t>;

    struct QuadInstance
    {
        VertexQuad vertices;
    };

    using QuadInstanceVector = Vector<QuadInstance>;
    using QuadInstanceSpan = Span<const QuadInstance>;

    struct RectDrawCommand
    {
        Rect bounds;
        Rect uv = {0.f, 0.f, 1.f, 1.f};
        Color color;
        float radius = 0.f;
    };

    struct RectBatchDrawCommand
    {
        RectInstanceVector instances;
    };

    struct QuadBatchDrawCommand
    {
        QuadInstanceVector instances;
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
        Vec2Vector points;
        Color color;
        float thickness = 1.f;
        bool closed = false;
    };

    enum class PathShape : uint8_t
    {
        Circle,
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
        Vec2Vector points;
        ColoredPointVector coloredPoints;
        ColorVector colors;
        Vec2 center;
        Vec2 radii;
        Color color;
        float rotation = 0.f;
        float thickness = 1.f;
        uint32_t segments = 0;
        bool filled = false;
    };

    struct CustomGeometryDrawCommand
    {
        VertexVector vertices;
        IndexVector indices;
    };

    struct CachedGeometryDrawCommand
    {
        // Borrowed cached geometry is valid until the next beginFrame on its Context.
        VertexSpan vertices;
        IndexSpan indices;
        Vec2 translation;
        Vec2 axisX = {1.f, 0.f};
        Vec2 axisY = {0.f, 1.f};
        Color tint;
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
        CachedGeometryDrawCommand cached;

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

        DrawCommand(const DrawCommand & other) : type(other.type), renderKey(other.renderKey), channel(other.channel)
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
                channel = other.channel;
                copyFrom(other);
            }

            return *this;
        }

        DrawCommand(DrawCommand && other) noexcept : type(other.type), renderKey(other.renderKey), channel(other.channel)
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
                channel = other.channel;
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
        uint32_t channel = 0;
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
            case DrawCommandType::CachedGeometry:
                ::new(&payload.cached) CachedGeometryDrawCommand;
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
            case DrawCommandType::CachedGeometry:
                ::new(&payload.cached) CachedGeometryDrawCommand(std::move(other.payload.cached));
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
            case DrawCommandType::CachedGeometry:
                ::new(&payload.cached) CachedGeometryDrawCommand(other.payload.cached);
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
            case DrawCommandType::CachedGeometry:
            case DrawCommandType::PushClip:
            case DrawCommandType::PopClip:
                break;
            }
        }
    };

    using DrawCommandVector = Vector<DrawCommand>;

    enum class SemanticAction : uint32_t
    {
        None = 0,
        Focus = 1U << 0,
        Press = 1U << 1,
        Toggle = 1U << 2,
        Expand = 1U << 3,
        Collapse = 1U << 4,
        Increment = 1U << 5,
        Decrement = 1U << 6,
        SetValue = 1U << 7,
        Select = 1U << 8
    };

    using SemanticActionFlags = uint32_t;

    [[nodiscard]] constexpr SemanticActionFlags semanticAction(SemanticAction value) noexcept
    {
        auto returnedValue = static_cast<SemanticActionFlags>(value);

        return returnedValue;
    }

    struct SemanticNode
    {
        Id id = InvalidId;
        Id parentId = InvalidId;
        SemanticRole role = SemanticRole::None;
        String name;
        String description;
        String value;
        Rect bounds;
        bool checked = false;
        bool selected = false;
        bool expanded = false;
        bool disabled = false;
        bool focused = false;
        bool readOnly = false;
        SemanticActionFlags actions = 0;
    };

    using SemanticNodeVector = Vector<SemanticNode>;

    struct DebugInfo
    {
        Id id = InvalidId;
        Id parentId = InvalidId;
        StringView type;
        String label;
        String path;
        StringView file;
        StringView function;
        uint32_t line = 0;
        uint64_t firstFrame = 0;
        uint64_t lastFrame = 0;
        Rect bounds;
        Rect clip;
        bool hovered = false;
        bool active = false;
        bool focused = false;
        bool disabled = false;
    };

    using DebugInfoVector = Vector<DebugInfo>;

    enum class EventType : uint8_t
    {
        PointerDown,
        PointerUp,
        FocusChanged,
        BeginEdit,
        Change,
        Commit,
        Cancel,
        DragBegin,
        Drop,
        PopupOpen,
        PopupClose
    };

    struct EventTraceEntry
    {
        EventType type = EventType::PointerDown;
        Id id = InvalidId;
        String path;
        StringView file;
        uint32_t line = 0;
        double timestamp = 0.0;
    };

    using EventTraceVector = Vector<EventTraceEntry>;

    struct FrameMetrics
    {
        size_t widgetCount = 0;
        size_t visibleWidgetCount = 0;
        size_t culledWidgetCount = 0;
        size_t textRunCount = 0;
        size_t shapedTextRunCount = 0;
        size_t deferredTextRunCount = 0;
        size_t textCacheEntryCount = 0;
        size_t textCacheHitCount = 0;
        size_t textCacheMissCount = 0;
        size_t textCacheEvictionCount = 0;
        size_t textCacheMemory = 0;
        size_t fontCount = 0;
        size_t glyphCount = 0;
        size_t fontAtlasPageCount = 0;
        size_t fontAtlasMemory = 0;
        size_t persistentStateCount = 0;
        size_t drawCommandCount = 0;
        size_t vertexCount = 0;
        size_t indexCount = 0;
        size_t batchCount = 0;
        size_t clipChanges = 0;
        size_t textureChanges = 0;
        size_t frameMemory = 0;
        size_t persistentMemory = 0;
        double buildMilliseconds = 0.0;
        double layoutMilliseconds = 0.0;
        double emitMilliseconds = 0.0;
        double meshMilliseconds = 0.0;
    };

    struct Glyph
    {
        TextureHandle texture = 0;
        Rect uv;
        Vec2 size;
        Vec2 bearing;
        float advance = 0.f;
    };

    struct ShapedGlyph
    {
        uint32_t glyph = 0;
        uint32_t face = 0;
        Vec2 position;
        float advance = 0.f;
        size_t cluster = 0;
    };

    using ShapedGlyphVector = Vector<ShapedGlyph>;

    struct RenderBatch
    {
        uint32_t vertexOffset = 0;
        uint32_t vertexCount = 0;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint64_t renderKey = 0;
        Rect bounds;
    };

    using RenderBatchVector = Vector<RenderBatch>;

    struct RenderVertex
    {
        Vec2 position;
        uint32_t color = 0xffffffffU;
        Vec2 uv;
    };

    using RenderVertexVector = Vector<RenderVertex>;

    struct RenderMeshMetrics
    {
        double buildMilliseconds = 0.0;
    };

    struct RenderMesh
    {
        RenderVertexVector vertices;
        IndexVector indices;
        RenderBatchVector batches;
        RenderStateVector renderStates;
        RenderMeshMetrics metrics;
        void clear() noexcept;
    };
} // namespace Mosaic
