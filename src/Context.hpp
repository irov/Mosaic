#pragma once

#include "Mosaic/Mosaic.hpp"
#include "DrawList.hpp"

#include <functional>
#include <limits>

namespace Mosaic
{
    namespace Detail
    {
        inline constexpr uint32_t CulledNodeFlag = 1U << 31U;

        enum class ItemPressPolicy : uint8_t
        {
            Release,
            Press,
            DoubleClick
        };

        [[nodiscard]] ItemPressPolicy itemPressPolicy(ButtonPressPolicy policy) noexcept;

        struct ItemBehaviorOptions
        {
            bool keyboardActivation = true;
            PointerButton pointerButton = PointerButton::Primary;
            ItemPressPolicy pressPolicy = ItemPressPolicy::Release;
            bool repeat = false;
            bool allowOverlap = false;
            bool allowDoubleClick = false;
        };

        [[nodiscard]] constexpr bool tableSizingFixed(TableSizing sizing) noexcept
        {
            return sizing == TableSizing::Fixed || sizing == TableSizing::FixedFit || sizing == TableSizing::FixedSame;
        }

        [[nodiscard]] constexpr bool tableSizingStretch(TableSizing sizing) noexcept
        {
            return sizing == TableSizing::Stretch || sizing == TableSizing::StretchProportional || sizing == TableSizing::StretchSame;
        }

        [[nodiscard]] inline float tableStretchWeight(const TableColumnOptions & options, float intrinsicWidth) noexcept
        {
            if(options.sizing == TableSizing::StretchSame)
            {
                return 1.f;
            }

            if(options.sizing == TableSizing::StretchProportional && options.widthOrWeight <= 0.f)
            {
                auto returnedValue = std::max(0.001f, intrinsicWidth);

                return returnedValue;
            }

            auto returnedValue = std::max(0.001f, options.widthOrWeight);

            return returnedValue;
        }

        struct TextUndoRecord
        {
            size_t offset = 0;
            String removed;
            String inserted;
        };

        using TextUndoRecordVector = Vector<TextUndoRecord>;

        struct TextEditorState
        {
            size_t cursor = 0;
            size_t anchor = 0;
            float textScrollX = 0.f;
            float textScrollY = 0.f;
            bool selectingText = false;
            String editOriginal;
            String editValue;
            String composition;
            size_t compositionBegin = 0;
            size_t compositionEnd = 0;
            size_t compositionSelectionBegin = 0;
            size_t compositionSelectionEnd = 0;
            String undoValue;
            TextUndoRecordVector undo;
            size_t undoBegin = 0;
            size_t undoCount = 0;
            size_t undoPosition = 0;
            double lastEditTimestamp = 0.0;
        };

        struct ColorEditorState
        {
            bool pickerOpen = false;
            bool initialized = false;
            float hue = 0.f;
            float saturation = 0.f;
            float value = 0.f;
            Color last;
            String hexText;
            Array<String, 4> channelText;
            uint8_t activeChannel = 0xfeU;
            float dragStartValue = 0.f;
        };

        struct NumericState
        {
            long double dragStartValue = 0.L;
            long double dragValueSpeed = 0.L;
            long double dragAccumulator = 0.L;
            long double dragLastApplied = 0.L;
            float sliderGrabOffset = 0.f;
            String temporaryText;
            long double lastValue = 0.L;
            bool initialized = false;
            bool temporaryInput = false;
            bool temporaryReplace = false;
            bool temporaryDoubleClickBlocked = false;
            bool dragThresholdPassed = false;
        };

        enum class NodeKind : uint8_t
        {
            Root,
            Scope,
            Window,
            Row,
            Column,
            Grid,
            Overlay,
            Scroll,
            Split,
            Absolute,
            Clip,
            Disabled,
            Interaction,
            Style,
            Backdrop,
            Text,
            Bullet,
            BulletText,
            Button,
            IconButton,
            Checkbox,
            Radio,
            Selectable,
            Tab,
            Toggle,
            Combo,
            Slider,
            DragValue,
            Progress,
            Separator,
            SeparatorText,
            Spacer,
            Image,
            ImageButton,
            InputText,
            InputMultiline,
            ColorEdit,
            Tree,
            Table,
            TableRow,
            TableCell,
            Canvas
        };
    } // namespace Detail

    struct Context
    {
        struct PreparedGlyph
        {
            TextureHandle texture = 0;
            Rect bounds;
            Rect uv;
            size_t cluster = 0;
        };

        using PreparedGlyphVector = Vector<PreparedGlyph>;

        struct PreparedTextBatch
        {
            TextureHandle texture = 0;
            VertexVector vertices;
            IndexVector indices;
        };

        using PreparedTextBatchVector = Vector<PreparedTextBatch>;
        using StringQuad = Array<String, 4>;
        using Vec2Quad = Array<Vec2, 4>;

        struct TextCacheAttributes
        {
            const FontProvider * provider = nullptr;
            uint64_t providerRevision = 0;
            FontHandle font = DefaultFont;
            uint32_t fontSize = 0;
            uint32_t wrapWidth = 0;
            bool editable = false;
            bool wordWrap = false;

            [[nodiscard]] bool operator==(const TextCacheAttributes &) const noexcept = default;
        };

