#include "Mosaic/Style.hpp"

namespace Mosaic
{
    //////////////////////////////////////////////////////////////////////////
    Theme Theme::dark() noexcept
    {
        return {};
    }
    //////////////////////////////////////////////////////////////////////////
    Theme Theme::light() noexcept
    {
        Theme value;
        value.colors.background = Color::fromBytes(241, 243, 247);
        value.colors.panel = Color::fromBytes(225, 229, 236);
        value.colors.panelHeader = Color::fromBytes(211, 218, 228);
        value.colors.titleBackground = Color::fromBytes(211, 218, 228);
        value.colors.titleBackgroundActive = Color::fromBytes(195, 205, 219);
        value.colors.titleBackgroundCollapsed = Color::fromBytes(225, 229, 236);
        value.colors.input = Color::fromBytes(250, 251, 253);
        value.colors.panelHovered = Color::fromBytes(211, 218, 228);
        value.colors.panelActive = Color::fromBytes(195, 205, 219);
        value.colors.frame = Color::fromBytes(250, 251, 253);
        value.colors.frameHovered = Color::fromBytes(238, 242, 247);
        value.colors.frameActive = Color::fromBytes(224, 234, 244);
        value.colors.button = Color::fromBytes(220, 226, 234);
        value.colors.buttonHovered = Color::fromBytes(205, 216, 228);
        value.colors.buttonActive = Color::fromBytes(177, 207, 231);
        value.colors.header = Color::fromBytes(220, 226, 234);
        value.colors.headerHovered = Color::fromBytes(202, 217, 230);
        value.colors.headerActive = Color::fromBytes(175, 207, 232);
        value.colors.popup = Color::fromBytes(250, 251, 253);
        value.colors.popupBorder = Color::fromBytes(132, 142, 158);
        value.colors.menu = Color::fromBytes(232, 236, 242);
        value.colors.menuBarBackground = Color::fromBytes(232, 236, 242);
        value.colors.menuHovered = Color::fromBytes(202, 217, 230);
        value.colors.menuActive = Color::fromBytes(175, 207, 232);
        value.colors.tab = Color::fromBytes(218, 224, 232);
        value.colors.tabHovered = Color::fromBytes(198, 215, 229);
        value.colors.tabActive = Color::fromBytes(169, 204, 232);
        value.colors.tabSelected = Color::fromBytes(169, 204, 232);
        value.colors.tabSelectedOverline = Color::fromBytes(35, 111, 183);
        value.colors.tabDimmed = Color::fromBytes(226, 230, 236);
        value.colors.tabDimmedSelected = Color::fromBytes(205, 215, 226);
        value.colors.tabDimmedSelectedOverline = Color::fromBytes(96, 132, 168);
        value.colors.scrollbar = Color::fromBytes(224, 229, 236);
        value.colors.scrollbarHovered = Color::fromBytes(211, 219, 228);
        value.colors.scrollbarActive = Color::fromBytes(194, 210, 223);
        value.colors.scrollbarGrab = Color::fromBytes(158, 169, 183);
        value.colors.scrollbarGrabHovered = Color::fromBytes(125, 146, 166);
        value.colors.scrollbarGrabActive = Color::fromBytes(56, 143, 199);
        value.colors.sliderGrab = Color::fromBytes(93, 111, 128);
        value.colors.sliderGrabHovered = Color::fromBytes(70, 132, 169);
        value.colors.sliderGrabActive = Color::fromBytes(43, 133, 190);
        value.colors.separator = Color::fromBytes(181, 190, 201);
        value.colors.separatorHovered = Color::fromBytes(96, 156, 190);
        value.colors.separatorActive = Color::fromBytes(56, 143, 199);
        value.colors.resizeGrip = Color::fromBytes(56, 143, 199);
        value.colors.resizeGripHovered = Color::fromBytes(43, 133, 190);
        value.colors.resizeGripActive = Color::fromBytes(30, 113, 170);
        value.colors.text = Color::fromBytes(27, 31, 38);
        value.colors.textDisabled = Color::fromBytes(119, 125, 136);
        value.colors.textSelectionBackground = Color::fromBytes(142, 190, 224);
        value.colors.textCursor = value.colors.text;
        value.colors.border = Color::fromBytes(174, 181, 193);
        value.colors.borderStrong = Color::fromBytes(132, 142, 158);
        value.colors.borderShadow = Color::fromBytes(0, 0, 0, 0);
        value.colors.textLink = Color::fromBytes(35, 111, 183);
        value.colors.accent = value.colors.textLink;
        value.colors.selection = Color::fromBytes(177, 207, 231);
        value.colors.checkMark = Color::fromBytes(250, 251, 253);
        value.colors.checkboxSelectedBackground = Color::fromBytes(35, 111, 183);
        value.colors.treeLines = value.colors.separator;
        value.colors.tableHeader = value.colors.panelHeader;
        value.colors.tableBorderStrong = value.colors.borderStrong;
        value.colors.tableBorderLight = value.colors.separator;
        value.colors.tableRow = Color::fromBytes(241, 243, 247, 0);
        value.colors.tableRowAlternate = Color::fromBytes(211, 218, 228, 96);
        value.colors.dragDropTarget = Color::fromBytes(224, 146, 30);
        value.colors.dragDropTargetBackground = Color::fromBytes(224, 146, 30, 34);
        value.colors.dockingPreview = Color::fromBytes(35, 111, 183);
        value.colors.dockingEmptyBackground = Color::fromBytes(225, 229, 236);
        value.colors.navigationCursor = Color::fromBytes(35, 111, 183);
        value.colors.navigationWindowingHighlight = Color::fromBytes(27, 31, 38, 178);
        value.colors.navigationWindowingDimBackground = Color::fromBytes(0, 0, 0, 36);
        value.colors.unsavedMarker = value.colors.text;
        value.colors.modalDimBackground = Color::fromBytes(0, 0, 0, 80);
        value.colors.plotLines = Color::fromBytes(35, 111, 183);
        value.colors.plotLinesHovered = Color::fromBytes(22, 89, 157);
        value.colors.plotHistogram = Color::fromBytes(35, 111, 183);
        value.colors.plotHistogramHovered = Color::fromBytes(22, 89, 157);

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
