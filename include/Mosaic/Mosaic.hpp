#pragma once

#include "Mosaic/Canvas.hpp"
#include "Mosaic/DragDrop.hpp"
#include "Mosaic/Docking.hpp"
#include "Mosaic/FontProvider.hpp"
#include "Mosaic/Frame.hpp"
#include "Mosaic/GraphicsBridge.hpp"
#include "Mosaic/Persistence.hpp"
#include "Mosaic/Platform.hpp"
#include "Mosaic/RendererAdapter.hpp"
#include "Mosaic/SelectionModel.hpp"
#include "Mosaic/ShortcutRegistry.hpp"

#include <concepts>
#include <limits>

namespace Mosaic
{
    struct Context;

    enum class LabelPlacement : uint8_t
    {
        Before,
        After,
        Hidden
    };

    enum class WindowCollapsePlacement : uint8_t
    {
        Left,
        Right
    };

    enum class Condition : uint8_t
    {
        Always,
        Once,
        FirstUseEver,
        Appearing
    };

    enum class Direction : uint8_t
    {
        Left,
        Right,
        Up,
        Down
    };

    using WindowSizeConstraintCallback = void (*)(Vec2 * desiredSize, const Vec2 & currentSize, void * userData);

    struct Response
    {
        Id id = InvalidId;
        uint32_t flags = 0;

