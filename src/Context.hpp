#pragma once

#include "Mosaic/Mosaic.hpp"
#include "DrawList.hpp"
#include "FrameArena.hpp"
#include "FrameRenderData.hpp"

#include <functional>
#include <limits>

namespace Mosaic
{
    namespace Detail
    {
        class AllocationTracker final : public Allocator
        {
        public:
            explicit AllocationTracker(Allocator * allocator) noexcept : m_allocator(allocator)
            {
            }

            [[nodiscard]] void * allocate(size_t size, size_t alignment) noexcept override
            {
                if(m_allocator == nullptr)
                {
                    return nullptr;
                }

                void * memory = m_allocator->allocate(size, alignment);

                if(memory != nullptr)
                {
                    ++m_allocationCount;
                    m_allocationBytes += size;
                }

                return memory;
            }

            void deallocate(void * memory, size_t size, size_t alignment) noexcept override
            {
                if(m_allocator == nullptr)
                {
                    return;
                }

                m_allocator->deallocate(memory, size, alignment);
            }

            void reset() noexcept
            {
                m_allocationCount = 0;
                m_allocationBytes = 0;
            }

            [[nodiscard]] Allocator * backingAllocator() const noexcept
            {
                return m_allocator;
            }

            [[nodiscard]] size_t allocationCount() const noexcept
            {
                return m_allocationCount;
            }

            [[nodiscard]] size_t allocationBytes() const noexcept
            {
                return m_allocationBytes;
            }

        private:
            Allocator * m_allocator = nullptr;
            size_t m_allocationCount = 0;
            size_t m_allocationBytes = 0;
        };

        using AllocationTrackerPtr = UniquePtr<AllocationTracker>;

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
            enum class ValueKind : uint8_t
            {
                None,
                Signed,
                Unsigned,
                Floating
            };

            long double dragStartValue = 0.L;
            long double dragValueSpeed = 0.L;
            long double dragAccumulator = 0.L;
            long double dragLastApplied = 0.L;
            uint64_t integralDragStart = 0;
            uint64_t integralDragLastApplied = 0;
            uint64_t integralDragRateWhole = 0;
            uint64_t integralDragRateRemainder = 0;
            uint64_t integralDragRateDenominator = 1;
            int64_t integralDragMotion = 0;
            float sliderGrabOffset = 0.f;
            String temporaryText;
            int64_t lastSignedValue = 0;
            uint64_t lastUnsignedValue = 0;
            long double lastFloatingValue = 0.L;
            ValueKind lastValueKind = ValueKind::None;
            bool initialized = false;
            bool temporaryInput = false;
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

        struct PreparedTextRunBatch
        {
            TextureHandle texture = 0;
            TexturedRectInstanceVector rectangles;
        };

        using PreparedTextRunBatchVector = Vector<PreparedTextRunBatch>;
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
            PreparedTextRunBatchVector batches;
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

        struct TextMeasureCacheEntry
        {
            String text;
            Vec2 size;
            const FontProvider * provider = nullptr;
            uint64_t providerRevision = 0;
            FontHandle font = DefaultFont;
            uint32_t fontSize = 0;
            uint64_t key = 0;
        };

        struct TransientTextIndexEntry
        {
            uint64_t key = 0;
            uint64_t frame = 0;
            size_t index = 0;
        };

        using TextUseHistoryArray = Array<TextUseHistory, 4096>;
        using TextMeasureCache = Vector<TextMeasureCacheEntry>;
        using TransientTextIndexVector = Vector<TransientTextIndexEntry>;
        using ThemePtr = UniquePtr<Theme>;
        using ThemePtrVector = Vector<ThemePtr>;

        struct Persistent;

        struct ScrollNodePayload
        {
            ScrollOptions options;
        };

        struct TableNodePayload
        {
            TableOptions options;
            Id settingsId = InvalidId;
        };

        struct TableItemNodePayload
        {
            TableColumnOptions columnOptions;
            SortDirection sortDirection = SortDirection::None;
            uint32_t sortOrder = 0;
            Array<Color, 2> rowBackgrounds;
            Array<bool, 2> rowBackgroundsEnabled = {false, false};
            Color cellBackground;
            uint32_t row = 0;
            uint32_t column = 0;
            bool cellBackgroundEnabled : 1 = false;
            bool header : 1 = false;
        };

