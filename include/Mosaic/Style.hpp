#pragma once

#include "Mosaic/Types.hpp"

namespace Mosaic
{
    enum class ColorButtonPosition : uint8_t
    {
        Left,
        Right
    };

    struct StyleColors
    {
        Color background = Color::fromBytes(24, 26, 31);
        Color panel = Color::fromBytes(34, 37, 44);
        Color panelHeader = Color::fromBytes(29, 34, 41);
        Color titleBackground = Color::fromBytes(29, 34, 41);
        Color titleBackgroundActive = Color::fromBytes(34, 39, 47);
        Color titleBackgroundCollapsed = Color::fromBytes(24, 27, 32);
        Color input = Color::fromBytes(18, 21, 26);
        Color panelHovered = Color::fromBytes(45, 50, 60);
        Color panelActive = Color::fromBytes(55, 62, 76);
        Color frame = Color::fromBytes(18, 21, 26);
        Color frameHovered = Color::fromBytes(25, 31, 38);
        Color frameActive = Color::fromBytes(30, 39, 49);
        Color button = Color::fromBytes(36, 43, 52);
        Color buttonHovered = Color::fromBytes(45, 57, 69);
        Color buttonActive = Color::fromBytes(31, 73, 98);
        Color header = Color::fromBytes(31, 43, 52);
        Color headerHovered = Color::fromBytes(38, 57, 69);
        Color headerActive = Color::fromBytes(34, 75, 100);
        Color popup = Color::fromBytes(20, 24, 30);
        Color popupBorder = Color::fromBytes(76, 88, 101);
        Color menu = Color::fromBytes(25, 30, 37);
        Color menuBarBackground = Color::fromBytes(25, 30, 37);
        Color menuHovered = Color::fromBytes(38, 57, 69);
        Color menuActive = Color::fromBytes(34, 75, 100);
        Color tab = Color::fromBytes(27, 34, 42);
        Color tabHovered = Color::fromBytes(39, 60, 74);
        Color tabActive = Color::fromBytes(31, 74, 101);
        Color tabSelected = Color::fromBytes(31, 74, 101);
        Color tabSelectedOverline = Color::fromBytes(75, 139, 245);
        Color tabDimmed = Color::fromBytes(22, 27, 33);
        Color tabDimmedSelected = Color::fromBytes(27, 38, 48);
        Color tabDimmedSelectedOverline = Color::fromBytes(58, 94, 132);
        Color scrollbar = Color::fromBytes(15, 19, 24);
        Color scrollbarHovered = Color::fromBytes(20, 26, 32);
        Color scrollbarActive = Color::fromBytes(24, 34, 42);
        Color scrollbarGrab = Color::fromBytes(72, 84, 97);
        Color scrollbarGrabHovered = Color::fromBytes(91, 112, 128);
        Color scrollbarGrabActive = Color::fromBytes(55, 154, 208);
        Color sliderGrab = Color::fromBytes(185, 207, 218);
        Color sliderGrabHovered = Color::fromBytes(204, 226, 236);
        Color sliderGrabActive = Color::fromBytes(87, 190, 235);
        Color separator = Color::fromBytes(55, 66, 77);
        Color separatorHovered = Color::fromBytes(74, 116, 140);
        Color separatorActive = Color::fromBytes(75, 170, 219);
        Color resizeGrip = Color::fromBytes(69, 158, 205);
        Color resizeGripHovered = Color::fromBytes(86, 180, 225);
        Color resizeGripActive = Color::fromBytes(106, 202, 241);
        Color text = Color::fromBytes(232, 235, 240);
        Color textDisabled = Color::fromBytes(125, 130, 140);
        Color textSelectionBackground = Color::fromBytes(58, 105, 180);
        Color textCursor = Color::fromBytes(232, 235, 240);
        Color accent = Color::fromBytes(75, 139, 245);
        Color border = Color::fromBytes(69, 74, 85);
        Color borderStrong = Color::fromBytes(91, 99, 114);
        Color borderShadow = Color::fromBytes(0, 0, 0, 0);
        Color selection = Color::fromBytes(58, 105, 180);
        Color warning = Color::fromBytes(231, 171, 56);
        Color error = Color::fromBytes(225, 73, 73);
        Color success = Color::fromBytes(65, 180, 110);
        Color textLink = Color::fromBytes(75, 139, 245);
        Color checkMark = Color::fromBytes(232, 235, 240);
        Color checkboxSelectedBackground = Color::fromBytes(75, 139, 245);
        Color treeLines = Color::fromBytes(55, 66, 77);
        Color tableHeader = Color::fromBytes(29, 34, 41);
        Color tableBorderStrong = Color::fromBytes(91, 99, 114);
        Color tableBorderLight = Color::fromBytes(55, 66, 77);
        Color tableRow = Color::fromBytes(24, 26, 31, 0);
        Color tableRowAlternate = Color::fromBytes(34, 37, 44, 96);
        Color dragDropTarget = Color::fromBytes(255, 214, 64);
        Color dragDropTargetBackground = Color::fromBytes(255, 214, 64, 34);
        Color dockingPreview = Color::fromBytes(75, 139, 245);
        Color dockingEmptyBackground = Color::fromBytes(18, 21, 26);
        Color navigationCursor = Color::fromBytes(75, 139, 245);
        Color navigationWindowingHighlight = Color::fromBytes(232, 235, 240, 178);
        Color navigationWindowingDimBackground = Color::fromBytes(0, 0, 0, 52);
        Color unsavedMarker = Color::fromBytes(232, 235, 240);
        Color modalDimBackground = Color::fromBytes(0, 0, 0, 112);
        Color plotLines = Color::fromBytes(75, 139, 245);
        Color plotLinesHovered = Color::fromBytes(111, 174, 255);
        Color plotHistogram = Color::fromBytes(75, 139, 245);
        Color plotHistogramHovered = Color::fromBytes(111, 174, 255);