        struct CachedText
        {
            TextCacheAttributes attributes;
            String text;
            Vec2 size;
            FloatVector offsets;
            SizeVector clusters;
            Vec2Vector positions;
            ShapedTextLineVector lines;
            PreparedTextBatchVector batches;
            uint64_t key = 0;
            uint64_t lastFrame = 0;
            CachedText * previous = nullptr;
            CachedText * next = nullptr;
        };

        using CachedTextPtr = UniquePtr<CachedText>;
        using CachedTextPtrVector = Vector<CachedTextPtr>;
        using TextCache = UnorderedMap<uint64_t, CachedTextPtrVector>;
        using CachedTextQuad = Array<const CachedText *, 4>;

        struct ColorTextData
        {
            StringQuad text;
            CachedTextQuad runs = {};
            Vec2Quad sizes;
            Array<bool, 4> prepared = {true, true, true, true};
        };

        using ColorTextDataVector = Vector<ColorTextData>;

        struct TextUseHistory
        {
            uint64_t key = 0;
            uint64_t lastFrame = 0;
            uint8_t uses = 0;
        };

        struct TransientTextIndexEntry
        {
            uint64_t key = 0;
            uint64_t frame = 0;
            size_t index = 0;
        };

        using TextUseHistoryArray = Array<TextUseHistory, 4096>;
        using TransientTextIndexVector = Vector<TransientTextIndexEntry>;
        using ThemePtr = UniquePtr<Theme>;
        using ThemePtrVector = Vector<ThemePtr>;

        struct Persistent;

        struct Node
        {
            Detail::NodeKind kind = Detail::NodeKind::Scope;
            Id id = InvalidId;
            Id parentId = InvalidId;
            size_t parent = 0;
            size_t firstChild = std::numeric_limits<size_t>::max();
            size_t lastChild = std::numeric_limits<size_t>::max();
            size_t nextSibling = std::numeric_limits<size_t>::max();
            String label;
            String valueText;
            StringView file;
            StringView function;
            uint32_t line = 0;
            Persistent * persistentState = nullptr;
            LayoutOptions layout;
            const Theme * style = nullptr;
            Vec2 measured;
            Vec2 textSize;
            float baseline = 0.f;
            bool baselineValid = false;
            Rect bounds;
            Rect content;
            Rect clip;
            Rect visualClip;
            Rect childrenClip;
            Rect explicitClip;
            Rect treeFrameBounds;
            Rect treeFrameClip;
            Rect treeLabelClip;
            Response response;
            SemanticRole semanticRole = SemanticRole::None;
            Id inputLayer = InvalidId;
            Id windowOwner = InvalidId;
            bool visible = true;
            bool disabled = false;
            bool inputBlocked = false;
            bool navigationBlocked = false;
            bool focusable = false;
            bool cursorOverride = false;
            CursorShape cursor = CursorShape::Arrow;
            bool checked = false;
            bool selected = false;
            bool expanded = false;
            bool treeFramed = false;
            bool treeLeaf = false;
            bool treeBullet = false;
            bool treeFramePadding = false;
            bool treeSpanLabelWidth = false;
            bool treeSpanAllColumns = false;
            bool treeLabelSpanAllColumns = false;
            bool treeAlignLabelWithCurrentX = false;
            bool treeNavigationLeftJumpsToParent = false;
            TreeLineMode treeLines = TreeLineMode::None;
            bool treeCloseVisible = false;
            bool treeCloseHovered = false;
            bool multiline = false;
            bool password = false;
            bool wordWrap = false;
            bool textHint = false;
            bool numericInput = false;
            bool readOnly = false;
            bool showValuePopup = false;
            bool showValueOnTrack = true;
            bool showValueTooltip = false;
            bool colorMarkerEnabled = false;
            bool textPrepared = false;
            bool valueTextPrepared = true;
            bool scrollLayoutDirty = false;
            bool scrollResizeHovered = false;
            bool sliderVertical = false;
            uint8_t scrollResizeEdges = 0;
            float tooltipDelay = 0.25f;
            Validation validation = Validation::Normal;
            bool windowTitleVisible = true;
            bool windowBackgroundVisible = true;
            float windowBackgroundAlpha = 1.f;
            bool windowUnsavedDocument = false;
            bool windowDocked = false;
            bool windowDockAutoHideTabBar = false;
            uint32_t windowDockGroup = 0;
            bool windowCollapsed = false;
            bool windowCloseVisible = false;
            bool windowCloseHovered = false;
            bool windowCollapseVisible = false;
            bool windowCollapseHovered = false;
            WindowCollapsePlacement windowCollapsePlacement = WindowCollapsePlacement::Right;
            bool windowResizeHovered = false;
            bool windowResizable = false;
            bool windowResizeGripVisible = false;
            uint8_t windowResizeEdges = 0;
            Rect windowResizeBounds;
            bool windowAutoSize = false;
            bool windowFitContentWidth = false;
            bool windowFitContentHeight = false;
            bool windowContentSizeExplicit = false;
            Vec2 windowContentSize;
            bool windowScrollable = false;
            bool windowPopup = false;
            bool windowBringToFront = true;
            bool windowAttachedPopup = false;
            bool menuBar = false;
            bool menuPopupItem = false;
            bool menuCheckVisible = false;
            bool menuSubmenu = false;
            bool fillBackground = true;
            bool fillHoverBackground = true;
            bool highlighted = false;
            bool selectableSpanAllColumns = false;
            bool arrowButton = false;
            bool hyperlink = false;
            bool overrideTextAlignment = false;
            Vec2 textAlignment;
            bool tabSelectedOverline = false;
            bool tabLeading = false;
            bool tabTrailing = false;
            bool tabCloseVisible = false;
            bool tabCloseHovered = false;
            bool tabUnsavedDocument = false;
            bool comboShowArrow = true;
            bool comboShowPreview = true;
            bool comboWidthFitPreview = false;
            Direction direction = Direction::Right;
            bool colorShowInputs = false;
            bool colorShowPreview = true;
            ColorInputMode colorInputMode = ColorInputMode::RgbByte;
            bool colorAlphaBackground = true;
            bool colorAlphaPreviewHalf = false;
            bool colorBorder = true;
            bool colorMarkers = true;
            bool colorHdr = false;
            uint8_t colorComponents = 0;
            LabelPlacement labelPlacement = LabelPlacement::Before;
            DockNodeId windowDockNode = 0;
            float scalar = 0.f;
            float secondaryScalar = 0.f;
            float textScrollX = 0.f;
            float textScrollY = 0.f;
            float textWrapWidth = 0.f;
            float itemWidth = 0.f;
            bool itemWidthRequested = false;
            uint64_t windowZOrder = 0;
            float splitMinimumFirst = 0.f;
            float splitMinimumSecond = 0.f;
            Vec2 windowMinimumSize;
            Vec2 windowMaximumSize = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
            Rect popupAnchor;
            PopupPlacement popupPlacement = PopupPlacement::Automatic;
            PopupHorizontalAlignment popupHorizontalAlignment = PopupHorizontalAlignment::Start;
            TextureHandle texture = 0;
            Rect uv = {0.f, 0.f, 1.f, 1.f};
            Color tint = {1.f, 1.f, 1.f, 1.f};
            Color imageBackground;
            float imagePadding = 2.f;
            bool imageBackgroundEnabled = false;
            Color colorMarker;
            ScrollOptions scrollOptions;
            TableOptions tableOptions;
            Id tableSettingsId = InvalidId;
            TableColumnOptions tableColumnOptions;
            SortDirection tableSortDirection = SortDirection::None;
            uint32_t tableSortOrder = 0;
            Array<Color, 2> tableRowBackgrounds;
            Array<bool, 2> tableRowBackgroundsEnabled = {false, false};
            Color tableCellBackground;
            bool tableCellBackgroundEnabled = false;
            uint32_t tableRow = 0;
            uint32_t tableColumn = 0;
            bool tableHeader = false;
            size_t textCursor = 0;
            size_t textAnchor = 0;
            size_t compositionBegin = 0;
            size_t compositionEnd = 0;
            const CachedText * textRun = nullptr;
            const CachedText * valueTextRun = nullptr;
            Vec2 valueTextSize;
            size_t colorTextDataIndex = std::numeric_limits<size_t>::max();
            size_t canvasCommandIndex = std::numeric_limits<size_t>::max();
            uint32_t canvasChannel = 0;
            uint32_t canvasChannelCount = 1;
            CanvasLayer canvasLayer = CanvasLayer::Local;
            size_t frameStringIndex = std::numeric_limits<size_t>::max();
        };

