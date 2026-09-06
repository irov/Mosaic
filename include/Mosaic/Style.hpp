#pragma once

#include "Mosaic/Types.hpp"

namespace Mosaic
{
    enum class ColorButtonPosition : uint8_t
    {
        Left,
        Right
    };

    enum class WindowMenuButtonPosition : uint8_t
    {
        None,
        Left,
        Right
    };

    enum class TreeLineMode : uint8_t
    {
        Style,
        None,
        Full,
        ToNodes
    };

    enum class TooltipDelay : uint8_t
    {
        Default,
        None,
        Short,
        Normal
    };

    struct StyleColors
    {
        Color background = Color::fromBytes(32, 32, 32);
        Color panel = Color::fromBytes(38, 38, 38);
        Color panelHeader = Color::fromBytes(42, 42, 42);
        Color titleBackground = Color::fromBytes(38, 38, 38);
        Color titleBackgroundActive = Color::fromBytes(42, 42, 42);
        Color titleBackgroundCollapsed = Color::fromBytes(32, 32, 32);
        Color input = Color::fromBytes(25, 25, 25);
        Color panelHovered = Color::fromBytes(49, 49, 49);
        Color panelActive = Color::fromBytes(57, 57, 57);
        Color frame = Color::fromBytes(25, 25, 25);
        Color frameHovered = Color::fromBytes(43, 43, 43);
        Color frameActive = Color::fromBytes(48, 48, 48);
        Color button = Color::fromBytes(48, 48, 48);
        Color buttonHovered = Color::fromBytes(61, 61, 61);
        Color buttonActive = Color::fromBytes(35, 63, 81);
        Color header = Color::fromBytes(39, 39, 39);
        Color headerHovered = Color::fromBytes(49, 49, 49);
        Color headerActive = Color::fromBytes(48, 64, 76);
        Color popup = Color::fromBytes(36, 36, 36);
        Color popupBorder = Color::fromBytes(70, 70, 70);
        Color menu = Color::fromBytes(36, 36, 36);
        Color menuBarBackground = Color::fromBytes(38, 38, 38);
        Color menuHovered = Color::fromBytes(51, 51, 51);
        Color menuActive = Color::fromBytes(45, 62, 75);
        Color tab = Color::fromBytes(38, 38, 38);
        Color tabHovered = Color::fromBytes(48, 48, 48);
        Color tabActive = Color::fromBytes(46, 46, 46);
        Color tabSelected = Color::fromBytes(38, 38, 38);
        Color tabSelectedOverline = Color::fromBytes(71, 174, 235);
        Color tabDimmed = Color::fromBytes(36, 36, 36);
        Color tabDimmedSelected = Color::fromBytes(40, 40, 40);
        Color tabDimmedSelectedOverline = Color::fromBytes(125, 135, 141);
        Color scrollbar = Color::fromBytes(29, 29, 29);
        Color scrollbarHovered = Color::fromBytes(32, 32, 32);
        Color scrollbarActive = Color::fromBytes(34, 34, 34);
        Color scrollbarGrab = Color::fromBytes(72, 72, 72);
        Color scrollbarGrabHovered = Color::fromBytes(100, 100, 100);
        Color scrollbarGrabActive = Color::fromBytes(115, 145, 163);
        Color sliderGrab = Color::fromBytes(164, 169, 173);
        Color sliderGrabHovered = Color::fromBytes(206, 214, 218);
        Color sliderGrabActive = Color::fromBytes(71, 174, 235);
        Color separator = Color::fromBytes(49, 49, 49);
        Color separatorHovered = Color::fromBytes(88, 109, 121);
        Color separatorActive = Color::fromBytes(71, 174, 235);
        Color resizeGrip = Color::fromBytes(69, 158, 205);
        Color resizeGripHovered = Color::fromBytes(86, 180, 225);
        Color resizeGripActive = Color::fromBytes(106, 202, 241);
        Color text = Color::fromBytes(204, 204, 204);
        Color textDisabled = Color::fromBytes(132, 132, 132);
        Color textSelectionBackground = Color::fromBytes(44, 86, 115);
        Color textCursor = Color::fromBytes(222, 222, 222);
        Color accent = Color::fromBytes(71, 174, 235);
        Color border = Color::fromBytes(57, 57, 57);
        Color borderStrong = Color::fromBytes(22, 22, 22);
        Color borderShadow = Color::fromBytes(0, 0, 0, 0);
        Color selection = Color::fromBytes(49, 68, 83);
        Color warning = Color::fromBytes(231, 171, 56);
        Color error = Color::fromBytes(225, 73, 73);
        Color success = Color::fromBytes(65, 180, 110);
        Color textLink = Color::fromBytes(71, 174, 235);
        Color checkMark = Color::fromBytes(221, 225, 227);
        Color checkboxSelectedBackground = Color::fromBytes(43, 71, 88);
        Color treeLines = Color::fromBytes(52, 52, 52);
        Color tableHeader = Color::fromBytes(41, 41, 41);
        Color tableBorderStrong = Color::fromBytes(22, 22, 22);
        Color tableBorderLight = Color::fromBytes(48, 48, 48);
        Color tableRow = Color::fromBytes(32, 32, 32, 0);
        Color tableRowAlternate = Color::fromBytes(255, 255, 255, 5);
        Color dragDropTarget = Color::fromBytes(71, 174, 235);
        Color dragDropTargetBackground = Color::fromBytes(71, 174, 235, 28);
        Color dockingPreview = Color::fromBytes(71, 174, 235);
        Color dockingEmptyBackground = Color::fromBytes(28, 28, 28);
        Color navigationCursor = Color::fromBytes(71, 174, 235);
        Color navigationWindowingHighlight = Color::fromBytes(232, 235, 240, 178);
        Color navigationWindowingDimBackground = Color::fromBytes(0, 0, 0, 52);
        Color unsavedMarker = Color::fromBytes(232, 235, 240);
        Color modalDimBackground = Color::fromBytes(0, 0, 0, 112);
        Color plotLines = Color::fromBytes(71, 174, 235);
        Color plotLinesHovered = Color::fromBytes(111, 174, 255);
        Color plotHistogram = Color::fromBytes(71, 174, 235);
        Color plotHistogramHovered = Color::fromBytes(111, 174, 255);