        [[nodiscard]] constexpr bool operator==(const StyleColors &) const noexcept = default;
    };

    struct StyleMetrics
    {
        FontHandle font = DefaultFont;
        float fontSize = 13.f;
        float lineHeight = 17.f;
        float padding = 5.f;
        float gap = 4.f;
        EdgeInsets framePadding = {6.f, 3.f};
        Vec2 itemSpacing = {6.f, 4.f};
        Vec2 innerSpacing = {5.f, 4.f};
        EdgeInsets cellPadding = {5.f, 3.f};
        EdgeInsets popupPadding = {7.f, 6.f};
        float indent = 14.f;
        // Kept as the compatibility-wide default. Component-specific values below
        // let tools tune borders without changing every other widget family.
        float borderWidth = 1.f;
        float windowBorderSize = 1.f;
        float childBorderSize = 1.f;
        float popupBorderSize = 1.f;
        float frameBorderSize = 1.f;
        float tabBorderSize = 1.f;
        float tabBarBorderSize = 1.f;
        float cornerRadius = 4.f;
        float childCornerRadius = 4.f;
        float frameCornerRadius = 3.f;
        float popupCornerRadius = 5.f;
        float menuItemCornerRadius = 0.f;
        float tabCornerRadius = 4.f;
        float tabCloseButtonMinimumWidthSelected = -1.f;
        float tabCloseButtonMinimumWidthUnselected = 0.f;
        float tabMinimumWidthBase = 54.f;
        float tabMinimumWidthForShrink = 34.f;
        float tabOverlineSize = 1.f;
        float scrollbarCornerRadius = 5.f;
        float grabCornerRadius = 6.f;
        float scrollbarWidth = 11.f;
        float scrollbarPadding = 2.f;
        float grabMinimumSize = 12.f;
        Vec2 touchExtraPadding = {};
        float splitterWidth = 6.f;
        float windowTitleHeight = 27.f;
        float windowBorderHoverPadding = 4.f;
        float minimumControlWidth = 80.f;
        float minimumPopupWidth = 144.f;
        float controlHeight = 23.f;
        Vec2 buttonTextAlignment = {0.5f, 0.5f};
        Vec2 selectableTextAlignment = {0.f, 0.5f};
        Vec2 windowTitleAlignment = {0.f, 0.5f};
        float separatorSize = 1.f;
        float separatorTextBorderSize = 1.f;
        Vec2 separatorTextAlignment = {0.f, 0.5f};
        Vec2 separatorTextPadding = {8.f, 3.f};
        float tableAngledHeadersAngleDegrees = 35.f;
        float tableAngledHeadersTextAlignment = 0.5f;
        float treeLinesSize = 1.f;
        float logarithmicSliderDeadzone = 4.f;
        float colorMarkerSize = 3.f;
        ColorButtonPosition colorButtonPosition = ColorButtonPosition::Right;
        bool dockingNodeHasCloseButton = true;

        [[nodiscard]] constexpr bool operator==(const StyleMetrics &) const noexcept = default;
    };

    struct StyleBehavior
    {
        // Hit testing remains active when hover is disabled. Only visual hover feedback and
        // hover cursors are suppressed, so the same UI can be used on touch-only hardware.
        bool hoverEnabled = true;
        bool animationsEnabled = true;
        float alpha = 1.f;
        float disabledAlpha = 0.6f;
        float hoverAnimationDuration = 0.12f;
        float activeAnimationDuration = 0.07f;
        float selectionAnimationDuration = 0.16f;
        float valueAnimationDuration = 0.12f;
        float pressOffset = 1.f;
        float dragThreshold = 3.f;
        // Automatic DragValue speed traverses a bounded range in roughly 500 pixels.
        float dragSpeedDefaultRatio = 0.002f;
        // Keep sub-step movement useful by accumulating one step over roughly five pixels.
        float dragSpeedMinimumStepRatio = 0.2f;
        float tooltipHoverDelay = 0.25f;
        float tooltipStationaryDelay = 0.15f;

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