        struct RecycledNodeStorage
        {
            String label;
            String valueText;
        };

        struct FrameNodeStrings
        {
            String semanticName;
            String semanticDescription;
            String semanticValue;
            String path;
        };

        struct TableColumnState
        {
            String label;
            TableColumnOptions options;
            float resolvedWidth = 0.f;
            Rect lastBounds;
            Rect bodyBounds;
            float resizeStartX = 0.f;
            float resizeStartWidth = 0.f;
            uint32_t order = 0;
            bool resizing = false;
            bool configured = false;
            bool restored = false;
        };

        using TableColumnStateVector = Vector<TableColumnState>;

        struct TableState
        {
            TableOptions options;
            TableColumnStateVector columns;
            TableSortSpecVector sortSpecs;
            FloatVector intrinsicWidths;
            FloatVector rowHeights;
            FloatVector requestedRowHeights;
            FloatVector requestedRowPaddingY;
            FloatVector rowPositions;
            FloatVector columnPositions;
            SizeVector displayOrder;
            uint32_t currentRow = 0;
            uint32_t currentColumn = 0;
            uint32_t headerRowCount = 0;
            uint32_t draggingColumn = std::numeric_limits<uint32_t>::max();
            uint32_t contextColumn = std::numeric_limits<uint32_t>::max();
            float columnDragStartX = 0.f;
            size_t virtualRowCount = 0;
            float virtualRowHeight = 0.f;
            uint32_t virtualFirstRow = 0;
            uint32_t maximumRow = 0;
            Array<Color, 2> currentRowBackgrounds;
            Color pendingCellBackground;
            bool columnDragMoved = false;
            bool headersSubmitted = false;
            bool angledHeadersSubmitted = false;
            bool submitAngledHeaders = false;
            bool rowsVirtualized = false;
            bool currentCellSubmitted = false;
            bool displayOrderDirty = true;
            bool sortSpecsDirty = false;
            Array<bool, 2> currentRowBackgroundsEnabled = {false, false};
            bool pendingCellBackgroundEnabled = false;
        };