        [[nodiscard]] constexpr bool operator==(const StyleColors &) const noexcept = default;
    };

    struct StyleMetrics
    {
        FontHandle font = DefaultFont;
        float fontSize = 12.f;
        float lineHeight = 16.f;
        float padding = 4.f;
        float gap = 2.f;
        EdgeInsets framePadding = {5.f, 2.f};
        Vec2 itemSpacing = {4.f, 2.f};
        Vec2 innerSpacing = {4.f, 2.f};
        EdgeInsets cellPadding = {4.f, 1.f};
        EdgeInsets popupPadding = {7.f, 6.f};
        float indent = 12.f;
        // Kept as the compatibility-wide default. Component-specific values below
        // let tools tune borders without changing every other widget family.
        float borderWidth = 1.f;
        float windowBorderSize = 1.f;
        float childBorderSize = 1.f;
        float popupBorderSize = 1.f;
        float frameBorderSize = 1.f;
        float tabBorderSize = 0.f;
        float tabBarBorderSize = 1.f;
        float cornerRadius = 2.f;
        float childCornerRadius = 0.f;
        float frameCornerRadius = 2.f;
        float popupCornerRadius = 3.f;
        float menuItemCornerRadius = 0.f;
        float tabCornerRadius = 0.f;
        float tabCloseButtonMinimumWidthSelected = -1.f;
        float tabCloseButtonMinimumWidthUnselected = 0.f;
        float tabMinimumWidthBase = 44.f;
        float tabMinimumWidthForShrink = 28.f;
        float tabOverlineSize = 1.f;
        float scrollbarCornerRadius = 2.f;
        float grabCornerRadius = 2.f;
        float scrollbarWidth = 9.f;
        float scrollbarPadding = 2.f;
        float grabMinimumSize = 10.f;
        Vec2 touchExtraPadding = {};
        float splitterWidth = 3.f;
        float windowTitleHeight = 24.f;
        float windowBorderHoverPadding = 4.f;
        float minimumControlWidth = 56.f;
        float propertyLabelWidth = 88.f;
        float minimumPopupWidth = 144.f;
        float controlHeight = 20.f;
        Vec2 buttonTextAlignment = {0.5f, 0.5f};
        Vec2 selectableTextAlignment = {0.f, 0.5f};
        Vec2 windowTitleAlignment = {0.f, 0.5f};
        float separatorSize = 1.f;
        float separatorTextBorderSize = 1.f;
        Vec2 separatorTextAlignment = {0.f, 0.5f};
        Vec2 separatorTextPadding = {8.f, 3.f};
        float tableAngledHeadersAngleDegrees = 35.f;
        float tableAngledHeadersTextAlignment = 0.5f;
        TreeLineMode treeLineMode = TreeLineMode::None;
        float treeLinesSize = 1.f;
        float treeLinesRounding = 0.f;
        float logarithmicSliderDeadzone = 4.f;
        float colorMarkerSize = 2.f;
        float imageRounding = 0.f;
        float imageBorderSize = 0.f;
        Vec2 displayWindowPadding = {19.f, 19.f};
        Vec2 displaySafeAreaPadding = {3.f, 3.f};
        ColorButtonPosition colorButtonPosition = ColorButtonPosition::Right;
        WindowMenuButtonPosition windowMenuButtonPosition = WindowMenuButtonPosition::Right;
        bool dockingNodeHasCloseButton = true;