        struct WindowNodePayload
        {
            Rect resizeBounds;
            Rect popupAnchor;
            Vec2 contentSize;
            Vec2 minimumSize;
            Vec2 maximumSize = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
            uint64_t zOrder = 0;
            DockNodeId dockNode = 0;
            uint32_t dockGroup = 0;
            float backgroundAlpha = 1.f;
            PopupPlacement popupPlacement = PopupPlacement::Automatic;
            PopupHorizontalAlignment popupHorizontalAlignment = PopupHorizontalAlignment::Start;
            WindowCollapsePlacement collapsePlacement = WindowCollapsePlacement::Right;
            uint8_t resizeEdges = 0;
            bool titleVisible : 1 = true;
            bool menuBar : 1 = false;
            bool backgroundVisible : 1 = true;
            bool unsavedDocument : 1 = false;
            bool docked : 1 = false;
            bool dockAutoHideTabBar : 1 = false;
            bool collapsed : 1 = false;
            bool closeVisible : 1 = false;
            bool closeHovered : 1 = false;
            bool collapseVisible : 1 = false;
            bool collapseHovered : 1 = false;
            bool resizeHovered : 1 = false;
            bool resizable : 1 = false;
            bool resizeGripVisible : 1 = false;
            bool autoSize : 1 = false;
            bool alwaysAutoResize : 1 = false;
            bool fitContentWidth : 1 = false;
            bool fitContentHeight : 1 = false;
            bool contentSizeExplicit : 1 = false;
            bool scrollable : 1 = false;
            bool popup : 1 = false;
            bool bringToFront : 1 = true;
            bool attachedPopup : 1 = false;
        };

        struct ImageNodePayload
        {
            TextureHandle texture = 0;
            SamplerFilter sampler = SamplerFilter::Linear;
            Rect uv = {0.f, 0.f, 1.f, 1.f};
            Color background;
            Color borderColor;
            float rounding = 0.f;
            float borderSize = 0.f;
            float padding = 2.f;
            bool backgroundEnabled : 1 = false;
        };

        struct TreeNodePayload
        {
            Rect frameBounds;
            Rect frameClip;
            Rect labelClip;
            TreeLineMode lines = TreeLineMode::None;
            bool framed : 1 = false;
            bool leaf : 1 = false;
            bool bullet : 1 = false;
            bool framePadding : 1 = false;
            bool spanLabelWidth : 1 = false;
            bool spanAllColumns : 1 = false;
            bool labelSpanAllColumns : 1 = false;
            bool alignLabelWithCurrentX : 1 = false;
            bool navigationLeftJumpsToParent : 1 = false;
            bool closeVisible : 1 = false;
            bool closeHovered : 1 = false;
        };

        struct TextEditNodePayload
        {
            size_t cursor = 0;
            size_t anchor = 0;
            size_t compositionBegin = 0;
            size_t compositionEnd = 0;
            float scrollX = 0.f;
            float scrollY = 0.f;
            bool multiline : 1 = false;
            bool password : 1 = false;
            bool hint : 1 = false;
            bool numeric : 1 = false;
            bool temporaryNumeric : 1 = false;
        };

        struct NodeDebugPayload
        {
            StringView file;
            StringView function;
            uint64_t identityIntegral = 0;
            uint32_t line = 0;
            IdentityValueKind identityValueKind = IdentityValueKind::Callsite;
        };

        struct ValueNodePayload
        {
            Vec2 valueTextSize;
            size_t colorTextDataIndex = std::numeric_limits<size_t>::max();
            float scalar = 0.f;
            float secondaryScalar = 0.f;
            float textWrapWidth = 0.f;
            float itemWidth = 0.f;
            float splitMinimumFirst = 0.f;
            float splitMinimumSecond = 0.f;
            bool itemWidthRequested : 1 = false;
            bool colorMarkerEnabled : 1 = false;
        };