        [[nodiscard]] constexpr bool hovered() const noexcept
        {
            auto returnedValue = test(0);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool active() const noexcept
        {
            auto returnedValue = test(1);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool focused() const noexcept
        {
            auto returnedValue = test(2);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool pressed() const noexcept
        {
            auto returnedValue = test(3);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool released() const noexcept
        {
            auto returnedValue = test(4);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool clicked() const noexcept
        {
            auto returnedValue = test(5);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool changed() const noexcept
        {
            auto returnedValue = test(6);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool editing() const noexcept
        {
            auto returnedValue = test(7);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool committed() const noexcept
        {
            auto returnedValue = test(8);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool canceled() const noexcept
        {
            auto returnedValue = test(9);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool doubleClicked() const noexcept
        {
            auto returnedValue = test(10);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool repeated() const noexcept
        {
            auto returnedValue = test(11);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool toggledOpen() const noexcept
        {
            auto returnedValue = test(12);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool disabled() const noexcept
        {
            auto returnedValue = test(13);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool activated() const noexcept
        {
            auto returnedValue = test(14);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool deactivated() const noexcept
        {
            auto returnedValue = test(15);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool deactivatedAfterEdit() const noexcept
        {
            auto returnedValue = test(16);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool toggledSelection() const noexcept
        {
            auto returnedValue = test(17);

            return returnedValue;
        }

        [[nodiscard]] constexpr bool submitted() const noexcept
        {
            auto returnedValue = test(18);

            return returnedValue;
        }

    private:
        [[nodiscard]] constexpr bool test(unsigned bit) const noexcept
        {
            auto returnedValue = (flags & (1U << bit)) != 0;

            return returnedValue;
        }
    };

    struct ItemQueryOptions
    {
        bool allowWhenBlockedByPopup = false;
        bool allowWhenBlockedByActiveItem = false;
        bool allowWhenOverlappedByItem = false;
        bool allowWhenOverlappedByWindow = false;
        bool allowWhenDisabled = false;
        bool stationary = false;
        float delay = 0.f;
    };

    struct DragDropSourceOptions
    {
        float threshold = -1.f;
        bool previewVisible = true;
    };

    struct DragDropTargetOptions
    {
        bool acceptBeforeDelivery = false;
        bool drawDefaultHighlight = true;
        bool suppressSourcePreview = false;
    };

    struct DragDropAcceptResult
    {
        DragPayload payload;
        bool preview = false;
        bool delivery = false;

        [[nodiscard]] constexpr bool accepted() const noexcept
        {
            return preview || delivery;
        }
    };

    struct ScopeQueryOptions
    {
        bool includeDescendants = false;
        bool rootScope = false;
        bool anyScope = false;
        bool includePopupHierarchy = true;
        bool includeDockHierarchy = false;
        bool allowWhenBlockedByPopup = false;
        bool allowWhenBlockedByActiveItem = false;
        bool allowWhenOverlappedByWindow = false;
        bool stationary = false;
        float delay = 0.f;
    };

    struct FrameCaptureOptions
    {
        bool semantics = false;
        bool debug = false;
        bool metrics = false;
    };

    enum class TextLogTarget : uint8_t
    {
        Terminal,
        File,
        Clipboard
    };

    struct TextLogOptions
    {
        size_t maximumDepth = 2;
        bool autoExpandTrees = true;
        bool indent = true;
        bool currentWindowOnly = true;
    };

    struct Configuration
    {
        bool pointerInput = true;
        bool keyboardInput = true;
        bool keyboardNavigation = true;
        bool navigationCapturesKeyboard = true;
        bool navigationCursorVisibleAuto = true;
        bool navigationCursorVisibleAlways = false;
        bool escapeClearsItemFocus = true;
        bool escapeClearsWindowFocus = false;
        bool dockingEnabled = true;
        bool dockingNoSplit = false;
        bool dockingNoMerge = false;
        bool dockingNoResize = false;
        bool dockingNoUndocking = false;
        bool dockingWithShift = false;
        bool cursorChanges = true;
        bool windowResizeFromEdges = true;
        bool windowMoveFromTitleBarOnly = false;
        bool scrollbarScrollByPage = true;
        bool inputTextCursorBlink = true;
        bool inputTextEnterKeepActive = false;
        bool dragClickToInputText = false;
        bool windowCopyContentsWithPrimaryC = false;
    };

    struct SliderOptions
    {
        Dimension width = Dimension::fixed(200.f);
        double minimum = 0.0;
        double maximum = 1.0;
        double step = 0.0;
        // Value units applied per physical pointer pixel. Zero selects a range-aware speed.
        double dragSpeed = 0.0;
        int precision = 3;
        bool showValuePopup = false;
        bool showValueOnTrack = true;
        bool showValueTooltip = false;
        float valueTooltipDelay = 0.25f;
        bool logarithmic = false;
        bool temporaryInput = true;
        bool wrapAround = false;
        bool clampInput = false;
        bool clampZeroRange = false;
        bool roundToFormat = true;
        bool speedTweaks = true;
        bool colorMarkers = false;
        LabelPlacement labelPlacement = LabelPlacement::Before;
        StringView format;
        Validation validation = Validation::Normal;
        StringView validationMessage;
    };

    struct RangeOptions
    {
        SliderOptions minimum;
        SliderOptions maximum;
    };

    enum class ButtonPressPolicy : uint8_t
    {
        Release,
        Press,
        DoubleClick
    };

    struct ButtonOptions
    {
        Dimension width = SizeRule::Content;
        Dimension height = SizeRule::Content;
        PointerButton pointerButton = PointerButton::Primary;
        ButtonPressPolicy pressPolicy = ButtonPressPolicy::Release;
        bool keyboardActivation = true;
        bool repeat = false;
        bool fillBackground = true;
        bool allowOverlap = false;
    };

    struct SelectableOptions
    {
        Dimension width = SizeRule::Fill;
        Dimension height = SizeRule::Content;
        PointerButton pointerButton = PointerButton::Primary;
        ButtonPressPolicy pressPolicy = ButtonPressPolicy::Release;
        bool keyboardActivation = true;
        bool allowOverlap = false;
        bool allowDoubleClick = false;
        bool selectOnNavigation = false;
        bool autoClosePopups = true;
        bool spanAllColumns = false;
        bool highlight = false;
        bool overrideTextAlignment = false;
        Vec2 textAlignment = {0.f, 0.5f};
    };

    enum class NumericBase : uint8_t
    {
        Decimal = 10,
        Hexadecimal = 16
    };

    struct NumericInputOptions
    {
        double step = 0.0;
        double fastStep = 0.0;
        int precision = 3;
        bool scientific = false;
        bool parseEmptyAsZero = false;
        bool displayZeroAsEmpty = false;
        bool readOnly = false;
        NumericBase base = NumericBase::Decimal;
        uint8_t minimumDigits = 0;
    };

    enum class ScrollAxes : uint8_t
    {
        Horizontal,
        Vertical,
        Both
    };

    enum class ScrollbarVisibility : uint8_t
    {
        Automatic,
        Always,
        Hidden
    };

    struct ScrollOptions
    {
        ScrollAxes axes = ScrollAxes::Vertical;
        Orientation contentOrientation = Orientation::Vertical;
        ScrollbarVisibility visibility = ScrollbarVisibility::Automatic;
        float wheelStep = 1.f;
        float smoothDuration = 0.14f;
        bool keyboard = true;
        bool nested = false;
        bool smooth = true;
        bool background = false;
        bool framed = false;
        bool frameStyle = false;
        bool autoResizeY = false;
        bool resizeX = false;
        bool resizeY = false;
        Vec2 minimumSize = {64.f, 48.f};
        Vec2 maximumSize = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    };

    struct WindowOptions
    {
        Rect initialBounds = {40.f, 40.f, 520.f, 420.f};
        bool * open = nullptr;
        bool * collapsed = nullptr;
        bool movable = true;
        bool resizable = true;
        bool resizeHorizontal = true;
        bool resizeVertical = true;
        bool navigation = true;
        bool input = true;
        bool bringToFront = true;
        bool focusOnAppearing = false;
        bool saveSettings = true;
        bool dockable = true;
        bool dockAutoHideTabBar = false;
        // Zero keeps the window floating. Windows with the same non-zero group
        // may dock together; windows from different groups never become targets.
        uint32_t dockGroup = 1;
        bool titleBar = true;
        bool background = true;
        float backgroundAlpha = 1.f;
        bool unsavedDocument = false;
        bool padding = true;
        bool fitContentWidth = false;
        bool fitContentHeight = false;
        bool scrollable = false;
        ScrollOptions scroll;
        WindowCollapsePlacement collapsePlacement = WindowCollapsePlacement::Right;
        Vec2 minimumSize = {120.f, 72.f};
        Vec2 maximumSize = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        WindowSizeConstraintCallback sizeConstraint = nullptr;
        void * sizeConstraintUserData = nullptr;
    };

    struct DockSpaceOptions
    {
        uint32_t group = 1;
        bool noSplit = false;
        bool noMerge = false;
        bool noResize = false;
        bool noUndocking = false;
        bool noDockingOverCentralNode = false;
        bool passthroughCentral = false;
        bool autoHideTabBar = false;
        bool background = true;
        Color backgroundColor;
    };

    struct ListClipperOptions
    {
        Orientation orientation = Orientation::Vertical;
        // Negative uses the current style gap. Item extent excludes this spacing.
        float spacing = -1.f;
        size_t overscan = 2;
    };

    enum class TreeLineMode : uint8_t
    {
        None,
        Full,
        ToNodes
    };

    struct TreeNodeOptions
    {
        bool defaultExpanded = false;
        bool leaf = false;
        bool bullet = false;
        bool selected = false;
        bool autoCloseChildNodes = false;
        bool openOnDoubleClick = false;
        bool openOnArrow = false;
        bool spanAvailableWidth = false;
        bool spanFullWidth = false;
        bool spanLabelWidth = false;
        bool spanAllColumns = false;
        bool labelSpanAllColumns = false;
        bool allowOverlap = false;
        bool framed = false;
        bool framePadding = false;
        bool alignLabelWithCurrentX = false;
        bool navigationLeftJumpsToParent = false;
        TreeLineMode lines = TreeLineMode::None;
        ButtonPressPolicy pressPolicy = ButtonPressPolicy::Release;
    };

    struct ImageButtonOptions
    {
        Rect uv = {0.f, 0.f, 1.f, 1.f};
        Color tint = {1.f, 1.f, 1.f, 1.f};
        Color background = {0.f, 0.f, 0.f, 1.f};
        float padding = 2.f;
        bool backgroundEnabled = false;
    };

    struct ImageOptions
    {
        Rect uv = {0.f, 0.f, 1.f, 1.f};
        Color tint = {1.f, 1.f, 1.f, 1.f};
        Color background = {0.f, 0.f, 0.f, 0.f};
        float padding = 0.f;
        bool backgroundEnabled = false;
    };

    enum class PopupPlacement : uint8_t
    {
        Automatic,
        Below,
        Above,
        Left,
        Right,
        Cursor,
        Center
    };

    enum class PopupHorizontalAlignment : uint8_t
    {
        Start,
        End
    };

    struct PopupOptions
    {
        Id owner = InvalidId;
        Rect anchor;
        PopupPlacement placement = PopupPlacement::Automatic;
        PopupHorizontalAlignment horizontalAlignment = PopupHorizontalAlignment::Start;
        Vec2 minimumSize = {144.f, 0.f};
        Vec2 maximumSize = {420.f, 480.f};
        bool closeOnEscape = true;
        bool closeOnClickOutside = true;
        bool closeOnSelection = true;
        bool allowSiblingOwners = false;
        bool modal = false;
        bool titleBar = false;
        bool movable = false;
        bool resizable = false;
        Color backdropColor = {0.f, 0.f, 0.f, 0.58f};
    };

    enum class ComboPopupHeight : uint8_t
    {
        Small,
        Regular,
        Large,
        Largest
    };

    struct ComboOptions
    {
        Dimension width = Dimension::fixed(200.f);
        LabelPlacement labelPlacement = LabelPlacement::After;
        ComboPopupHeight popupHeight = ComboPopupHeight::Regular;
        bool popupAlignLeft = false;
        bool showArrow = true;
        bool showPreview = true;
        bool widthFitPreview = false;
    };

    struct ListBoxOptions
    {
        LayoutOptions layout;
        int highlighted = -1;
        int * hovered = nullptr;
    };

    using ComboItemGetter = StringView (*)(void * userData, size_t index);

    struct MenuItemOptions
    {
        StringView shortcut;
        bool * checked = nullptr;
        bool selected = false;
        bool enabled = true;
        bool closeOnActivate = true;
    };

    struct MenuOptions
    {
        float width = 0.f;
        bool fillBackground = false;
    };

    struct MenuBarOptions
    {
        Dimension width = SizeRule::Fill;
        bool fillBackground = true;
    };

    enum class ColorInputMode : uint8_t
    {
        RgbByte,
        RgbFloat,
        HsvFloat,
        Hexadecimal
    };

    struct ColorEditOptions
    {
        Dimension width = SizeRule::Content;
        Dimension height = SizeRule::Content;
        LabelPlacement labelPlacement = LabelPlacement::Before;
        bool showInputs = false;
        bool showPreview = true;
        bool tooltip = true;
        bool picker = true;
        bool smallPreview = true;
        bool label = true;
        ColorInputMode inputMode = ColorInputMode::RgbByte;
        bool alphaOpaque = false;
        bool alphaBackground = true;
        bool alphaPreviewHalf = false;
        bool optionsMenu = true;
        bool dragDrop = true;
        bool colorMarkers = true;
        bool hdr = false;
        bool border = true;
        bool inputIsHsv = false;
        bool openPickerOnClick = true;
    };

    struct ColorPickerOptions
    {
        Vec2 size = {320.f, 180.f};
        const Color * reference = nullptr;
        bool alpha = true;
        bool hueWheel = false;
        bool rotateHueWheelTriangle = true;
        bool showInputs = true;
        bool showPreview = true;
        ColorInputMode inputMode = ColorInputMode::RgbByte;
        bool alphaOpaque = false;
        bool alphaBackground = true;
        bool alphaPreviewHalf = false;
        bool optionsMenu = true;
        bool dragDrop = true;
        bool colorMarkers = true;
        bool hdr = false;
        bool border = true;
        bool inputIsHsv = false;
    };

    struct PlotOptions
    {
        Dimension width = SizeRule::Fill;
        float height = 80.f;
        float minimum = 0.f;
        float maximum = 1.f;
        size_t offset = 0;
        StringView overlay;
    };

    using PlotValueGetter = float (*)(void * userData, size_t index);

    struct HelpMarkerOptions
    {
        StringView marker = "(?)";
        // A non-positive component uses the corresponding viewport extent.
        Vec2 tooltipSize = {320.f, 0.f};
        // A negative value uses Theme::behavior.tooltipHoverDelay.
        float tooltipDelay = -1.f;
    };

    struct ItemTooltipOptions
    {
        // A non-positive component uses the corresponding viewport extent.
        Vec2 maximumSize = {240.f, 0.f};
        // A negative value uses the normal or stationary delay from the theme.
        float delay = -1.f;
        bool stationary = false;
        bool allowWhenDisabled = true;
        // Reuse a recently opened tooltip delay while moving between nearby items.
        bool sharedDelay = true;
        // Keyboard/navigation focus may expose the same help as pointer hover.
        bool navigationFocus = true;
    };

    enum class TableSizing : uint8_t
    {
        Fixed,
        Stretch,
        FixedFit,
        StretchProportional,
        FixedSame,
        StretchSame
    };

    enum class TableColumnIndent : uint8_t
    {
        Default,
        Enable,
        Disable
    };

    enum class SortDirection : uint8_t
    {
        None,
        Ascending,
        Descending
    };

    enum class TableBackgroundTarget : uint8_t
    {
        Row0,
        Row1
    };

    using TableContextMenuCallback = void (*)(Context * ui, Id table, uint32_t column, void * userData);

    struct TableOptions
    {
        // Non-zero lets independent table instances share persistent column state.
        Id settingsId = InvalidId;
        bool headers = true;
        bool resizable = true;
        bool reorderable = true;
        bool hideable = true;
        bool sortable = false;
        bool multiSort = false;
        bool rowSelection = false;
        bool rowBackground = false;
        bool bordersInnerHorizontal = false;
        bool bordersInnerVertical = false;
        bool bordersOuterHorizontal = false;
        bool bordersOuterVertical = false;
        bool clipCells = true;
        bool preciseWidths = false;
        bool highlightHoveredColumn = false;
        bool padOuterHorizontal = false;
        bool padInnerHorizontal = true;
        bool saveSettings = true;
        bool contextMenuInBody = false;
        bool bordersInBody = true;
        bool bordersInBodyUntilResize = false;
        bool extendHostHorizontal = true;
        bool extendHostVertical = true;
        bool keepColumnsVisible = true;
        bool sortTristate = false;
        bool scrollHorizontal = false;
        bool scrollVertical = false;
        float innerWidth = 0.f;
        float rowMinimumHeight = 0.f;
        uint32_t frozenRows = 0;
        uint32_t frozenColumns = 0;
        TableContextMenuCallback contextMenu = nullptr;
        void * contextMenuUserData = nullptr;
    };

    struct TableColumnOptions
    {
        TableSizing sizing = TableSizing::Stretch;
        float widthOrWeight = 1.f;
        // Per-column width for item widgets. Zero uses the normal item-width
        // stack, positive values are fixed, negative values offset from the
        // available cell width.
        float itemWidth = 0.f;
        bool enabled = true;
        bool visible = true;
        bool sortable = true;
        bool sortAscending = true;
        bool sortDescending = true;
        bool resizable = true;
        bool reorderable = true;
        bool hideable = true;
        bool clip = true;
        bool defaultSort = false;
        SortDirection preferredSort = SortDirection::Ascending;
        bool headerLabelVisible = true;
        bool headerContributesToWidth = true;
        bool angledHeader = false;
        TableColumnIndent indent = TableColumnIndent::Default;
    };

    struct TableRowOptions
    {
        float minimumHeight = 0.f;
        // Negative keeps the table style. A non-negative value pads this row only.
        float cellPaddingY = -1.f;
    };

    struct TableSortSpec
    {
        uint32_t column = 0;
        SortDirection direction = SortDirection::None;
        uint32_t order = 0;
    };

    using TableSortSpecVector = Vector<TableSortSpec>;
    using TableSortSpecSpan = Span<const TableSortSpec>;

    struct TableSortState
    {
        TableSortSpecSpan specifications;
        bool dirty = false;
    };

    struct TableColumnStatus
    {
        bool enabled = false;
        bool visible = false;
        bool sorted = false;
        bool hovered = false;
        SortDirection sortDirection = SortDirection::None;
        uint32_t displayOrder = 0;
        float width = 0.f;
    };

    struct ColumnsOptions
    {
        bool border = true;
        bool horizontalBorders = false;
        bool resizable = true;
        bool clip = true;
        float minimumWidth = 24.f;
    };

    struct PropertyGridOptions
    {
        float valueColumnWeight = 2.25f;
        float labelColumnWeight = 1.f;
        bool showHeaders = true;
        StringView valueHeader = "Value";
        StringView labelHeader = "label";
    };

    enum class TextCharacterFilter : uint8_t
    {
        None,
        Decimal,
        Scientific,
        Hexadecimal,
        Uppercase,
        NoBlank,
        CasingSwap,
        LowercaseOnly
    };

    enum class TextInputCallbackEvent : uint8_t
    {
        CharacterFilter,
        Completion,
        HistoryPrevious,
        HistoryNext,
        Edit,
        Always,
        Resize
    };

    struct TextInputCallbackData
    {
        TextInputCallbackEvent event = TextInputCallbackEvent::Edit;
        String * value = nullptr;
        size_t capacity = 0;
        size_t previousCapacity = 0;
        size_t cursor = 0;
        size_t anchor = 0;
        void * userData = nullptr;
        char32_t character = U'\0';
        bool reject = false;
        bool changed = false;
    };

    using TextInputCallback = void (*)(TextInputCallbackData & data);

    struct TextInputOptions
    {
        bool password = false;
        bool numeric = false;
        bool readOnly = false;
        bool selectAllOnFocus = false;
        bool enterReturnsTrue = false;
        bool escapeClearsAll = false;
        bool undoRedo = true;
        bool tabStop = true;
        bool allowTabInput = false;
        bool controlEnterForNewLine = false;
        bool wordWrap = false;
        bool noHorizontalScroll = false;
        bool alwaysOverwrite = false;
        bool elideLeft = false;
        bool callbackCompletion = false;
        bool callbackHistory = false;
        bool callbackCharacterFilter = false;
        bool callbackEdit = false;
        bool callbackAlways = false;
        // Invoked only when the editable MosaicString changes capacity.
        bool callbackResize = false;
        size_t maximumBytes = 0;
        TextCharacterFilter characterFilter = TextCharacterFilter::None;
        TextInputCallback callback = nullptr;
        void * callbackUserData = nullptr;
        StringView hint;
        Validation validation = Validation::Normal;
        StringView validationMessage;
    };

    struct LiveEditOptions
    {
        bool text = true;
        bool scalar = true;
    };

    struct TextOptions
    {
        LayoutOptions layout;
        bool wordWrap = false;
    };

    using TabBarContentCallback = void (*)(Context * ui, void * userData);

    struct TabItemOptions
    {
        bool leading = false;
        bool trailing = false;
        bool setSelected = false;
        bool reorderable = true;
        bool closeButton = true;
        bool closeWithMiddleMouse = true;
        bool unsavedDocument = false;
        bool noTooltip = false;
        bool disabled = false;
    };

    using TabItemOptionsSpan = Span<const TabItemOptions>;

    enum class TabFittingPolicy : uint8_t
    {
        Mixed,
        Shrink,
        Scroll
    };

    struct TabsOptions
    {
        bool reorderable = false;
        bool autoSelectNewTabs = false;
        bool tabListPopupButton = false;
        bool closeWithMiddleMouse = true;
        bool selectedOverline = false;
        TabFittingPolicy fittingPolicy = TabFittingPolicy::Mixed;
        // Compatibility aliases. fitAvailableWidth forces Shrink; disabling
        // scrollToFit prevents Mixed and Scroll from creating a scroll region.
        bool fitAvailableWidth = false;
        bool scrollToFit = true;
        bool scrollingButtons = true;
        bool tooltips = true;
        TabBarContentCallback leadingContent = nullptr;
        TabBarContentCallback trailingContent = nullptr;
        void * contentUserData = nullptr;
    };

    struct TabItemButtonOptions
    {
        Dimension width = SizeRule::Content;
        bool repeat = false;
    };

    enum class BoxSelectionMode : uint8_t
    {
        OneDimensional,
        TwoDimensional
    };

    enum class SelectionPressPolicy : uint8_t
    {
        // Select unselected items on press and selected items on release. This
        // keeps an existing multi-selection available for drag and drop.
        Automatic,
        Press,
        Release
    };

    enum class SelectionRequestType : uint8_t
    {
        Clear,
        SetAll,
        SetItem,
        SetRange
    };

    struct SelectionRequest
    {
        SelectionRequestType type = SelectionRequestType::SetItem;
        Id scope = InvalidId;
        Id item = InvalidId;
        Id anchor = InvalidId;
        bool selected = true;
        bool clearFirst = false;
    };

    using SelectionRequestVector = Vector<SelectionRequest>;
    using SelectionRequestSpan = Span<const SelectionRequest>;

    enum class SelectionScope : uint8_t
    {
        Rect,
        Window
    };

    struct BoxSelectionOptions
    {
        bool enabled = true;
        bool clearOnClick = true;
        bool allowFromSelectedItems = false;
        bool noScroll = false;
        BoxSelectionMode mode = BoxSelectionMode::TwoDimensional;
    };

    struct SelectionOptions
    {
        bool singleSelect = false;
        bool selectAll = true;
        bool rangeSelect = true;
        bool autoSelect = true;
        bool autoClear = true;
        bool autoClearOnReselect = true;
        bool selectOnRightClick = true;
        bool clearOnEscape = true;
        bool navigationWrapX = false;
        bool applyRequests = true;
        SelectionScope scope = SelectionScope::Rect;
        SelectionPressPolicy pressPolicy = SelectionPressPolicy::Automatic;
        SelectableOptions item;
    };

    struct SplitOptions
    {
        float minimumFirst = 80.f;
        float minimumSecond = 80.f;
        // Zero disables snapping. Splitters with the same non-zero index snap to each other.
        uint32_t snapIndex = 1;
    };

    [[nodiscard]] Context * newContext(PlatformAdapter * platform = nullptr, FontProvider * fontProvider = nullptr);
    void deleteContext(Context * ui) noexcept;

    void beginFrame(Context * ui, const Input & input, const Viewport & viewport = {});
    void setFrameCaptureOptions(Context * ui, const FrameCaptureOptions & options) noexcept;
    void setConfiguration(Context * ui, const Configuration & configuration) noexcept;
    [[nodiscard]] bool getConfiguration(const Context * ui, Configuration * const _out) noexcept;
    [[nodiscard]] const Frame & endFrame(Context * ui);
    [[nodiscard]] const Frame & getFrame(const Context * ui) noexcept;
    [[nodiscard]] bool frameActive(const Context * ui) noexcept;
    void beginTextLog(Context * ui, TextLogTarget target, StringView filename = "mosaic_log.txt");
    void beginTextLog(Context * ui, TextLogTarget target, const TextLogOptions & options, StringView filename = "mosaic_log.txt");
    void logText(Context * ui, StringView text);
    [[nodiscard]] bool textLogActive(const Context * ui) noexcept;
    void setInputCaptureOverride(Context * ui, const InputCaptureOverride & overrideValue) noexcept;

    void setTheme(Context * ui, const Theme & theme);
    [[nodiscard]] const Theme & getTheme(const Context * ui) noexcept;
    void setPlatformAdapter(Context * ui, PlatformAdapter * platform) noexcept;
    void setFontProvider(Context * ui, FontProvider * fontProvider) noexcept;
    void setNextWindowPosition(Context * ui, const Vec2 & position, Condition condition = Condition::Always) noexcept;
    void setNextWindowSize(Context * ui, const Vec2 & size, Condition condition = Condition::Always) noexcept;
    void setNextWindowContentSize(Context * ui, const Vec2 & size, Condition condition = Condition::Always) noexcept;
    void setNextWindowCollapsed(Context * ui, bool collapsed, Condition condition = Condition::Always) noexcept;
    void setNextWindowFocus(Context * ui) noexcept;

    [[nodiscard]] Scope scope(Context * ui, const Key & key, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] WindowScope window(Context * ui, StringView label, const WindowOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] WindowScope window(Context * ui, const Key & key, StringView label, const WindowOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope row(Context * ui, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope column(Context * ui, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope grid(Context * ui, uint32_t columns, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope overlay(Context * ui, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope scrollArea(Context * ui, StringView label, Orientation orientation = Orientation::Vertical, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope scrollArea(Context * ui, StringView label, const ScrollOptions & scrollOptions, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Canvas dockSpace(Context * ui, const Key & key, StringView label, const DockSpaceOptions & options = {}, const LayoutOptions & layout = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Canvas dockSpace(Context * ui, StringView label, const DockSpaceOptions & options = {}, const LayoutOptions & layout = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope split(Context * ui, StringView label, Orientation orientation, float ratio = 0.5f, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope split(Context * ui, StringView label, Orientation orientation, float * ratio, const SplitOptions & splitOptions = {}, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope absolute(Context * ui, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope clip(Context * ui, const Rect & rect, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope disabledScope(Context * ui, bool disabled = true, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope interactionScope(Context * ui, bool enabled = true, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope liveEditScope(Context * ui, const LiveEditOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope styleScope(Context * ui, const Theme & theme, const SourceLocation & location = SourceLocation::current());

    // Positive values request a fixed width. Negative values consume the available
    // width while leaving the absolute value on the right; -FLT_MIN fills it.
    // setNextItemWidth affects the next width-aware widget, while push/pop provide
    // a scoped default for consecutive widgets.
    void setNextItemWidth(Context * ui, float width) noexcept;
    void pushItemWidth(Context * ui, float width);
    void popItemWidth(Context * ui) noexcept;

    Response text(Context * ui, StringView value, const SourceLocation & location = SourceLocation::current());
    Response text(Context * ui, StringView value, const TextOptions & options, const SourceLocation & location = SourceLocation::current());
    Response bullet(Context * ui, const SourceLocation & location = SourceLocation::current());
    Response bulletText(Context * ui, StringView value, const SourceLocation & location = SourceLocation::current());
    Response button(Context * ui, StringView label, const SourceLocation & location = SourceLocation::current());
    Response button(Context * ui, const Key & key, StringView label, const SourceLocation & location = SourceLocation::current());
    Response button(Context * ui, const Key & key, StringView label, const ButtonOptions & options, const SourceLocation & location = SourceLocation::current());
    Response smallButton(Context * ui, StringView label, const SourceLocation & location = SourceLocation::current());
    Response smallButton(Context * ui, const Key & key, StringView label, const SourceLocation & location = SourceLocation::current());
    Response invisibleButton(Context * ui, const Key & key, const Vec2 & size, const ButtonOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response arrowButton(Context * ui, const Key & key, Direction direction, const ButtonOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response hyperlink(Context * ui, StringView label, StringView url = {}, const SourceLocation & location = SourceLocation::current());
    Response labelText(Context * ui, StringView label, StringView value, const SourceLocation & location = SourceLocation::current());
    Response iconButton(Context * ui, const Key & key, TextureHandle texture, StringView description = {}, const SourceLocation & location = SourceLocation::current());
    Response checkbox(Context * ui, StringView label, bool * value, const SourceLocation & location = SourceLocation::current());
    Response checkbox(Context * ui, const Key & key, StringView label, bool * value, const SourceLocation & location = SourceLocation::current());
    Response radioButton(Context * ui, StringView label, bool selected, const SourceLocation & location = SourceLocation::current());
    Response selectable(Context * ui, StringView label, bool selected = false, const SourceLocation & location = SourceLocation::current());
    Response selectable(Context * ui, const Key & key, StringView label, bool selected = false, const SourceLocation & location = SourceLocation::current());
    Response selectable(Context * ui, const Key & key, StringView label, bool selected, const SelectableOptions & options, const SourceLocation & location = SourceLocation::current());
    Response selectable(Context * ui, const Key & key, StringView label, SelectionModel * selection, Id item, IdSpan orderedItems = {}, const SourceLocation & location = SourceLocation::current());
    Response selectable(Context * ui, const Key & key, StringView label, SelectionModel * selection, Id item, IdSpan orderedItems, const SelectionOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope multiSelect(Context * ui, const Key & key, SelectionModel * selection, IdSpan orderedItems, const SelectionOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] SelectionRequestSpan selectionRequests(const Context * ui) noexcept;
    void applySelectionRequests(SelectionModel * selection, IdSpan orderedItems, SelectionRequestSpan requests, Id scope = InvalidId);
    Response selectable(Context * ui, const Key & key, StringView label, Id item, const SourceLocation & location = SourceLocation::current());
    Response boxSelect(Context * ui, Id surface, SelectionModel * selection, const BoxSelectionOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response toggle(Context * ui, StringView label, bool * value, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, double * value, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response verticalSlider(Context * ui, StringView label, float * value, float minimum, float maximum, const Vec2 & size = {24.f, 160.f}, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response verticalSlider(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, const Vec2 & size = {24.f, 160.f}, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response progressBar(Context * ui, float value, StringView label = {}, const SourceLocation & location = SourceLocation::current());
    Response plotLines(Context * ui, StringView label, ConstFloatSpan values, const PlotOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response plotLines(Context * ui, StringView label, size_t valueCount, PlotValueGetter getter, void * userData, const PlotOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response plotHistogram(Context * ui, StringView label, ConstFloatSpan values, const PlotOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response plotHistogram(Context * ui, StringView label, size_t valueCount, PlotValueGetter getter, void * userData, const PlotOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response separator(Context * ui, const SourceLocation & location = SourceLocation::current());
    Response separatorText(Context * ui, StringView label, const SourceLocation & location = SourceLocation::current());
    Response spacer(Context * ui, float size, const SourceLocation & location = SourceLocation::current());
    Response image(Context * ui, TextureHandle texture, const Vec2 & size, const Rect & uv = {0.f, 0.f, 1.f, 1.f}, const Color & tint = {1.f, 1.f, 1.f, 1.f}, const SourceLocation & location = SourceLocation::current());
    Response image(Context * ui, TextureHandle texture, const Vec2 & size, const ImageOptions & options, const SourceLocation & location = SourceLocation::current());
    Response imageButton(Context * ui, const Key & key, TextureHandle texture, const Vec2 & size, const SourceLocation & location = SourceLocation::current());
    Response imageButton(Context * ui, const Key & key, TextureHandle texture, const Vec2 & size, const ImageButtonOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputText(Context * ui, StringView label, String * value, const TextInputOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response inputMultiline(Context * ui, StringView label, String * value, const TextInputOptions & options = {}, const LayoutOptions & layout = {}, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, int8_t * value, int8_t step = 1, int8_t fastStep = 10, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, uint8_t * value, uint8_t step = 1, uint8_t fastStep = 10, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, int16_t * value, int16_t step = 1, int16_t fastStep = 100, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, uint16_t * value, uint16_t step = 1, uint16_t fastStep = 100, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, int32_t * value, int32_t step = 1, int32_t fastStep = 100, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, uint32_t * value, uint32_t step = 1, uint32_t fastStep = 100, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, int64_t * value, int64_t step = 1, int64_t fastStep = 100, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, uint64_t * value, uint64_t step = 1, uint64_t fastStep = 100, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, int8_t * value, const NumericInputOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, uint8_t * value, const NumericInputOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, int16_t * value, const NumericInputOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, uint16_t * value, const NumericInputOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, int32_t * value, const NumericInputOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, uint32_t * value, const NumericInputOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, int64_t * value, const NumericInputOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputInt(Context * ui, StringView label, uint64_t * value, const NumericInputOptions & options, const SourceLocation & location = SourceLocation::current());
    Response inputFloat(Context * ui, StringView label, float * value, const NumericInputOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response inputDouble(Context * ui, StringView label, double * value, const NumericInputOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response inputFloatVector(Context * ui, StringView label, FloatSpan values, const NumericInputOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response inputIntVector(Context * ui, StringView label, Int32Span values, int32_t step = 1, int32_t fastStep = 100, const SourceLocation & location = SourceLocation::current());
    Response inputFloat3(Context * ui, StringView label, FloatSpan values, const NumericInputOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope treeNode(Context * ui, const Key & key, StringView label, bool defaultExpanded = false, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope treeNode(Context * ui, const Key & key, StringView label, const TreeNodeOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope treeNode(Context * ui, const Key & key, StringView label, SelectionModel * selection, Id item, IdSpan orderedItems, const TreeNodeOptions & treeOptions = {}, const SelectionOptions & selectionOptions = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Canvas canvas(Context * ui, StringView label, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Canvas canvas(Context * ui, const Key & key, StringView label, const LayoutOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, float * value, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, double * value, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragRange(Context * ui, StringView label, float * minimumValue, float * maximumValue, float lowerBound, float upperBound, const RangeOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragRange(Context * ui, StringView label, int32_t * minimumValue, int32_t * maximumValue, int32_t lowerBound, int32_t upperBound, const RangeOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, int32_t * value, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, int8_t * value, int8_t minimum, int8_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, uint8_t * value, uint8_t minimum, uint8_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, int16_t * value, int16_t minimum, int16_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, uint16_t * value, uint16_t minimum, uint16_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, uint32_t * value, uint32_t minimum, uint32_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, int64_t * value, int64_t minimum, int64_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragValue(Context * ui, StringView label, uint64_t * value, uint64_t minimum, uint64_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragFloatVector(Context * ui, StringView label, FloatSpan values, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response dragIntVector(Context * ui, StringView label, Int32Span values, int32_t minimum, int32_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response sliderFloatVector(Context * ui, StringView label, FloatSpan values, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response sliderIntVector(Context * ui, StringView label, Int32Span values, int32_t minimum, int32_t maximum, const SliderOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response sliderAngle(Context * ui, StringView label, float * radians, float minimumDegrees = -360.f, float maximumDegrees = 360.f, StringView format = "%.0f deg", LabelPlacement labelPlacement = LabelPlacement::Before, const SourceLocation & location = SourceLocation::current());
    Response searchField(Context * ui, StringView label, String * value, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope collapsingHeader(Context * ui, StringView label, bool defaultExpanded = false, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope collapsingHeader(Context * ui, StringView label, bool * open, bool defaultExpanded = false, const SourceLocation & location = SourceLocation::current());
    Response comboBox(Context * ui, StringView label, int * selected, StringViewSpan items, const SourceLocation & location = SourceLocation::current());
    Response comboBox(Context * ui, StringView label, int * selected, StringViewSpan items, const ComboOptions & options, const SourceLocation & location = SourceLocation::current());
    Response comboBox(Context * ui, StringView label, int * selected, size_t itemCount, ComboItemGetter getter, void * userData, const SourceLocation & location = SourceLocation::current());
    Response comboBox(Context * ui, StringView label, int * selected, size_t itemCount, ComboItemGetter getter, void * userData, const ComboOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope beginCombo(Context * ui, StringView label, StringView preview, const PopupOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope beginCombo(Context * ui, const Key & key, StringView label, StringView preview, const PopupOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope beginCombo(Context * ui, StringView label, StringView preview, const ComboOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope beginCombo(Context * ui, const Key & key, StringView label, StringView preview, const ComboOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] bool isComboOpen(const Context * ui, const Key & key) noexcept;
    [[nodiscard]] bool isComboOpen(const Context * ui, Id combo) noexcept;
    Response listBox(Context * ui, StringView label, int * selected, StringViewSpan items, const LayoutOptions & layout = {}, const SourceLocation & location = SourceLocation::current());
    Response listBox(Context * ui, StringView label, int * selected, StringViewSpan items, const ListBoxOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope beginListBox(Context * ui, StringView label, const ListBoxOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response tabs(Context * ui, StringView label, int * selected, StringViewSpan items, const SourceLocation & location = SourceLocation::current());
    Response tabs(Context * ui, StringView label, int * selected, StringViewSpan items, const TabsOptions & options, const SourceLocation & location = SourceLocation::current());
    Response tabs(Context * ui, StringView label, int * selected, StringViewSpan items, BoolSpan open, const TabsOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response tabs(Context * ui, StringView label, int * selected, StringViewSpan items, BoolSpan open, TabItemOptionsSpan itemOptions, const TabsOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TabBarScope beginTabBar(Context * ui, StringView label, const TabsOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TabBarScope beginTabBar(Context * ui, const Key & key, StringView label, const TabsOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope beginTabItem(Context * ui, StringView label, bool * open = nullptr, const TabItemOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope beginTabItem(Context * ui, const Key & key, StringView label, bool * open = nullptr, const TabItemOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    void setTabItemClosed(Context * ui, const Key & key) noexcept;
    Response tabItemButton(Context * ui, const Key & key, StringView label, const TabItemButtonOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response colorEditorRgb(Context * ui, StringView label, Color * color, const SourceLocation & location = SourceLocation::current());
    Response colorEditorRgb(Context * ui, StringView label, Color * color, const ColorEditOptions & options, const SourceLocation & location = SourceLocation::current());
    Response colorEditorRgba(Context * ui, StringView label, Color * color, const SourceLocation & location = SourceLocation::current());
    Response colorEditorRgba(Context * ui, StringView label, Color * color, const ColorEditOptions & options, const SourceLocation & location = SourceLocation::current());
    Response colorButton(Context * ui, const Key & key, Color * color, bool includeAlpha = true, const ColorEditOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response colorPickerRgb(Context * ui, StringView label, Color * color, const ColorPickerOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response colorPickerRgba(Context * ui, StringView label, Color * color, const ColorPickerOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response vectorEditor(Context * ui, StringView label, FloatSpan values, float minimum = 0.f, float maximum = 1.f, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope table(Context * ui, StringView label, uint32_t columns, const LayoutOptions & layout = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope table(Context * ui, StringView label, uint32_t columns, const TableOptions & tableOptions, const LayoutOptions & layout = {}, const SourceLocation & location = SourceLocation::current());
    void tableSetupColumn(Context * ui, uint32_t column, StringView label, const TableColumnOptions & options = {});
    void tableHeadersRow(Context * ui);
    void tableAngledHeadersRow(Context * ui);
    Response tableHeader(Context * ui, uint32_t column, StringView label = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] bool tableColumnName(const Context * ui, Id table, uint32_t column, StringView * const _out) noexcept;
    void tableNextRow(Context * ui);
    void tableNextRow(Context * ui, const TableRowOptions & options);
    [[nodiscard]] bool tableNextColumn(Context * ui);
    void tableSetRowBackground(Context * ui, const Color & color);
    void tableSetRowBackground(Context * ui, TableBackgroundTarget target, const Color & color);
    void tableSetCellBackground(Context * ui, const Color & color);
    [[nodiscard]] bool tableVisibleRows(Context * ui, size_t rowCount, float rowHeight, VisibleRange * const _out, size_t overscan = 2);
    Response tableNextRow(Context * ui, const Key & key, bool selected, const SourceLocation & location = SourceLocation::current());
    Response tableNextRow(Context * ui, const Key & key, SelectionModel * selection, Id item, IdSpan orderedItems = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] bool tableSetColumn(Context * ui, uint32_t column);
    [[nodiscard]] bool tableSortSpecs(const Context * ui, Id table, TableSortSpecSpan * const _out) noexcept;
    [[nodiscard]] bool tableSortState(const Context * ui, Id table, TableSortState * const _out) noexcept;
    void tableSortSpecsHandled(Context * ui, Id table) noexcept;
    [[nodiscard]] bool tableColumnStatus(const Context * ui, Id table, uint32_t column, TableColumnStatus * const _out) noexcept;
    void resetTableSettings(Context * ui, Id table) noexcept;
    [[nodiscard]] Scope columns(Context * ui, StringView label, uint32_t count = 1, const ColumnsOptions & options = {}, const LayoutOptions & layout = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] bool nextColumn(Context * ui);
    [[nodiscard]] uint32_t columnIndex(const Context * ui) noexcept;
    [[nodiscard]] uint32_t columnCount(const Context * ui) noexcept;
    [[nodiscard]] float columnWidth(const Context * ui, int32_t column = -1) noexcept;
    void setColumnWidth(Context * ui, int32_t column, float width) noexcept;
    [[nodiscard]] float columnOffset(const Context * ui, int32_t column = -1) noexcept;
    void setColumnOffset(Context * ui, int32_t column, float offset) noexcept;
    [[nodiscard]] Scope propertyGrid(Context * ui, StringView label, const PropertyGridOptions & options = {}, const LayoutOptions & layout = {}, const SourceLocation & location = SourceLocation::current());
    void propertyGridNextRow(Context * ui);
    [[nodiscard]] bool propertyGridValue(Context * ui);
    [[nodiscard]] bool propertyGridLabel(Context * ui);
    [[nodiscard]] Scope menuBar(Context * ui, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] Scope menuBar(Context * ui, const MenuBarOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope menu(Context * ui, StringView label, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope menu(Context * ui, StringView label, const MenuOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] TreeScope menu(Context * ui, const Key & key, StringView label, const MenuOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response menuItem(Context * ui, StringView label, bool enabled = true, const SourceLocation & location = SourceLocation::current());
    Response menuItem(Context * ui, const Key & key, StringView label, bool enabled = true, const SourceLocation & location = SourceLocation::current());
    Response menuItem(Context * ui, StringView label, const MenuItemOptions & options, const SourceLocation & location = SourceLocation::current());
    Response menuItem(Context * ui, const Key & key, StringView label, const MenuItemOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] WindowScope popup(Context * ui, StringView label, bool * open, const Rect & bounds = {}, const SourceLocation & location = SourceLocation::current());
    void openPopup(Context * ui, const Key & key, const PopupOptions & options = {});
    [[nodiscard]] WindowScope popup(Context * ui, const Key & key, const PopupOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] WindowScope popup(Context * ui, const Key & key, StringView label, const PopupOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    void closeCurrentPopup(Context * ui) noexcept;
    void closePopup(Context * ui, const Key & key, Id owner = InvalidId) noexcept;
    [[nodiscard]] bool isPopupOpen(const Context * ui, const Key & key) noexcept;
    [[nodiscard]] bool isPopupOpen(const Context * ui, const Key & key, Id owner) noexcept;
    [[nodiscard]] WindowScope modal(Context * ui, StringView label, bool * open, const Vec2 & size = {420.f, 240.f}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] WindowScope modal(Context * ui, const Key & key, StringView label, bool * open, const Vec2 & size = {420.f, 240.f}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] WindowScope tooltip(Context * ui, StringView label, const Vec2 & size = {240.f, 0.f}, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] WindowScope tooltip(Context * ui, const Key & key, StringView label, const Vec2 & size = {240.f, 0.f}, const SourceLocation & location = SourceLocation::current());
    void itemTooltip(Context * ui, const Response & item, StringView description, const Vec2 & size = {240.f, 0.f}, const SourceLocation & location = SourceLocation::current());
    void itemTooltip(Context * ui, const Response & item, StringView description, const Vec2 & size, float delay, const SourceLocation & location = SourceLocation::current());
    void itemTooltip(Context * ui, const Response & item, StringView description, const ItemTooltipOptions & options, const SourceLocation & location = SourceLocation::current());
    [[nodiscard]] WindowScope itemTooltip(Context * ui, const Response & item, const Key & key, const ItemTooltipOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    Response helpMarker(Context * ui, StringView description, const HelpMarkerOptions & options = {}, const SourceLocation & location = SourceLocation::current());

    Response slider(Context * ui, StringView label, float * value, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());

    Response slider(Context * ui, StringView label, int8_t * value, int8_t minimum, int8_t maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int8_t * value, int8_t minimum, int8_t maximum, LabelPlacement labelPlacement, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int8_t * value, int8_t minimum, int8_t maximum, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint8_t * value, uint8_t minimum, uint8_t maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint8_t * value, uint8_t minimum, uint8_t maximum, LabelPlacement labelPlacement, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint8_t * value, uint8_t minimum, uint8_t maximum, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int16_t * value, int16_t minimum, int16_t maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int16_t * value, int16_t minimum, int16_t maximum, LabelPlacement labelPlacement, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int16_t * value, int16_t minimum, int16_t maximum, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint16_t * value, uint16_t minimum, uint16_t maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint16_t * value, uint16_t minimum, uint16_t maximum, LabelPlacement labelPlacement, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint16_t * value, uint16_t minimum, uint16_t maximum, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, LabelPlacement labelPlacement, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint32_t * value, uint32_t minimum, uint32_t maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint32_t * value, uint32_t minimum, uint32_t maximum, LabelPlacement labelPlacement, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint32_t * value, uint32_t minimum, uint32_t maximum, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int64_t * value, int64_t minimum, int64_t maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int64_t * value, int64_t minimum, int64_t maximum, LabelPlacement labelPlacement, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, int64_t * value, int64_t minimum, int64_t maximum, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint64_t * value, uint64_t minimum, uint64_t maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint64_t * value, uint64_t minimum, uint64_t maximum, LabelPlacement labelPlacement, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, uint64_t * value, uint64_t minimum, uint64_t maximum, const SliderOptions & options, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, float * value, float minimum, float maximum, const SourceLocation & location = SourceLocation::current());
    Response slider(Context * ui, StringView label, double * value, double minimum, double maximum, const SourceLocation & location = SourceLocation::current());

    Response property(Context * ui, StringView name, bool * value, const SourceLocation & location = SourceLocation::current());
    Response property(Context * ui, StringView name, String * value, const SourceLocation & location = SourceLocation::current());

    template<class T> requires std::is_integral_v<T>
    Response property(Context * ui, StringView name, T * value, T minimum = std::numeric_limits<T>::lowest(), T maximum = std::numeric_limits<T>::max(), const SourceLocation & location = SourceLocation::current())
    {
        auto propertyScope = Mosaic::scope(ui, {}, location);
        auto propertyRow = Mosaic::row(ui, {}, location);
        Mosaic::text(ui, name, location);
        auto returnedValue = Mosaic::slider(ui, StringView{}, value, minimum, maximum, location);

        return returnedValue;
    }

    template<class T> requires std::is_floating_point_v<T>
    Response property(Context * ui, StringView name, T * value, T minimum = T{0}, T maximum = T{1}, const SourceLocation & location = SourceLocation::current())
    {
        auto propertyScope = Mosaic::scope(ui, {}, location);
        auto propertyRow = Mosaic::row(ui, {}, location);
        Mosaic::text(ui, name, location);
        auto returnedValue = Mosaic::slider(ui, StringView{}, value, minimum, maximum, location);

        return returnedValue;
    }

    [[nodiscard]] bool visibleRange(const Context * ui, size_t itemCount, float itemHeight, VisibleRange * const _out, Id scrollArea = InvalidId) noexcept;
    [[nodiscard]] bool beginListClipper(Context * ui, size_t itemCount, float itemExtent, VisibleRange * const _out, Id scrollArea = InvalidId, const ListClipperOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    void endListClipper(Context * ui, const VisibleRange & range, size_t itemCount, float itemExtent, const ListClipperOptions & options = {}, const SourceLocation & location = SourceLocation::current());
    void scrollTo(Context * ui, Id scrollArea, float offset) noexcept;
    void scrollTo(Context * ui, Id scrollArea, const Vec2 & offset) noexcept;
    void scrollToEnd(Context * ui, Id scrollArea, ScrollAxes axes = ScrollAxes::Both) noexcept;
    void scrollToItem(Context * ui, Id scrollArea, Id item) noexcept;
    void scrollToItem(Context * ui, Id scrollArea, Id item, const Vec2 & alignment) noexcept;
    void scrollToPosition(Context * ui, Id scrollArea, const Vec2 & position, const Vec2 & alignment = {}) noexcept;
    [[nodiscard]] bool scrollOffset(const Context * ui, Id scrollArea, Vec2 * const _out) noexcept;
    [[nodiscard]] bool scrollRange(const Context * ui, Id scrollArea, Vec2 * const _out) noexcept;
    void setTreeExpanded(Context * ui, Id tree, bool expanded) noexcept;
    [[nodiscard]] bool treeExpanded(const Context * ui, Id tree) noexcept;
    void setWindowBounds(Context * ui, Id window, const Rect & bounds) noexcept;
    [[nodiscard]] bool windowBounds(const Context * ui, Id window, Rect * const _out) noexcept;
    [[nodiscard]] bool windowFocused(const Context * ui, Id window) noexcept;
    [[nodiscard]] bool windowHovered(const Context * ui, Id window) noexcept;
    void focus(Context * ui, Id id) noexcept;
    // Focus the next focusable item submitted after this call. Positive offsets
    // skip that many focusable items; -1 focuses the preceding submitted item.
    void focusNextItem(Context * ui, int32_t offset = 0) noexcept;
    void setItemDefaultFocus(Context * ui, Id id) noexcept;
    [[nodiscard]] Id focused(const Context * ui) noexcept;
    [[nodiscard]] Id currentWindow(const Context * ui) noexcept;
    [[nodiscard]] Id capturedPointerOwner(const Context * ui) noexcept;
    [[nodiscard]] Id navigationFocus(const Context * ui) noexcept;
    void setItemCursor(Context * ui, Id item, CursorShape shape) noexcept;
    [[nodiscard]] CursorShape cursorShape(const Context * ui) noexcept;
    [[nodiscard]] bool currentViewport(const Context * ui, Viewport * const _out) noexcept;

    [[nodiscard]] ShortcutRegistry & shortcuts(Context * ui) noexcept;
    [[nodiscard]] const ShortcutRegistry & shortcuts(const Context * ui) noexcept;
    [[nodiscard]] PlatformAdapter & platform(Context * ui) noexcept;
    [[nodiscard]] const Input & input(const Context * ui) noexcept;
    [[nodiscard]] bool keyDown(const Context * ui, KeyCode key) noexcept;
    [[nodiscard]] float keyDownDuration(const Context * ui, KeyCode key) noexcept;
    [[nodiscard]] float pointerDownDuration(const Context * ui, PointerButton button = PointerButton::Primary) noexcept;
    [[nodiscard]] bool pointerDragging(const Context * ui, PointerButton button = PointerButton::Primary, float threshold = -1.f) noexcept;
    [[nodiscard]] bool fontCacheMetrics(const Context * ui, FontCacheMetrics * const _out) noexcept;
    [[nodiscard]] bool fontAtlasPage(const Context * ui, size_t index, FontAtlasPage * const _out) noexcept;
    [[nodiscard]] bool fontCacheEntries(const Context * ui, FontCacheEntryVector * const _out);
    [[nodiscard]] bool glyphCacheEntries(const Context * ui, GlyphCacheEntryVector * const _out);
    [[nodiscard]] DockModel & docking(Context * ui) noexcept;
    [[nodiscard]] const DockModel & docking(const Context * ui) noexcept;
    [[nodiscard]] DockModel & docking(Context * ui, uint32_t group) noexcept;
    [[nodiscard]] const DockModel & docking(const Context * ui, uint32_t group) noexcept;
    void setDockArea(Context * ui, const Rect & bounds) noexcept;
    void setDockArea(Context * ui, uint32_t group, const Rect & bounds) noexcept;
    [[nodiscard]] bool dockArea(const Context * ui, Rect * const _out) noexcept;
    [[nodiscard]] bool dockArea(const Context * ui, uint32_t group, Rect * const _out) noexcept;
    void clearDockSpace(Context * ui, uint32_t group) noexcept;
    [[nodiscard]] DockNodeId dockSpaceRoot(const Context * ui, uint32_t group) noexcept;
    [[nodiscard]] DockNodeId dockSpaceCentralNode(const Context * ui, uint32_t group) noexcept;
    [[nodiscard]] DockNodeId dockNodeForWindow(const Context * ui, uint32_t group, Id window) noexcept;
    bool dockSpaceSetCentralNode(Context * ui, uint32_t group, DockNodeId node) noexcept;
    bool dockWindow(Context * ui, uint32_t group, Id window, DockNodeId target, DockPlacement placement = DockPlacement::Center, float ratio = 0.5f);
    bool undockWindow(Context * ui, uint32_t group, Id window) noexcept;
    bool activateDockWindow(Context * ui, uint32_t group, Id window) noexcept;
    [[nodiscard]] DragDrop & dragDrop(Context * ui) noexcept;
    [[nodiscard]] const DragDrop & dragDrop(const Context * ui) noexcept;
    [[nodiscard]] bool beginDragDropSource(Context * ui, const Response & source, TypeId type, ByteSpan data, const DragDropSourceOptions & options = {});
    [[nodiscard]] DragDropAcceptResult acceptDragDropPayload(Context * ui, const Response & target, TypeId type, const DragDropTargetOptions & options = {});
    [[nodiscard]] bool dragDropSourcePreviewVisible(const Context * ui) noexcept;
    void cancelDragDrop(Context * ui) noexcept;

    [[nodiscard]] bool serialize(const Context * ui, Serializer & serializer);
    bool deserialize(Context * ui, const Deserializer & deserializer);
    void resetPersistentSection(Context * ui, StringView section);

    [[nodiscard]] bool debugBounds(const Context * ui, Id id, Rect * const _out) noexcept;
    [[nodiscard]] bool debugClip(const Context * ui, Id id, Rect * const _out) noexcept;
    [[nodiscard]] bool itemResponse(const Context * ui, Id id, Response * const _out) noexcept;
    [[nodiscard]] bool itemVisible(const Context * ui, Id id) noexcept;
    [[nodiscard]] bool itemHovered(const Context * ui, Id id, const ItemQueryOptions & options = {}) noexcept;
    [[nodiscard]] bool scopeFocused(const Context * ui, Id id, bool includeDescendants = true) noexcept;
    [[nodiscard]] bool scopeFocused(const Context * ui, Id id, const ScopeQueryOptions & options) noexcept;
    [[nodiscard]] bool scopeHovered(const Context * ui, Id id) noexcept;
    [[nodiscard]] bool scopeHovered(const Context * ui, Id id, const ScopeQueryOptions & options) noexcept;
    [[nodiscard]] double itemHoverDuration(const Context * ui, Id id) noexcept;
    [[nodiscard]] double itemStationaryHoverDuration(const Context * ui, Id id) noexcept;
    [[nodiscard]] bool pointerPosition(const Context * ui, Vec2 * const _out) noexcept;
    [[nodiscard]] bool consumeWheel(Context * ui, Id owner, Vec2 * const _out) noexcept;
    [[nodiscard]] bool isFocused(const Context * ui, Id id) noexcept;

    [[nodiscard]] Fill solidFill(const Color & color) noexcept;
    [[nodiscard]] Fill linearFill(const Vec2 & from, const Vec2 & to, FillStopSpan stops, FillSpread spread = FillSpread::Clamp) noexcept;
    [[nodiscard]] Fill radialFill(const Vec2 & center, float radius, FillStopSpan stops, FillSpread spread = FillSpread::Clamp) noexcept;
    [[nodiscard]] Fill conicFill(const Vec2 & center, float startAngle, FillStopSpan stops, FillSpread spread = FillSpread::Repeat) noexcept;

    void canvasRect(Context * ui, Id canvas, const Rect & bounds, const Color & color);
    void canvasRects(Context * ui, Id canvas, RectInstanceSpan instances);
    void canvasQuads(Context * ui, Id canvas, QuadInstanceSpan instances);
    void canvasBox(Context * ui, Id canvas, const Rect & bounds, const BoxStyle & style);
    void canvasRoundedRect(Context * ui, Id canvas, const Rect & bounds, float radius, const Color & color);
    void canvasGradient(Context * ui, Id canvas, const Rect & bounds, const Color & topLeft, const Color & topRight, const Color & bottomRight, const Color & bottomLeft);
    void canvasLine(Context * ui, Id canvas, const Vec2 & first, const Vec2 & second, float thickness, const Color & color);
    void canvasPolyline(Context * ui, Id canvas, Vec2Span points, float thickness, const Color & color, bool closed);
    void canvasCircle(Context * ui, Id canvas, const Vec2 & center, float radius, float thickness, const Color & color, uint32_t segments = 0);
    void canvasCircleFilled(Context * ui, Id canvas, const Vec2 & center, float radius, const Color & color, uint32_t segments = 0);
    void canvasEllipse(Context * ui, Id canvas, const Vec2 & center, const Vec2 & radii, float rotation, float thickness, const Color & color, uint32_t segments = 0);
    void canvasEllipseFilled(Context * ui, Id canvas, const Vec2 & center, const Vec2 & radii, float rotation, const Color & color, uint32_t segments = 0);
    void canvasRegularPolygon(Context * ui, Id canvas, const Vec2 & center, float radius, uint32_t sideCount, float rotation, float thickness, const Color & color);
    void canvasRegularPolygonFilled(Context * ui, Id canvas, const Vec2 & center, float radius, uint32_t sideCount, float rotation, const Color & color);
    void canvasGradientRing(Context * ui, Id canvas, const Vec2 & center, float innerRadius, float outerRadius, float startAngle, ColorSpan colors);
    void canvasTriangle(Context * ui, Id canvas, const Vec2 & first, const Vec2 & second, const Vec2 & third, float thickness, const Color & color);
    void canvasTriangleFilled(Context * ui, Id canvas, const Vec2 & first, const Vec2 & second, const Vec2 & third, const Color & color);
    void canvasConvexPolygon(Context * ui, Id canvas, Vec2Span points, const Color & color);
    void canvasGradientPolygon(Context * ui, Id canvas, ColoredPointSpan points);
    void canvasConcavePolygon(Context * ui, Id canvas, Vec2Span points, const Color & color);
    void canvasBezierQuadratic(Context * ui, Id canvas, const Vec2 & first, const Vec2 & control, const Vec2 & second, float thickness, const Color & color, uint32_t segments = 0);
    void canvasBezierCubic(Context * ui, Id canvas, const Vec2 & first, const Vec2 & firstControl, const Vec2 & secondControl, const Vec2 & second, float thickness, const Color & color, uint32_t segments = 0);
    [[nodiscard]] bool canvasText(Context * ui, Id canvas, const Vec2 & position, StringView value, const Color & color, Vec2 * const _out);
    void canvasPushClip(Context * ui, Id canvas, const Rect & bounds);
    void canvasPopClip(Context * ui, Id canvas);
    void canvasImage(Context * ui, Id canvas, TextureHandle texture, const Rect & bounds, const Rect & uv, const Color & tint);
    void canvasCustom(Context * ui, Id canvas, VertexSpan vertices, IndexSpan indices, const RenderState & state);
    void canvasSetChannel(Context * ui, Id canvas, uint32_t channel) noexcept;
    void canvasSetLayer(Context * ui, Id canvas, CanvasLayer layer) noexcept;
} // namespace Mosaic