        struct TabState
        {
            TabsOptions options;
            SizeVector order;
            SizeVector prefixNodes;
            SizeVector suffixNodes;
            SizeVector leadingNodes;
            SizeVector centralNodes;
            SizeVector trailingNodes;
            SizeVector layoutNodes;
            IdVector orderIds;
            IdVector previousItems;
            IdVector submittedItems;
            Id scrollArea = InvalidId;
            Id selectedItem = InvalidId;
            size_t hostNode = std::numeric_limits<size_t>::max();
            size_t headerNode = std::numeric_limits<size_t>::max();
            size_t itemCount = 0;
            size_t visibleCount = 0;
            size_t dragging = std::numeric_limits<size_t>::max();
            uint64_t lastFrame = 0;
        };

        struct Persistent
        {
            Rect lastBounds;
            Rect lastClip;
            Rect pressBounds;
            Rect pressClip;
            Id lastInputLayer = InvalidId;
            Id windowOwner = InvalidId;
            Rect windowBounds;
            bool windowInitialized = false;
            bool windowSettingsPolicyInitialized = false;
            bool windowSaveSettings = true;
            bool windowVisible = false;
            bool windowAcceptsInput = true;
            bool windowBringToFront = true;
            bool windowPopup = false;
            uint64_t windowZOrder = 0;
            bool windowContentWidthFitted = false;
            bool windowContentHeightFitted = false;
            bool windowPositionConditionApplied = false;
            bool windowSizeConditionApplied = false;
            bool windowContentSizeConditionApplied = false;
            bool windowCollapsedConditionApplied = false;
            bool windowCollapsedValue = false;
            bool windowCollapsedInitialized = false;
            uint64_t windowVisibleLastFrame = 0;
            bool expanded = false;
            bool expandedInitialized = false;
            Id autoCloseTree = InvalidId;
            float scroll = 0.f;
            float scrollExtent = 0.f;
            Vec2 scrollPosition;
            Vec2 scrollTarget;
            Vec2 scrollVelocity;
            Vec2 scrollRange;
            Vec2 scrollContentSize;
            bool scrollTargetInitialized = false;
            bool scrollToEndX = false;
            bool scrollToEndY = false;
            Orientation scrollOrientation = Orientation::Vertical;
            Rect scrollbarTrackBounds;
            Rect scrollbarThumbBounds;
            Rect verticalScrollbarTrack;
            Rect verticalScrollbarThumb;
            Rect horizontalScrollbarTrack;
            Rect horizontalScrollbarThumb;
            float scrollbarDragOffset = 0.f;
            bool draggingScrollbar = false;
            uint8_t draggingScrollAxis = 0;
            Vec2 scrollAreaSize;
            Vec2 scrollResizeStartSize;
            bool scrollAreaSizeInitialized = false;
            bool resizingScrollArea = false;
            uint8_t scrollResizeEdges = 0;
            bool editing = false;
            bool acceptsTabInput = false;
            Vec2 dragOffset;
            Vec2 dragStartPosition;
            Vec2 dragLastPosition;
            Rect windowResizeStartBounds;
            uint8_t windowResizeEdges = 0;
            bool resizingDockHost = false;
            size_t numericStateIndex = std::numeric_limits<size_t>::max();
            double pressStartedTimestamp = 0.0;
            double lastRepeatTimestamp = 0.0;
            double hoverStartedTimestamp = 0.0;
            double hoverDuration = 0.0;
            double stationaryHoverStartedTimestamp = 0.0;
            double stationaryHoverDuration = 0.0;
            uint64_t hoverLastFrame = 0;
            uint64_t navigationFocusLastFrame = 0;
            Rect splitterBounds;
            Orientation splitterOrientation = Orientation::Horizontal;
            uint32_t splitterSnapIndex = 0;
            bool draggingWindow = false;
            bool windowBackgroundMovePending = false;
            uint8_t windowInteraction = 0;
            bool visualInitialized = false;
            float hoverVisual = 0.f;
            float activeVisual = 0.f;
            float selectionVisual = 0.f;
            float focusVisual = 0.f;
            float scalarVisual = 0.f;
            uint64_t firstFrame = 0;
            uint64_t lastFrame = 0;
            UniquePtr<TableState> table;
        };

        struct PopupState
        {
            Id id = InvalidId;
            Id key = InvalidId;
            Id node = InvalidId;
            Id owner = InvalidId;
            Id restoreFocus = InvalidId;
            PopupOptions options;
            Rect bounds;
            uint32_t level = 0;
            uint64_t lastFrame = 0;
            bool open = false;
        };

        using PopupStateVector = Vector<PopupState>;

        struct SelectionItem
        {
            SelectionModel * model = nullptr;
            Id item = InvalidId;
            Id node = InvalidId;
            Id scope = InvalidId;
            bool navigationWrapX = false;
        };

        struct SelectionOverlay
        {
            Rect bounds;
            Rect clip;
            Color fill;
            Color border;
        };

        using SelectionItemVector = Vector<SelectionItem>;
        using SelectionOverlayVector = Vector<SelectionOverlay>;