        struct Node
        {
            Detail::NodeKind kind = Detail::NodeKind::Scope;
            ItemRef item;
            Id id = InvalidId;
            Id parentId = InvalidId;
            Id identityScope = InvalidId;
            Id identityParent = InvalidId;
            Id identityLocal = InvalidId;
            size_t parent = 0;
            size_t firstChild = std::numeric_limits<size_t>::max();
            size_t lastChild = std::numeric_limits<size_t>::max();
            size_t nextSibling = std::numeric_limits<size_t>::max();
            String label;
            String valueText;
            Persistent * persistentState = nullptr;
            LayoutOptions layout;
            const Theme * style = nullptr;
            Vec2 measured;
            Vec2 textSize;
            float baseline = 0.f;
            bool baselineValid : 1 = false;
            Rect bounds;
            Rect content;
            Rect clip;
            Rect visualClip;
            Rect childrenClip;
            Rect explicitClip;
            Response response;
            SemanticRole semanticRole = SemanticRole::None;
            Id inputLayer = InvalidId;
            Id windowOwner = InvalidId;
            uint64_t windowSubmission = 0;
            bool visible : 1 = true;
            bool disabled : 1 = false;
            bool inputBlocked : 1 = false;
            bool navigationBlocked : 1 = false;
            bool focusable : 1 = false;
            bool anonymousIdentity : 1 = false;
            bool sameLine : 1 = false;
            float sameLineOffset = 0.f;
            float sameLineSpacing = -1.f;
            bool alignTextToFramePadding : 1 = false;
            SameLineOptions nextSameLine;
            bool nextSameLinePending : 1 = false;
            bool nextTextAlignToFramePadding : 1 = false;
            bool cursorOverride : 1 = false;
            CursorShape cursor = CursorShape::Arrow;
            bool checked : 1 = false;
            bool selected : 1 = false;
            bool expanded : 1 = false;
            bool idConflict : 1 = false;
            bool wordWrap : 1 = false;
            bool readOnly : 1 = false;
            bool showValuePopup : 1 = false;
            bool showValueOnTrack : 1 = true;
            bool showValueTooltip : 1 = false;
            bool textPrepared : 1 = false;
            bool valueTextPrepared : 1 = true;
            bool scrollLayoutDirty : 1 = false;
            bool scrollResizeHovered : 1 = false;
            bool sliderVertical : 1 = false;
            uint8_t scrollResizeEdges = 0;
            float tooltipDelay = 0.25f;
            Validation validation = Validation::Normal;
            bool menuBar : 1 = false;
            bool menuPopupItem : 1 = false;
            bool menuCheckVisible : 1 = false;
            bool menuSubmenu : 1 = false;
            bool legacyColumns : 1 = false;
            bool fillBackground : 1 = true;
            bool fillHoverBackground : 1 = true;
            bool highlighted : 1 = false;
            bool selectableSpanAllColumns : 1 = false;
            bool arrowButton : 1 = false;
            bool hyperlink : 1 = false;
            bool overrideTextAlignment : 1 = false;
            Vec2 textAlignment;
            bool tabSelectedOverline : 1 = false;
            bool tabLeading : 1 = false;
            bool tabTrailing : 1 = false;
            bool tabCloseVisible : 1 = false;
            bool tabCloseHovered : 1 = false;
            bool tabUnsavedDocument : 1 = false;
            bool comboShowArrow : 1 = true;
            bool comboShowPreview : 1 = true;
            bool comboWidthFitPreview : 1 = false;
            Direction direction = Direction::Right;
            bool colorShowInputs : 1 = false;
            bool colorShowPreview : 1 = true;
            ColorInputMode colorInputMode = ColorInputMode::RgbByte;
            bool colorAlphaBackground : 1 = true;
            bool colorAlphaPreviewHalf : 1 = false;
            bool colorBorder : 1 = true;
            bool colorMarkers : 1 = true;
            bool colorHdr : 1 = false;
            uint8_t colorComponents = 0;
            LabelPlacement labelPlacement = LabelPlacement::Before;
            Color tint = {1.f, 1.f, 1.f, 1.f};
            Color colorMarker;
            ScrollNodePayload * scrollPayload = nullptr;
            TableNodePayload * tablePayload = nullptr;
            TableItemNodePayload * tableItemPayload = nullptr;
            WindowNodePayload * windowPayload = nullptr;
            ImageNodePayload * imagePayload = nullptr;
            TreeNodePayload * treePayload = nullptr;
            TextEditNodePayload * textEditPayload = nullptr;
            NodeDebugPayload * debugPayload = nullptr;
            ValueNodePayload * valuePayload = nullptr;
            const CachedText * textRun = nullptr;
            const CachedText * valueTextRun = nullptr;
            size_t canvasCommandIndex = std::numeric_limits<size_t>::max();
            CanvasLayer canvasLayer = CanvasLayer::Local;
            size_t frameStringIndex = std::numeric_limits<size_t>::max();