        [[nodiscard]] constexpr bool operator==(const StyleMetrics &) const noexcept = default;
    };

    struct StyleBehavior
    {
        // Hit testing remains active when hover is disabled. Only visual hover feedback and
        // hover cursors are suppressed, so the same UI can be used on touch-only hardware.
        bool hoverEnabled = true;
        bool animationsEnabled = true;
        bool antiAliasedLines = true;
        bool antiAliasedLinesUseTexture = false;
        bool antiAliasedFill = true;
        uint8_t curveTessellationQuality = 24;
        uint8_t ellipseTessellationQuality = 32;
        uint8_t rectangleTessellationQuality = 8;
        float curveTessellationMaximumError = 1.6f;
        float circleTessellationMaximumError = 0.3f;
        float alpha = 1.f;
        float disabledAlpha = 0.6f;
        float hoverAnimationDuration = 0.09f;
        float activeAnimationDuration = 0.045f;
        float selectionAnimationDuration = 0.12f;
        float valueAnimationDuration = 0.1f;
        float pressOffset = 0.f;
        float dragThreshold = 3.f;
        // Automatic DragValue speed traverses a bounded range in roughly 500 pixels.
        float dragSpeedDefaultRatio = 0.002f;
        // Keep sub-step movement useful by accumulating one step over roughly five pixels.
        float dragSpeedMinimumStepRatio = 0.2f;
        float tooltipHoverDelay = 0.25f;
        float tooltipShortDelay = 0.25f;
        float tooltipNormalDelay = 0.40f;
        float tooltipStationaryDelay = 0.15f;
        TooltipDelay tooltipMouseDelay = TooltipDelay::Short;
        bool tooltipMouseStationary = true;
        bool tooltipMouseSharedDelay = true;
        bool tooltipNavigationFocus = true;

        [[nodiscard]] constexpr bool operator==(const StyleBehavior &) const noexcept = default;
    };

    struct Theme
    {
        StyleColors colors;
        StyleMetrics metrics;
        StyleBehavior behavior;

        [[nodiscard]] constexpr bool operator==(const Theme &) const noexcept = default;

        [[nodiscard]] static Theme dark() noexcept;
        [[nodiscard]] static Theme light() noexcept;
    };
} // namespace Mosaic
