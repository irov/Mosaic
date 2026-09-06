#pragma once

#include "Mosaic/Style.hpp"

namespace Mosaic
{
    inline constexpr size_t FillStopCapacity = 32;

    struct Transform2D
    {
        Vec2 translation;
        Vec2 axisX = {1.f, 0.f};
        Vec2 axisY = {0.f, 1.f};
    };

    enum class StrokeScale : uint8_t
    {
        World,
        Screen
    };

    enum class NineSliceMode : uint8_t
    {
        Stretch,
        Tile
    };

    struct NineSliceOptions
    {
        Vec2 sourceSize;
        Rect uv = {0.f, 0.f, 1.f, 1.f};
        float left = 0.f;
        float top = 0.f;
        float right = 0.f;
        float bottom = 0.f;
        NineSliceMode edges = NineSliceMode::Stretch;
        NineSliceMode center = NineSliceMode::Stretch;
        Color tint = {1.f, 1.f, 1.f, 1.f};
    };

    struct GridStyle
    {
        Vec2 origin;
        Vec2 axisX = {1.f, 0.f};
        Vec2 axisY = {0.f, 1.f};
        Rect range;
        float minorSpacing = 16.f;
        uint32_t majorInterval = 8;
        float minorThickness = 1.f;
        float majorThickness = 1.f;
        Color minorColor = {1.f, 1.f, 1.f, 0.08f};
        Color majorColor = {1.f, 1.f, 1.f, 0.2f};
        StrokeScale strokeScale = StrokeScale::Screen;
        bool bounded = false;
    };

    struct Icon
    {
        TextureHandle texture = 0;
        Rect uv = {0.f, 0.f, 1.f, 1.f};
        Vec2 logicalSize = {16.f, 16.f};
        Color tint = {1.f, 1.f, 1.f, 1.f};
        StringView semanticFallback;
    };

    struct HighlightedTextRange
    {
        size_t begin = 0;
        size_t end = 0;
        Color color = {1.f, 0.78f, 0.24f, 1.f};
    };

    using HighlightedTextRangeVector = Vector<HighlightedTextRange>;
    using HighlightedTextRangeSpan = Span<const HighlightedTextRange>;

    enum class EditorTransactionPhase : uint8_t
    {
        Begin,
        Change,
        Commit,
        Cancel
    };

    struct EditorTransaction
    {
        Id owner = InvalidId;
        TypeId type = 0;
        EditorTransactionPhase phase = EditorTransactionPhase::Begin;
        ByteSpan before;
        ByteSpan after;
    };

    using EditorTransactionCallback = void (*)(const EditorTransaction & transaction, void * userData);

    enum class TreeDropZone : uint8_t
    {
        None,
        Before,
        Inside,
        After
    };

    struct TreeBadge
    {
        StringView text;
        Color color = {0.36f, 0.62f, 0.92f, 1.f};
    };

    using TreeBadgeVector = Vector<TreeBadge>;
    using TreeBadgeSpan = Span<const TreeBadge>;

    struct TreeTrailingAction
    {
        Key key;
        Icon icon;
        StringView tooltip;
        bool enabled = true;
    };

    using TreeTrailingActionVector = Vector<TreeTrailingAction>;
    using TreeTrailingActionSpan = Span<const TreeTrailingAction>;

    struct TreeRow
    {
        Key key;
        Id item = InvalidId;
        uint32_t depth = 0;
        bool leaf = false;
        bool expanded = false;
        bool selected = false;
        bool renameActive = false;
        bool dragEnabled = false;
        bool dropEnabled = false;
        Icon leadingIcon;
        StringView label;
        TreeBadgeSpan badges;
        TreeTrailingActionSpan trailingActions;
        String * renameValue = nullptr;
        TypeId dragType = 0;
        ByteSpan dragPayload;
        TypeId acceptedDropType = 0;
        Id scrollArea = InvalidId;
        float autoScrollMargin = 24.f;
        float autoScrollSpeed = 320.f;
        float autoExpandDelay = 0.65f;
    };

    using TreeRowVector = Vector<TreeRow>;
    using TreeRowSpan = Span<const TreeRow>;

    using VirtualItemExtent = float (*)(size_t item, void * userData);
    using VirtualItemOffset = float (*)(size_t item, void * userData);

    struct VirtualScrollAnchor
    {
        size_t item = 0;
        size_t itemCount = 0;
        Id identity = InvalidId;
        float offset = 0.f;
        bool valid = false;
    };

    using VirtualItemIdentity = Id (*)(size_t item, void * userData);