            [[nodiscard]] const ScrollOptions & scrollOptions() const noexcept
            {
                static const ScrollOptions DefaultOptions;

                if(scrollPayload == nullptr)
                {
                    return DefaultOptions;
                }

                return scrollPayload->options;
            }

            [[nodiscard]] ScrollOptions & mutableScrollOptions() noexcept
            {
                return scrollPayload->options;
            }

            [[nodiscard]] const TableOptions & tableOptions() const noexcept
            {
                static const TableOptions DefaultOptions;

                if(tablePayload == nullptr)
                {
                    return DefaultOptions;
                }

                return tablePayload->options;
            }

            [[nodiscard]] TableOptions & mutableTableOptions() noexcept
            {
                return tablePayload->options;
            }

            [[nodiscard]] Id tableSettingsId() const noexcept
            {
                Id returnedValue = tablePayload == nullptr ? InvalidId : tablePayload->settingsId;

                return returnedValue;
            }

            void setTableSettingsId(Id settingsId) noexcept
            {
                tablePayload->settingsId = settingsId;
            }

            [[nodiscard]] const TableItemNodePayload & tableItem() const noexcept
            {
                static const TableItemNodePayload DefaultPayload;

                if(tableItemPayload == nullptr)
                {
                    return DefaultPayload;
                }

                return *tableItemPayload;
            }

            [[nodiscard]] TableItemNodePayload & mutableTableItem() noexcept
            {
                return *tableItemPayload;
            }

            [[nodiscard]] const WindowNodePayload & windowData() const noexcept
            {
                static const WindowNodePayload DefaultPayload;

                if(windowPayload == nullptr)
                {
                    return DefaultPayload;
                }

                return *windowPayload;
            }

            [[nodiscard]] WindowNodePayload & mutableWindowData() noexcept
            {
                return *windowPayload;
            }

            [[nodiscard]] const ImageNodePayload & imageData() const noexcept
            {
                static const ImageNodePayload DefaultPayload;

                if(imagePayload == nullptr)
                {
                    return DefaultPayload;
                }

                return *imagePayload;
            }

            [[nodiscard]] ImageNodePayload & mutableImageData() noexcept
            {
                return *imagePayload;
            }

            [[nodiscard]] const TreeNodePayload & treeData() const noexcept
            {
                static const TreeNodePayload DefaultPayload;

                if(treePayload == nullptr)
                {
                    return DefaultPayload;
                }

                return *treePayload;
            }

            [[nodiscard]] TreeNodePayload & mutableTreeData() noexcept
            {
                return *treePayload;
            }

            [[nodiscard]] TextEditNodePayload & textEditData() noexcept
            {
                return *textEditPayload;
            }

            [[nodiscard]] const TextEditNodePayload & textEditData() const noexcept
            {
                static const TextEditNodePayload DefaultPayload;

                if(textEditPayload == nullptr)
                {
                    return DefaultPayload;
                }

                return *textEditPayload;
            }

            [[nodiscard]] const NodeDebugPayload & debugData() const noexcept
            {
                static const NodeDebugPayload DefaultPayload;

                if(debugPayload == nullptr)
                {
                    return DefaultPayload;
                }

                return *debugPayload;
            }

            [[nodiscard]] NodeDebugPayload & mutableDebugData() noexcept
            {
                return *debugPayload;
            }

            [[nodiscard]] const ValueNodePayload & valueData() const noexcept
            {
                static const ValueNodePayload DefaultPayload;

                if(valuePayload == nullptr)
                {
                    return DefaultPayload;
                }

                return *valuePayload;
            }

            [[nodiscard]] ValueNodePayload & valueData() noexcept
            {
                return *valuePayload;
            }
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
            String identityDebugValue;
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
            Id currentRowIdentity = InvalidId;
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