        struct ScopeState
        {
            uint64_t token = 0;
            size_t previousParent = 0;
            bool previousDisabled = false;
            bool previousInputBlocked = false;
            bool previousNavigationBlocked = false;
            bool previousLiveEditText = true;
            bool previousLiveEditScalar = true;
            Id previousInputLayer = InvalidId;
            Id previousWindow = InvalidId;
            const Theme * previousStyle = nullptr;
            SelectionModel * previousSelectionModel = nullptr;
            IdSpan previousSelectionOrder;
            SelectionOptions previousSelectionOptions;
            Id previousSelectionScope = InvalidId;
        };

        using NodeVector = Vector<Node>;
        using RecycledNodeStorageVector = Vector<RecycledNodeStorage>;
        using PersistentMap = UnorderedMap<Id, Persistent>;
        using TextEditorStateMap = UnorderedMap<Id, Detail::TextEditorState>;
        using ColorEditorStateMap = UnorderedMap<Id, Detail::ColorEditorState>;
        using TabStateMap = UnorderedMap<Id, TabState>;
        using DrawCommandVectorVector = Vector<DrawCommandVector>;
        using FrameNodeStringVector = Vector<FrameNodeStrings>;

        struct FrameNodeIndexEntry
        {
            Id id = InvalidId;
            size_t index = 0;
            uint64_t frame = 0;
        };

        struct NextWindowData
        {
            Vec2 position;
            Vec2 size;
            Vec2 contentSize;
            Condition positionCondition = Condition::Always;
            Condition sizeCondition = Condition::Always;
            Condition contentSizeCondition = Condition::Always;
            Condition collapsedCondition = Condition::Always;
            bool positionPending = false;
            bool sizePending = false;
            bool contentSizePending = false;
            bool collapsedPending = false;
            bool collapsed = false;
            bool focusPending = false;
        };

        using FrameNodeIndexVector = Vector<FrameNodeIndexEntry>;
        using DockModelMap = UnorderedMap<uint32_t, DockModel>;
        using DockAreaMap = UnorderedMap<uint32_t, Rect>;
        using DockSpaceOptionsMap = UnorderedMap<uint32_t, DockSpaceOptions>;
        using WindowDockGroupMap = UnorderedMap<Id, uint32_t>;
        using DockWindowMap = UnorderedMap<uint32_t, IdVector>;
        using DockLayoutIndexMap = UnorderedMap<Id, size_t>;

        struct RenderStateHash
        {
            [[nodiscard]] size_t operator()(const RenderState & value) const noexcept
            {
                auto combine = [](size_t seed, size_t part) noexcept
                {
                    auto returnedValue = seed ^ (part + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));

                    return returnedValue;
                };
                size_t hash = std::hash<TextureHandle>{}(value.texture);
                hash = combine(hash, std::hash<float>{}(value.clip.x));
                hash = combine(hash, std::hash<float>{}(value.clip.y));
                hash = combine(hash, std::hash<float>{}(value.clip.width));
                hash = combine(hash, std::hash<float>{}(value.clip.height));
                hash = combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(value.blend)));
                hash = combine(hash, std::hash<RenderTargetHandle>{}(value.renderTarget));
                hash = combine(hash, std::hash<uint32_t>{}(value.variant));
                auto returnedValue = combine(hash, std::hash<float>{}(value.clipRadius));