    struct VirtualListOptions
    {
        Orientation orientation = Orientation::Vertical;
        float fixedExtent = 0.f;
        float estimatedExtent = 20.f;
        float spacing = 0.f;
        size_t overscan = 2;
        Id scrollArea = InvalidId;
        VirtualItemExtent extent = nullptr;
        VirtualItemOffset offset = nullptr;
        VirtualItemIdentity identity = nullptr;
        void * userData = nullptr;
        VirtualScrollAnchor * anchor = nullptr;
        size_t scrollToItem = std::numeric_limits<size_t>::max();
        float scrollToAlignment = 0.f;
    };

    struct VirtualListState
    {
        VisibleRange range;
        size_t itemCount = 0;
        float leadingExtent = 0.f;
        float trailingExtent = 0.f;
        float totalExtent = 0.f;
    };

    struct VirtualGridOptions
    {
        Vec2 itemSize = {96.f, 96.f};
        Vec2 spacing = {8.f, 8.f};
        size_t minimumColumns = 1;
        size_t overscanRows = 1;
        Id scrollArea = InvalidId;
        VirtualItemIdentity identity = nullptr;
        void * userData = nullptr;
        VirtualScrollAnchor * anchor = nullptr;
        size_t scrollToItem = std::numeric_limits<size_t>::max();
        float scrollToAlignment = 0.f;
    };

    struct VirtualGridState
    {
        VisibleRange range;
        size_t columns = 1;
        size_t firstRow = 0;
        size_t lastRow = 0;
        float leadingExtent = 0.f;
        float trailingExtent = 0.f;
    };

    struct ResourceTile
    {
        Key key;
        Id resource = InvalidId;
        Icon thumbnail;
        StringView label;
        StringView type;
        bool selected = false;
        bool loading = false;
        bool error = false;
        TypeId dragType = 0;
        ByteSpan dragPayload;
    };

    using ResourceTileVector = Vector<ResourceTile>;
    using ResourceTileSpan = Span<const ResourceTile>;

    enum class ResourceBrowserMode : uint8_t
    {
        Tiles,
        List
    };

    enum class DesignSelectionMode : uint8_t
    {
        None,
        Marquee,
        Lasso
    };

    struct DesignGuide
    {
        Vec2 begin;
        Vec2 end;
        Color color = {0.2f, 0.72f, 1.f, 0.72f};
    };

    using DesignGuideVector = Vector<DesignGuide>;
    using DesignGuideSpan = Span<const DesignGuide>;

    struct DesignSurfaceState
    {
        Vec2 pan;
        float zoom = 1.f;
        Vec2 pointerAnchor;
        Vec2 selectionAnchor;
        Rect selectionBounds;
        Vec2Vector lasso;
        bool pointerTracked = false;
        bool selecting = false;
    };

    struct PropertyEditOptions
    {
        bool mixed = false;
        bool readOnly = false;
        bool resettable = false;
        Validation validation = Validation::Normal;
        StringView validationMessage;
        Id resource = InvalidId;
        TypeId resourceType = 0;
    };


    enum class GizmoKind : uint8_t
    {
        Point,
        Axis,
        Box,
        ResizeEdge,
        ResizeCorner,
        Rotation,
        Padding,
        Gap,
        Splitter
    };

    struct GizmoOptions
    {
        GizmoKind kind = GizmoKind::Point;
        StrokeScale strokeScale = StrokeScale::Screen;
        Color color = {0.16f, 0.65f, 1.f, 1.f};
        float size = 8.f;
        float thickness = 1.f;
        bool enabled = true;
    };

    struct SnapQuery
    {
        Id owner = InvalidId;
        Rect movingBounds;
        Vec2 proposedDelta;
        float threshold = 6.f;
    };

    struct SnapResult
    {
        Vec2 adjustedDelta;
        Vec2 firstGuideBegin;
        Vec2 firstGuideEnd;
        Vec2 secondGuideBegin;
        Vec2 secondGuideEnd;
        bool snappedX = false;
        bool snappedY = false;
    };

    using SnapProvider = bool (*)(const SnapQuery & query, SnapResult * const _out, void * userData);

    enum class LayoutTrackSizeKind : uint8_t
    {
        Fixed,
        Weight,
        Content
    };