        struct WindowPersistentState
        {
            Rect bounds;
            Rect resizeStartBounds;
            Vec2 dragOffset;
            uint64_t zOrder = 0;
            uint64_t visibleLastFrame = 0;
            uint8_t resizeEdges = 0;
            uint8_t interaction = 0;
            bool initialized : 1 = false;
            bool settingsPolicyInitialized : 1 = false;
            bool saveSettings : 1 = true;
            bool visible : 1 = false;
            bool acceptsInput : 1 = true;
            bool acceptsPointerInput : 1 = true;
            bool acceptsNavigationFocus : 1 = true;
            bool bringToFront : 1 = true;
            bool popup : 1 = false;
            bool contentWidthFitted : 1 = false;
            bool contentHeightFitted : 1 = false;
            bool positionConditionApplied : 1 = false;
            bool sizeConditionApplied : 1 = false;
            bool contentSizeConditionApplied : 1 = false;
            bool collapsedConditionApplied : 1 = false;
            bool collapsedValue : 1 = false;
            bool collapsedInitialized : 1 = false;
            bool resizingDockHost : 1 = false;
            bool dragging : 1 = false;
            bool backgroundMovePending : 1 = false;
        };

        struct ScrollPersistentState
        {
            float value = 0.f;
            float extent = 0.f;
            Vec2 position;
            Vec2 target;
            Vec2 velocity;
            Vec2 range;
            Vec2 contentSize;
            uint64_t animationFrame = 0;
            Orientation orientation = Orientation::Vertical;
            Rect trackBounds;
            Rect thumbBounds;
            Rect verticalTrack;
            Rect verticalThumb;
            Rect horizontalTrack;
            Rect horizontalThumb;
            float dragOffset = 0.f;
            uint8_t draggingAxis = 0;
            Vec2 areaSize;
            Vec2 resizeStartSize;
            uint8_t resizeEdges = 0;
            bool targetInitialized : 1 = false;
            bool toEndX : 1 = false;
            bool toEndY : 1 = false;
            bool draggingScrollbar : 1 = false;
            bool areaSizeInitialized : 1 = false;
            bool resizingArea : 1 = false;
        };

        struct Persistent
        {
            Rect lastBounds;
            Rect lastClip;
            Rect pressBounds;
            Rect pressClip;
            Id lastInputLayer = InvalidId;
            Id windowOwner = InvalidId;
            bool expanded : 1 = false;
            bool expandedInitialized : 1 = false;
            Id autoCloseTree = InvalidId;
            bool editing : 1 = false;
            bool acceptsTabInput : 1 = false;
            Vec2 dragStartPosition;
            Vec2 dragLastPosition;
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
            bool visualInitialized : 1 = false;
            float hoverVisual = 0.f;
            float activeVisual = 0.f;
            float selectionVisual = 0.f;
            float focusVisual = 0.f;
            float scalarVisual = 0.f;
            uint64_t firstFrame = 0;
            uint64_t lastFrame = 0;
            Allocator * allocator = nullptr;
            UniquePtr<WindowPersistentState> window;
            UniquePtr<ScrollPersistentState> scroll;
            UniquePtr<TableState> table;

            [[nodiscard]] WindowPersistentState & windowData()
            {
                if(window == nullptr)
                {
                    window = makeUnique<WindowPersistentState>(*allocator);
                }

                return *window;
            }

            [[nodiscard]] const WindowPersistentState & windowData() const noexcept
            {
                static const WindowPersistentState DefaultState;

                if(window == nullptr)
                {
                    return DefaultState;
                }

                return *window;
            }

            [[nodiscard]] ScrollPersistentState & scrollData()
            {
                if(scroll == nullptr)
                {
                    scroll = makeUnique<ScrollPersistentState>(*allocator);
                }

                return *scroll;
            }

            [[nodiscard]] const ScrollPersistentState & scrollData() const noexcept
            {
                static const ScrollPersistentState DefaultState;

                if(scroll == nullptr)
                {
                    return DefaultState;
                }

                return *scroll;
            }
        };

        struct InteractionScrollTransform
        {
            Id id = InvalidId;
            Vec2 position;
            Rect clip;
        };

        struct InteractionSnapshotItem
        {
            ItemRef item;
            Detail::NodeKind kind = Detail::NodeKind::Scope;
            Id identityParent = InvalidId;
            Id inputLayer = InvalidId;
            Id windowOwner = InvalidId;
            uint64_t windowSubmission = 0;
            Rect capturedBounds;
            Rect capturedClip;
            Rect capturedWindowBounds;
            Rect bounds;
            Rect clip;
            Vec2 textSize;
            ScrollOptions scrollOptions;
            TableOptions tableOptions;
            float textVisibleHeight = 0.f;
            bool animationsEnabled = true;
            size_t scrollTransformBegin = 0;
            size_t scrollTransformCount = 0;
            uint64_t windowZOrder = 0;
            bool visible = false;
            bool disabled = false;
            bool inputBlocked = false;
            bool navigationBlocked = false;
            bool focusable = false;
            bool windowPopup = false;
            bool windowAcceptsPointerInput = true;
        };