                return returnedValue;
            }
        };

        struct RenderStateIndexEntry
        {
            size_t hash = 0;
            uint64_t key = 0;
            uint64_t frame = 0;
        };

        using RenderStateIndexVector = Vector<RenderStateIndexEntry>;

        struct ThemeHash
        {
            [[nodiscard]] size_t operator()(const Theme & value) const noexcept
            {
                auto combine = [](size_t seed, size_t part) noexcept
                {
                    auto returnedValue = seed ^ (part + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));

                    return returnedValue;
                };
                auto color = [&combine](size_t seed, const Color & value) noexcept
                {
                    seed = combine(seed, std::hash<float>{}(value.r));
                    seed = combine(seed, std::hash<float>{}(value.g));
                    seed = combine(seed, std::hash<float>{}(value.b));
                    auto returnedValue = combine(seed, std::hash<float>{}(value.a));

                    return returnedValue;
                };
                auto vector = [&combine](size_t seed, const Vec2 & value) noexcept
                {
                    seed = combine(seed, std::hash<float>{}(value.x));
                    auto returnedValue = combine(seed, std::hash<float>{}(value.y));

                    return returnedValue;
                };
                auto insets = [&combine](size_t seed, const EdgeInsets & value) noexcept
                {
                    seed = combine(seed, std::hash<float>{}(value.left));
                    seed = combine(seed, std::hash<float>{}(value.top));
                    seed = combine(seed, std::hash<float>{}(value.right));
                    auto returnedValue = combine(seed, std::hash<float>{}(value.bottom));

                    return returnedValue;
                };
                size_t hash = std::hash<FontHandle>{}(value.metrics.font);
                hash = combine(hash, std::hash<float>{}(value.metrics.fontSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.lineHeight));
                hash = combine(hash, std::hash<float>{}(value.metrics.padding));
                hash = combine(hash, std::hash<float>{}(value.metrics.gap));
                hash = insets(hash, value.metrics.framePadding);
                hash = vector(hash, value.metrics.itemSpacing);
                hash = vector(hash, value.metrics.innerSpacing);
                hash = insets(hash, value.metrics.cellPadding);
                hash = insets(hash, value.metrics.popupPadding);
                hash = combine(hash, std::hash<float>{}(value.metrics.indent));
                hash = combine(hash, std::hash<float>{}(value.metrics.borderWidth));
                hash = combine(hash, std::hash<float>{}(value.metrics.windowBorderSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.childBorderSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.popupBorderSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.frameBorderSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.tabBorderSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.tabBarBorderSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.tabMinimumWidthBase));
                hash = combine(hash, std::hash<float>{}(value.metrics.tabMinimumWidthForShrink));
                hash = combine(hash, std::hash<float>{}(value.metrics.cornerRadius));
                hash = combine(hash, std::hash<float>{}(value.metrics.frameCornerRadius));
                hash = combine(hash, std::hash<float>{}(value.metrics.popupCornerRadius));
                hash = combine(hash, std::hash<float>{}(value.metrics.tabCornerRadius));
                hash = combine(hash, std::hash<float>{}(value.metrics.scrollbarWidth));
                hash = combine(hash, std::hash<float>{}(value.metrics.grabMinimumSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.splitterWidth));
                hash = combine(hash, std::hash<float>{}(value.metrics.windowTitleHeight));
                hash = combine(hash, std::hash<float>{}(value.metrics.minimumControlWidth));
                hash = combine(hash, std::hash<float>{}(value.metrics.minimumPopupWidth));
                hash = combine(hash, std::hash<float>{}(value.metrics.controlHeight));
                hash = vector(hash, value.metrics.buttonTextAlignment);
                hash = vector(hash, value.metrics.selectableTextAlignment);
                hash = vector(hash, value.metrics.windowTitleAlignment);
                hash = combine(hash, std::hash<float>{}(value.metrics.separatorSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.separatorTextBorderSize));
                hash = vector(hash, value.metrics.separatorTextAlignment);
                hash = vector(hash, value.metrics.separatorTextPadding);
                hash = combine(hash, std::hash<float>{}(value.metrics.tableAngledHeadersAngleDegrees));
                hash = combine(hash, std::hash<float>{}(value.metrics.tableAngledHeadersTextAlignment));
                hash = combine(hash, std::hash<float>{}(value.metrics.treeLinesSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.logarithmicSliderDeadzone));
                hash = combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(value.metrics.colorButtonPosition)));
                hash = combine(hash, std::hash<float>{}(value.behavior.alpha));
                hash = combine(hash, std::hash<float>{}(value.behavior.disabledAlpha));
                hash = color(hash, value.colors.background);
                hash = color(hash, value.colors.panel);
                hash = color(hash, value.colors.button);
                hash = color(hash, value.colors.header);
                hash = color(hash, value.colors.text);
                auto returnedValue = color(hash, value.colors.accent);

                return returnedValue;
            }
        };

        struct ThemeIndexEntry
        {
            size_t hash = 0;
            size_t index = 0;
            uint64_t frame = 0;
        };

        using ThemeIndexVector = Vector<ThemeIndexEntry>;
        using ScopeStateVector = Vector<ScopeState>;
        using NodeIndexVector = Vector<size_t>;
        using NumericStateVector = Vector<Detail::NumericState>;
        using NodeIndexSpan = Span<const size_t>;

        Allocator * allocator = nullptr;
        NullPlatformAdapter nullPlatform;
        PlatformAdapter * platform = &nullPlatform;
        FontProvider * fontProvider = nullptr;
        Input input;
        Array<bool, static_cast<size_t>(KeyCode::Count)> keyDownStates = {};
        Array<float, static_cast<size_t>(KeyCode::Count)> keyDownDurations = {};
        Array<float, 5> pointerDownDurations = {};
        Viewport viewport;
        Configuration configuration;
        bool navigationCursorVisible = true;
        InputCaptureOverride inputCaptureOverride;
        FrameCaptureOptions frameCaptureOptions;
        NextWindowData nextWindow;
        Theme theme = Theme::dark();
        Theme frameTheme = theme;
        const Theme * currentStyle = &frameTheme;
        ThemePtrVector frameStyles;
        ThemeIndexVector frameStyleIndices;
        size_t frameStyleCount = 0;
        Frame frame;
        DrawList drawList;
        NodeVector nodes;
        RecycledNodeStorageVector recycledNodeStorage;
        ColorTextDataVector frameColorTextData;
        size_t frameColorTextDataCount = 0;
        NodeIndexVector layoutChildren;
        FloatVector layoutFloatScratch;
        NodeIndexVector scrollLayoutNodes;
        NodeIndexVector windowRenderOrder;
        IdVector visibleWindowIds;
        IdVector previousVisibleWindowIds;
        PersistentMap persistent;
        TextEditorStateMap textEditorStates;
        ColorEditorStateMap colorEditorStates;
        TabStateMap tabStates;
        NumericStateVector numericStates;
        SizeVector freeNumericStateIndices;
        FrameNodeIndexVector frameNodeIndices;
        ScopeStateVector scopes;
        IdVector previousFocusOrder;
        IdVector menuBarItems;
        IdVector previousMenuBarItems;
        IdVector focusOrder;
        IdVector navigationCandidates;
        SelectionItemVector selectionItems;
        SelectionItemVector previousSelectionItems;
        SelectionRequestVector selectionRequests;
        SelectionOverlayVector selectionOverlays;
        SelectionModel * currentSelectionModel = nullptr;
        IdSpan currentSelectionOrder;
        SelectionOptions currentSelectionOptions;
        Id currentSelectionScope = InvalidId;
        DrawCommandVectorVector frameCanvasCommands;
        size_t frameCanvasCommandCount = 0;
        FrameNodeStringVector frameNodeStrings;
        size_t frameNodeStringCount = 0;
        Id activeBoxSelection = InvalidId;
        SelectionModel * boxSelectionModel = nullptr;
        IdVector boxSelectionOriginal;
        ShortcutRegistry shortcuts;
        Shortcut nextItemShortcut;
        ShortcutOptions nextItemShortcutOptions;
        bool nextItemShortcutPending = false;
        FloatVector itemWidthStack;
        float nextItemWidth = 0.f;
        bool nextItemWidthPending = false;
        DockModel docking;
        DockModelMap dockModels;
        DockLayoutEntryVector dockLayout;
        DockSplitterLayoutEntryVector dockSplitters;
        DockWindowMap visibleDockWindowsScratch;
        DockWindowMap collapsedDockWindowsScratch;
        DockLayoutIndexMap dockLayoutIndices;
        DockAreaMap dockAreas;
        DockSpaceOptionsMap dockSpaceOptions;
        WindowDockGroupMap windowDockGroups;
        UnorderedMap<Id, String> windowLabels;
        TextCache textCache;
        CachedText * textCacheOldest = nullptr;
        CachedText * textCacheNewest = nullptr;
        CachedTextPtrVector transientText;
        size_t transientTextCount = 0;
        TransientTextIndexVector transientTextIndices;
        size_t transientTextIndexCount = 0;
        TextUseHistoryArray textUseHistory;
        const FontProvider * textCacheProvider = nullptr;
        uint64_t textCacheProviderRevision = 0;
        size_t textCacheEntryCount = 0;
        size_t textCacheMemory = 0;
        uint64_t nextPersistentStateSweep = 0;
        size_t frameTextCacheHits = 0;
        size_t frameTextCacheMisses = 0;
        size_t frameTextCacheEvictions = 0;
        RenderState lastRenderState;
        uint64_t lastRenderStateKey = 0;
        RenderStateIndexVector renderStateIndices;
        Rect dockArea;
        Id activeDockSplitter = InvalidId;
        DockNodeId activeDockNode = 0;
        uint32_t activeDockGroup = 0;
        float dockSplitterDragOffset = 0.f;
        Id dockingDragWindow = InvalidId;
        Id dockingTabDragWindow = InvalidId;
        DockNodeId dockingTabDragNode = 0;
        size_t dockingTabDragIndex = 0;
        Vec2 dockingTabDragStart;
        DockNodeId dockingPreviewNode = 0;
        Id dockingPreviewTargetWindow = InvalidId;
        DockPlacement dockingPreviewPlacement = DockPlacement::Center;
        Rect dockingTargetBounds;
        Rect dockingPreviewBounds;
        DragDrop dragDrop;
        PopupStateVector popupStack;
        IdSet popupClosedThisFrame;
        size_t currentParent = 0;
        Id focused = InvalidId;
        Id pointerFocused = InvalidId;
        Id navigationFocused = InvalidId;
        int32_t focusNextOffset = 0;
        bool focusNextPending = false;
        Id active = InvalidId;
        Id captured = InvalidId;
        PointerId capturedPointer = 0;
        Id wheelOwner = InvalidId;
        double wheelOwnerTimestamp = 0.0;
        Modifiers previousModifiers;
        bool menuKeyboardMode = false;
        Id currentInputLayer = InvalidId;
        Id currentWindow = InvalidId;
        Id pointerWindow = InvalidId;
        CursorShape currentCursor = CursorShape::Arrow;
        Id blockingInputLayer = InvalidId;
        Id previousBlockingInputLayer = InvalidId;
        bool popupReplacementAllowed = false;
        bool popupClosedByOutsidePointer = false;
        Id sharedTooltipSource = InvalidId;
        double sharedTooltipVisibleUntil = 0.0;
        bool textLogEnabled = false;
        TextLogTarget textLogTarget = TextLogTarget::Terminal;
        size_t textLogStartNode = 0;
        size_t textLogRootNode = 0;
        size_t textLogMaximumDepth = 2;
        Id textLogWindow = InvalidId;
        bool textLogAutoExpandTrees = true;
        bool textLogIndent = true;
        String textLogFilename;
        String textLogBuffer;
        bool currentDisabled = false;
        bool currentInputBlocked = false;
        bool currentNavigationBlocked = false;
        bool currentLiveEditText = true;
        bool currentLiveEditScalar = true;
        bool activeFrame = false;
        uint64_t nextScopeToken = 1;
        uint64_t anonymousCounter = 1;
        uint64_t nextWindowZOrder = 1;
        double frameStarted = 0.0;

        [[nodiscard]] Persistent & state(Id id);
        [[nodiscard]] Persistent & state(Node & node);
        [[nodiscard]] const Persistent * findState(Id id) const noexcept;
        [[nodiscard]] Detail::TextEditorState & textEditorState(Id id);
        [[nodiscard]] const Detail::TextEditorState * findTextEditorState(Id id) const noexcept;
        [[nodiscard]] Detail::ColorEditorState & colorEditorState(Id id);
        [[nodiscard]] TabState & tabState(Id id);
        [[nodiscard]] Detail::NumericState & numericState(Persistent & persistentState);
        void collectPersistentStateGarbage();
        [[nodiscard]] ColorTextData & ensureColorTextData(Node & node);
        [[nodiscard]] const ColorTextData * findColorTextData(const Node & node) const noexcept;
        [[nodiscard]] Node * findFrameNode(Id id) noexcept;
        [[nodiscard]] const Node * findFrameNode(Id id) const noexcept;
        [[nodiscard]] size_t findFrameNodeIndex(Id id) const noexcept;
        [[nodiscard]] DrawCommandVector & canvasCommands(Node & node);
        [[nodiscard]] FrameNodeStrings & ensureNodeStrings(Node & node);
        [[nodiscard]] String & nodeSemanticValue(Node & node);
        [[nodiscard]] StringView nodeSemanticValue(const Node & node) const noexcept;
        [[nodiscard]] String & nodeSemanticName(Node & node);
        [[nodiscard]] StringView nodeSemanticName(const Node & node) const noexcept;
        [[nodiscard]] String & nodeSemanticDescription(Node & node);
        [[nodiscard]] StringView nodeSemanticDescription(const Node & node) const noexcept;
        [[nodiscard]] const String & nodePath(size_t index);
        [[nodiscard]] const String & nodePath(Node & node);
        void indexFrameNode(Id id, size_t index);
        [[nodiscard]] TableState & tableState(Id id);
        [[nodiscard]] const TableState * findTableState(Id id) const noexcept;
        [[nodiscard]] const Theme * internStyle(const Theme & value);
        [[nodiscard]] Theme & mutableStyle(Node & node);
        [[nodiscard]] float gap(const Node & node) const noexcept;
        [[nodiscard]] Vec2 estimateText(StringView value, const Theme & nodeStyle) const noexcept;
        [[nodiscard]] Vec2 measureText(StringView value, const Theme & nodeStyle) const noexcept;
        void syncTextCache();
        void clearTextCache() noexcept;
        void touchCachedText(CachedText & text) noexcept;
        void unlinkCachedText(CachedText & text) noexcept;
        void trimTextCache(size_t maximumEntries, size_t maximumMemory);
        [[nodiscard]] CachedText * findTransientText(uint64_t key, const TextCacheAttributes & attributes, StringView value) noexcept;
        void indexTransientText(uint64_t key, size_t index);
        [[nodiscard]] const CachedText * findOrCreateText(StringView value, const Theme & nodeStyle, bool editable, bool wordWrap = false, float wrapWidth = 0.f);
        void estimateNodeText(Node & node, StringView value);
        void prepareText(Node & node, StringView value);
        void prepareValueText(Node & node, StringView value);
        void shapeValueText(Node & node);
        void prepareColorChannelText(Node & node, size_t channel, StringView value);
        void shapeColorChannelText(Node & node, size_t channel);
        void prepareVisibleText();
        [[nodiscard]] Id localId(Detail::NodeKind kind, const Key & key, const SourceLocation & location, bool anonymous);
        size_t addNode(Detail::NodeKind kind, const Key & key, StringView label, const LayoutOptions & layout, const SourceLocation & location, SemanticRole semanticRole = SemanticRole::None, bool focusable = false, bool anonymous = false);
        [[nodiscard]] Node acquireNode();
        void recycleFrameNodes();
        [[nodiscard]] Response interact(size_t index, bool keyboardActivation = true, const Rect * interactionBounds = nullptr);
        [[nodiscard]] Response interact(size_t index, const Detail::ItemBehaviorOptions & options, const Rect * interactionBounds = nullptr);
        [[nodiscard]] uint64_t pushScope(size_t node, const Theme * previousStyle, bool previousDisabled);
        void closeScope(uint64_t token) noexcept;
        [[nodiscard]] float resolveDimension(const Dimension & dimension, float measured, float available) const noexcept;
        Vec2 measureNode(size_t index, const Vec2 & available);
        void cullNode(size_t index, const Rect & bounds);
        void arrangeNode(size_t index, const Rect & bounds, const Rect & inheritedClip);
        [[nodiscard]] uint64_t internRenderState(const RenderState & value);
        void emitText(DrawList & drawList, const Node & node, const Vec2 & position, const Color & color, const RenderState & baseState, uint64_t baseKey);
        void emitValueText(DrawList & drawList, const Node & node, const Vec2 & position, const Color & color, const RenderState & baseState, uint64_t baseKey);
        void emitPreparedText(DrawList & drawList, const PreparedTextBatchVector & batches, const Vec2 & position, const Color & color, const RenderState & baseState, uint64_t baseKey, const Vec2 & axisX = {1.f, 0.f}, const Vec2 & axisY = {0.f, 1.f});
        void emitNode(size_t index, DrawList & drawList, CanvasLayer canvasPass = CanvasLayer::Local);
        void updateShortcuts();
    };
} // namespace Mosaic