    struct LayoutBoxCell
    {
        Id id = InvalidId;
        Id box = InvalidId;
        Id parent = InvalidId;
        Rect bounds;
        Rect contentBounds;
        uint32_t depth = 0;
        int32_t renderOrder = 0;
        LayoutTrackSizeKind sizeKind = LayoutTrackSizeKind::Weight;
        float size = 1.f;
        float minimumSize = 0.f;
        float gap = 0.f;
        EdgeInsets padding;
        bool leaf = true;
        bool selected = false;
        bool safeArea = false;
    };

    using LayoutBoxCellVector = Vector<LayoutBoxCell>;
    using LayoutBoxCellSpan = Span<const LayoutBoxCell>;

    struct LayoutBoxSplitter
    {
        Id id = InvalidId;
        Id box = InvalidId;
        Id before = InvalidId;
        Id after = InvalidId;
        Orientation orientation = Orientation::Horizontal;
        Rect bounds;
        float beforeMinimum = 0.f;
        float afterMinimum = 0.f;
    };

    using LayoutBoxSplitterVector = Vector<LayoutBoxSplitter>;
    using LayoutBoxSplitterSpan = Span<const LayoutBoxSplitter>;

    enum class LayoutBoxEditKind : uint8_t
    {
        None,
        Select,
        MoveSplitter,
        ResizePadding,
        ChangeGap,
        Place
    };

    struct LayoutBoxEditOptions
    {
        float handleThickness = 7.f;
        float snapThreshold = 6.f;
        bool snapping = true;
        bool pairedEdges = false;
        bool snappingTemporarilyDisabled = false;
        bool drawCellBounds = true;
        bool drawSafeArea = true;
    };

    struct TimelineSnapQuery
    {
        Id owner = InvalidId;
        double time = 0.0;
        double threshold = 0.0;
        bool frames = true;
        bool keyframes = true;
        bool markers = true;
        bool boundaries = true;
    };

    struct TimelineSnapResult
    {
        double time = 0.0;
        Id target = InvalidId;
        bool snapped = false;
    };

    using TimelineSnapProvider = bool (*)(const TimelineSnapQuery & query, TimelineSnapResult * const _out, void * userData);

    struct TimelineTrack
    {
        Id id = InvalidId;
        Id parent = InvalidId;
        uint32_t depth = 0;
        StringView label;
        Icon icon;
        bool expanded = true;
        bool selected = false;
        bool muted = false;
        bool locked = false;
    };

    using TimelineTrackVector = Vector<TimelineTrack>;
    using TimelineTrackSpan = Span<const TimelineTrack>;

    struct TimelineKeyframe
    {
        Id id = InvalidId;
        Id track = InvalidId;
        double time = 0.0;
        bool selected = false;
    };

    using TimelineKeyframeVector = Vector<TimelineKeyframe>;
    using TimelineKeyframeSpan = Span<const TimelineKeyframe>;

    struct TimelineState
    {
        double visibleBegin = 0.0;
        double visibleEnd = 10.0;
        double playhead = 0.0;
        double workBegin = 0.0;
        double workEnd = 10.0;
        double loopBegin = 0.0;
        double loopEnd = 10.0;
        float trackWidth = 240.f;
        float trackHeight = 22.f;
        float trackScroll = 0.f;
    };

    struct TimelineOptions
    {
        LayoutOptions layout;
        DoubleSpan markers;
        TimelineSnapProvider snapProvider = nullptr;
        void * snapUserData = nullptr;
        double frameRate = 60.0;
        float snapThreshold = 6.f;
        bool snapFrames = true;
        bool snapKeyframes = true;
        bool snapMarkers = true;
        bool snapBoundaries = true;
        bool allowDuplicate = true;
        bool allowScale = true;
    };

    struct TimelineResponse
    {
        Id item = InvalidId;
        double time = 0.0;
        double timeDelta = 0.0;
        double selectionBegin = 0.0;
        double selectionEnd = 0.0;
        double scale = 1.0;
        Rect selectionBounds;
        size_t firstSelectedTrack = std::numeric_limits<size_t>::max();
        size_t lastSelectedTrack = std::numeric_limits<size_t>::max();
        EditorTransactionPhase phase = EditorTransactionPhase::Change;
        bool playheadChanged = false;
        bool keyframesChanged = false;
        bool marquee = false;
        bool selectionFinished = false;
        bool duplicateRequested = false;
        bool scaleRequested = false;
        bool snapped = false;
    };

    struct CurvePoint
    {
        Id id = InvalidId;
        Vec2 value;
        Vec2 incomingTangent;
        Vec2 outgoingTangent;
        Color color = {0.3f, 0.75f, 1.f, 1.f};
        bool selected = false;
    };