        struct InteractionSnapshotIndexEntry
        {
            ItemRef item;
            size_t index = 0;
            uint64_t generation = 0;
        };

        struct WindowFrameInstance
        {
            ItemRef item;
            size_t node = std::numeric_limits<size_t>::max();
        };

        using WindowFrameInstanceVector = Vector<WindowFrameInstance>;

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
            size_t node = 0;
            size_t previousParent = 0;
            bool previousDisabled = false;
            bool previousInputBlocked = false;
            bool previousNavigationBlocked = false;
            bool previousLiveEditText = true;
            bool previousLiveEditScalar = true;
            Id previousInputLayer = InvalidId;
            Id previousWindow = InvalidId;
            uint64_t previousWindowSubmission = 0;
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
        using InteractionSnapshotItemVector = Vector<InteractionSnapshotItem>;
        using InteractionSnapshotIndexVector = Vector<InteractionSnapshotIndexEntry>;
        using InteractionScrollTransformVector = Vector<InteractionScrollTransform>;
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
                hash = combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(value.sampler)));
                hash = combine(hash, std::hash<float>{}(value.clip.x));
                hash = combine(hash, std::hash<float>{}(value.clip.y));
                hash = combine(hash, std::hash<float>{}(value.clip.width));
                hash = combine(hash, std::hash<float>{}(value.clip.height));
                hash = combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(value.blend)));
                hash = combine(hash, std::hash<RenderTargetHandle>{}(value.renderTarget));
                hash = combine(hash, std::hash<uint32_t>{}(value.variant));
                hash = combine(hash, std::hash<float>{}(value.clipRadius));
                hash = combine(hash, std::hash<float>{}(value.linePenumbra));
                hash = combine(hash, std::hash<float>{}(value.fillFeather));
                hash = combine(hash, std::hash<float>{}(value.curveTessellationMaximumError));
                hash = combine(hash, std::hash<float>{}(value.circleTessellationMaximumError));
                hash = combine(hash, std::hash<uint8_t>{}(value.curveQuality));
                hash = combine(hash, std::hash<uint8_t>{}(value.ellipseQuality));
                auto returnedValue = combine(hash, std::hash<uint8_t>{}(value.rectangleQuality));

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
                hash = combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(value.metrics.treeLineMode)));
                hash = combine(hash, std::hash<float>{}(value.metrics.treeLinesSize));
                hash = combine(hash, std::hash<float>{}(value.metrics.treeLinesRounding));
                hash = combine(hash, std::hash<float>{}(value.metrics.imageRounding));
                hash = combine(hash, std::hash<float>{}(value.metrics.imageBorderSize));
                hash = vector(hash, value.metrics.displayWindowPadding);
                hash = vector(hash, value.metrics.displaySafeAreaPadding);
                hash = combine(hash, std::hash<float>{}(value.metrics.logarithmicSliderDeadzone));
                hash = combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(value.metrics.colorButtonPosition)));
                hash = combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(value.metrics.windowMenuButtonPosition)));
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

        Detail::AllocationTrackerPtr allocationTracker;
        Allocator * allocator = nullptr;
        Allocator * previousFrameAllocator = nullptr;
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
        bool itemPickerEnabled = false;
        Id itemPickerHovered = InvalidId;
        Id itemPickerSelected = InvalidId;
        Rect itemPickerHoveredBounds;
        Rect itemPickerSelectedBounds;
        InputCaptureOverride inputCaptureOverride;
        FrameCaptureOptions frameCaptureOptions;
        NextWindowData nextWindow;
        bool mainMenuBarSubmitted = false;
        Theme theme = Theme::dark();
        Theme frameTheme = theme;
        ColorEditOptions defaultColorEditOptions;
        ColorPickerOptions defaultColorPickerOptions;
        float mainFontScale = 1.f;
        const Theme * currentStyle = &frameTheme;
        ThemePtrVector frameStyles;
        ThemeIndexVector frameStyleIndices;
        size_t frameStyleCount = 0;
        Frame frame;
        DrawList drawList;
        DrawCommandStorage drawCommandStorage;
        Detail::FrameArena frameArena;
        ScrollNodePayload fallbackScrollNodePayload;
        TableNodePayload fallbackTableNodePayload;
        TableItemNodePayload fallbackTableItemNodePayload;
        WindowNodePayload fallbackWindowNodePayload;
        ImageNodePayload fallbackImageNodePayload;
        TreeNodePayload fallbackTreeNodePayload;
        TextEditNodePayload fallbackTextEditNodePayload;
        NodeDebugPayload fallbackNodeDebugPayload;
        ValueNodePayload fallbackValueNodePayload;
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
        InteractionSnapshotItemVector interactionSnapshot;
        InteractionSnapshotIndexVector interactionSnapshotIndices;
        InteractionScrollTransformVector interactionScrollTransforms;
        uint64_t interactionSnapshotGeneration = 0;
        WindowFrameInstanceVector windowFrameInstances;
        ScopeStateVector scopes;
        UInt64Vector fontScopeTokens;
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
        Detail::FrameRenderDataPtrVector frameRenderData;
        size_t frameRenderDataCount = 0;
        FrameNodeStringVector frameNodeStrings;
        size_t frameNodeStringCount = 0;
        Id activeBoxSelection = InvalidId;
        SelectionModel * boxSelectionModel = nullptr;
        IdVector boxSelectionOriginal;
        Rect boxSelectionBounds;
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
        mutable TextMeasureCache textMeasureCache;
        const FontProvider * textCacheProvider = nullptr;
        uint64_t textCacheProviderRevision = 0;
        size_t textCacheEntryCount = 0;
        size_t textCacheMemory = 0;
        uint64_t nextPersistentStateSweep = 0;
        size_t frameTextCacheHits = 0;
        size_t frameTextCacheMisses = 0;
        size_t frameTextCacheEvictions = 0;
        mutable size_t frameTextMeasureCacheHits = 0;
        mutable size_t frameTextMeasureCacheMisses = 0;
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
        ItemRef activeItem;
        Id captured = InvalidId;
        ItemRef capturedItem;
        PointerId capturedPointer = 0;
        Id wheelOwner = InvalidId;
        ItemRef wheelOwnerItem;
        double wheelOwnerTimestamp = 0.0;
        Modifiers previousModifiers;
        bool menuKeyboardMode = false;
        Id currentInputLayer = InvalidId;
        Id currentWindow = InvalidId;
        uint64_t currentWindowSubmission = 0;
        Id pointerWindow = InvalidId;
        uint64_t pointerWindowSubmission = 0;
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
        uint64_t nextWindowSubmission = 1;
        uint64_t nextWindowZOrder = 1;
        uint32_t debugBeginCall = 0;
        bool debugBeginOnceConsumed = false;
        double frameStarted = 0.0;

        [[nodiscard]] Persistent & state(Id id);
        [[nodiscard]] Persistent & state(Node & node);
        [[nodiscard]] Persistent * findState(Id id) noexcept;
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
        [[nodiscard]] Node * findFrameNode(const ItemRef & item) noexcept;
        [[nodiscard]] const Node * findFrameNode(const ItemRef & item) const noexcept;
        [[nodiscard]] size_t findFrameNodeIndex(Id id) const noexcept;
        [[nodiscard]] size_t findFrameNodeIndex(const ItemRef & item) const noexcept;
        [[nodiscard]] const InteractionSnapshotItem * findInteractionSnapshot(const ItemRef & item) const noexcept;
        [[nodiscard]] ItemRef addWindowFrameInstance(size_t node, uint64_t submission);
        [[nodiscard]] bool windowHasMultipleFrameInstances(Id id) const noexcept;
        void captureInteractionSnapshot();
        void prepareInteractionSnapshot();
        void updateInteractionScrollAnimations();
        void routeInteractionWheel();
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
        void emitPreparedText(DrawList & drawList, const CachedText & text, const Vec2 & position, const Color & color, const RenderState & baseState, uint64_t baseKey, const Vec2 & axisX = {1.f, 0.f}, const Vec2 & axisY = {0.f, 1.f});
        void emitNode(size_t index, DrawList & drawList, CanvasLayer canvasPass = CanvasLayer::Local);
        void updateShortcuts();
    };
} // namespace Mosaic