    using CurvePointVector = Vector<CurvePoint>;
    using CurvePointSpan = Span<const CurvePoint>;

    struct CurveEditorResponse
    {
        Id point = InvalidId;
        enum class Handle : uint8_t
        {
            Point,
            IncomingTangent,
            OutgoingTangent
        } handle = Handle::Point;
        Vec2 delta;
        Rect selectionBounds;
        EditorTransactionPhase phase = EditorTransactionPhase::Change;
        bool changed = false;
        bool boxSelection = false;
    };

    struct CurveEditorOptions
    {
        LayoutOptions layout;
        float pointRadius = 5.f;
        float tangentRadius = 4.f;
        float hitRadius = 9.f;
        bool editTangents = true;
        bool boxSelection = true;
    };

    enum class NodePinDirection : uint8_t
    {
        Input,
        Output
    };

    struct NodePin
    {
        Id id = InvalidId;
        Id node = InvalidId;
        TypeId type = 0;
        NodePinDirection direction = NodePinDirection::Input;
        StringView label;
        Color color = {0.5f, 0.7f, 1.f, 1.f};
    };

    using NodePinVector = Vector<NodePin>;
    using NodePinSpan = Span<const NodePin>;

    struct GraphNode
    {
        Id id = InvalidId;
        Rect bounds;
        StringView title;
        Icon icon;
        NodePinSpan pins;
        bool selected = false;
    };

    using GraphNodeVector = Vector<GraphNode>;
    using GraphNodeSpan = Span<const GraphNode>;

    struct GraphLink
    {
        Id id = InvalidId;
        Id outputPin = InvalidId;
        Id inputPin = InvalidId;
        Color color = {0.5f, 0.7f, 1.f, 1.f};
    };

    using GraphLinkVector = Vector<GraphLink>;
    using GraphLinkSpan = Span<const GraphLink>;

    using NodeConnectionValidator = bool (*)(const NodePin & output, const NodePin & input, void * userData);

    struct NodeGraphOptions
    {
        LayoutOptions layout;
        NodeConnectionValidator connectionValidator = nullptr;
        void * connectionUserData = nullptr;
        Vec2 minimapSize = {180.f, 120.f};
        float pinHitRadius = 10.f;
        bool showMinimap = true;
        bool allowConnections = true;
    };

    struct NodeGraphResponse
    {
        Id node = InvalidId;
        Id pin = InvalidId;
        Id link = InvalidId;
        Id outputPin = InvalidId;
        Id inputPin = InvalidId;
        Vec2 delta;
        Vec2 connectionPosition;
        EditorTransactionPhase phase = EditorTransactionPhase::Change;
        bool connectionPreview = false;
        bool connectionCommitted = false;
        bool connectionAccepted = false;
        bool minimapInteracted = false;
    };

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
        SamplerFilter sampler = SamplerFilter::Linear;
        Rect clip;
        BlendMode blend = BlendMode::PremultipliedAlpha;
        RenderTargetHandle renderTarget = 0;
        uint32_t variant = 0;
        float clipRadius = 0.f;
        float linePenumbra = 0.f;
        float fillFeather = 0.f;
        float curveTessellationMaximumError = 0.f;
        float circleTessellationMaximumError = 0.f;
        uint8_t curveQuality = 0;
        uint8_t ellipseQuality = 0;
        uint8_t rectangleQuality = 0;
        [[nodiscard]] bool operator==(const RenderState &) const noexcept = default;
    };

    using RenderStateVector = Vector<RenderState>;

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
        uint64_t zOrder = 0;
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
        size_t textMeasureCacheHitCount = 0;
        size_t textMeasureCacheMissCount = 0;
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
        size_t generalAllocationCount = 0;
        size_t generalAllocationBytes = 0;
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
        explicit RenderMesh(Allocator * allocator = nullptr)
            : vertices(StlAllocator<RenderVertex>(allocator == nullptr ? defaultAllocator() : *allocator))
            , indices(StlAllocator<uint32_t>(allocator == nullptr ? defaultAllocator() : *allocator))
            , batches(StlAllocator<RenderBatch>(allocator == nullptr ? defaultAllocator() : *allocator))
            , renderStates(StlAllocator<RenderState>(allocator == nullptr ? defaultAllocator() : *allocator))
        {
        }

        RenderVertexVector vertices;
        IndexVector indices;
        RenderBatchVector batches;
        RenderStateVector renderStates;
        RenderMeshMetrics metrics;
        void clear() noexcept;
    };
} // namespace Mosaic
