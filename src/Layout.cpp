#include "Layout.hpp"
#include "Render.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        bool isContainer(NodeKind kind) noexcept
        {
            return kind <= NodeKind::Style || kind == NodeKind::Tree || kind == NodeKind::Table || kind == NodeKind::TableRow || kind == NodeKind::TableCell || kind == NodeKind::Canvas;
        }
        //////////////////////////////////////////////////////////////////////////
        bool clipsDescendants(NodeKind kind) noexcept
        {
            return kind == NodeKind::Root || kind == NodeKind::Window || kind == NodeKind::Scroll || kind == NodeKind::Clip || kind == NodeKind::Table || kind == NodeKind::TableRow || kind == NodeKind::TableCell || kind == NodeKind::Canvas;
        }
        //////////////////////////////////////////////////////////////////////////
        bool isFloatingRootChild(NodeKind kind) noexcept
        {
            return kind == NodeKind::Window || kind == NodeKind::Backdrop;
        }
        //////////////////////////////////////////////////////////////////////////
        bool measuresChildrenAsOverlay(NodeKind kind) noexcept
        {
            bool result = false;
            switch(kind)
            {
            case NodeKind::Overlay:
            case NodeKind::Absolute:
            case NodeKind::Clip:
            case NodeKind::Scope:
            case NodeKind::Disabled:
            case NodeKind::Interaction:
            case NodeKind::Style:
            case NodeKind::Canvas:
            {
                result = true;
                break;
            }
            default:
            {
                break;
            }
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        bool arrangesChildrenAsOverlay(NodeKind kind) noexcept
        {
            bool result = false;
            switch(kind)
            {
            case NodeKind::Overlay:
            case NodeKind::Clip:
            case NodeKind::Scope:
            case NodeKind::Disabled:
            case NodeKind::Interaction:
            case NodeKind::Style:
            case NodeKind::Canvas:
            {
                result = true;
                break;
            }
            default:
            {
                break;
            }
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        bool inheritsFirstBaseline(NodeKind kind) noexcept
        {
            bool result = false;
            switch(kind)
            {
            case NodeKind::Scope:
            case NodeKind::Disabled:
            case NodeKind::Interaction:
            case NodeKind::Style:
            case NodeKind::Column:
            {
                result = true;
                break;
            }
            default:
            {
                break;
            }
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        bool isHorizontal(NodeKind kind, Orientation orientation) noexcept
        {
            auto returnedValue = kind == NodeKind::Row || (kind == NodeKind::Scroll && orientation == Orientation::Horizontal) || (kind == NodeKind::Split && orientation == Orientation::Horizontal);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        bool scrollsContent(const Context::Node & node) noexcept
        {
            auto returnedValue = node.kind == NodeKind::Scroll || (node.kind == NodeKind::Window && node.windowScrollable == true);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        bool hasInsets(const EdgeInsets & insets) noexcept
        {
            return insets.left != 0.f || insets.top != 0.f || insets.right != 0.f || insets.bottom != 0.f;
        }
        //////////////////////////////////////////////////////////////////////////
        Rect inset(const Rect & rectangle, const EdgeInsets & value) noexcept
        {
            Rect result = {rectangle.x + value.left, rectangle.y + value.top, std::max(0.f, rectangle.width - value.left - value.right), std::max(0.f, rectangle.height - value.top - value.bottom)};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    float Context::resolveDimension(const Dimension & dimension, float measured, float available) const noexcept
    {
        switch(dimension.rule)
        {
        case SizeRule::Fixed:
        {
            return dimension.value;
        }
        case SizeRule::Fill:
        {
            return available;
        }
        case SizeRule::Percent:
        {
            auto returnedValue = available * std::clamp(dimension.value, 0.f, 1.f);

            return returnedValue;
        }
        case SizeRule::Auto:
        case SizeRule::Content:
        {
            return measured;
        }
        }

        return measured;
    }
    //////////////////////////////////////////////////////////////////////////
    Vec2 Context::measureNode(size_t index, const Vec2 & available)
    {
        Node & node = nodes[index];
        node.baseline = 0.f;
        node.baselineValid = false;
        Vec2 contentSize;
        float nodeGap = gap(node);
        size_t childCount = 0;

        switch(node.kind)
        {
        case Detail::NodeKind::Text:
            if(node.wordWrap == true)
            {
                float resolvedWidth = node.layout.width.rule == SizeRule::Fixed ? node.layout.width.value : available.x;
                node.textWrapWidth = std::max(1.f, resolvedWidth - node.layout.padding.left - node.layout.padding.right);
                prepareText(node, node.label);
            }

            contentSize = node.textSize;
            break;
        case Detail::NodeKind::Bullet:
            contentSize = {node.style->metrics.indent * 0.65f, node.style->metrics.lineHeight};
            break;
        case Detail::NodeKind::BulletText:
            contentSize = {node.textSize.x + node.style->metrics.indent, node.textSize.y};
            break;
        case Detail::NodeKind::Button:
        case Detail::NodeKind::Selectable:
        case Detail::NodeKind::Tab:
        case Detail::NodeKind::Tree:
            contentSize = node.textSize;
            contentSize.x += node.style->metrics.framePadding.left + node.style->metrics.framePadding.right + (node.kind == Detail::NodeKind::Tree ? node.style->metrics.indent : 0.f) + (node.kind == Detail::NodeKind::Tab && node.tabCloseVisible ? node.style->metrics.controlHeight * 0.7f : 0.f) + (node.kind == Detail::NodeKind::Tab && node.tabUnsavedDocument ? node.style->metrics.fontSize * 0.75f : 0.f);
            contentSize.y = std::max(contentSize.y + node.style->metrics.padding, node.style->metrics.controlHeight);

            if(node.semanticRole == SemanticRole::MenuItem)
            {
                float checkExtent = node.menuPopupItem ? node.style->metrics.controlHeight * 0.75f : 0.f;
                float shortcutExtent = node.valueText.empty() == true ? 0.f : node.valueTextSize.x + node.style->metrics.innerSpacing.x;
                float submenuExtent = node.menuSubmenu ? node.style->metrics.controlHeight * 0.75f : 0.f;
                contentSize.x += checkExtent + shortcutExtent + submenuExtent;
            }

            if(node.kind == Detail::NodeKind::Selectable && node.tableHeader == true && node.tableColumnOptions.angledHeader == true)
            {
                constexpr float radiansPerDegree = 0.0174532925f;
                float angle = std::clamp(node.style->metrics.tableAngledHeadersAngleDegrees, -80.f, 80.f) * radiansPerDegree;
                float sine = std::abs(std::sin(angle));
                float cosine = std::abs(std::cos(angle));
                contentSize = {std::max(node.style->metrics.controlHeight, node.textSize.y * sine + node.style->metrics.framePadding.left + node.style->metrics.framePadding.right), std::max(node.style->metrics.controlHeight, node.textSize.x * sine + node.textSize.y * cosine + node.style->metrics.framePadding.top + node.style->metrics.framePadding.bottom)};
            }

            break;
        case Detail::NodeKind::Toggle:
            contentSize = node.textSize;
            contentSize.x += node.style->metrics.padding * 2.f + 34.f + node.style->metrics.gap;
            contentSize.y = std::max(contentSize.y + node.style->metrics.padding, node.style->metrics.controlHeight);
            break;
        case Detail::NodeKind::Combo:
        {
            float arrowWidth = node.comboShowArrow ? node.style->metrics.controlHeight : 0.f;
            float previewWidth = node.comboShowPreview ? (node.comboWidthFitPreview ? node.valueTextSize.x + node.style->metrics.framePadding.left + node.style->metrics.framePadding.right : std::max(200.f, node.style->metrics.minimumControlWidth) - arrowWidth) : 0.f;
            float labelWidth = Detail::itemLabelWidth(node);
            contentSize = {previewWidth + arrowWidth + labelWidth, node.style->metrics.controlHeight};
            break;
        }
        case Detail::NodeKind::IconButton:
        case Detail::NodeKind::ImageButton:
            contentSize = node.measured.x > 0.f ? node.measured : Vec2{node.style->metrics.controlHeight, node.style->metrics.controlHeight};
            break;
        case Detail::NodeKind::Checkbox:
        case Detail::NodeKind::Radio:
            contentSize = node.textSize;
            contentSize.x += node.style->metrics.controlHeight + node.style->metrics.gap;
            contentSize.y = std::max(contentSize.y, node.style->metrics.controlHeight);
            break;
        case Detail::NodeKind::Slider:
            contentSize = {std::max(200.f, node.style->metrics.minimumControlWidth) + Detail::itemLabelWidth(node), node.style->metrics.controlHeight};
            break;
        case Detail::NodeKind::InputText:
            contentSize = {std::max(200.f, node.style->metrics.minimumControlWidth), node.style->metrics.controlHeight};
            break;
        case Detail::NodeKind::ColorEdit:
            contentSize = {std::max(node.colorShowInputs ? 250.f : 200.f, node.style->metrics.minimumControlWidth) + Detail::itemLabelWidth(node), node.style->metrics.controlHeight};
            break;
        case Detail::NodeKind::DragValue:
            contentSize = {std::max(96.f, node.style->metrics.minimumControlWidth), node.style->metrics.controlHeight};
            break;
        case Detail::NodeKind::InputMultiline:
            contentSize = {std::max(240.f, node.style->metrics.minimumControlWidth), 120.f};
            break;
        case Detail::NodeKind::Progress:
            contentSize = {std::max(160.f, node.style->metrics.minimumControlWidth), node.style->metrics.controlHeight};
            break;
        case Detail::NodeKind::Separator:
            contentSize = {1.f, std::max(1.f, node.style->metrics.separatorSize)};
            break;
        case Detail::NodeKind::SeparatorText:
            contentSize = {node.textSize.x + node.style->metrics.separatorTextPadding.x * 2.f, std::max(node.textSize.y, node.style->metrics.lineHeight) + node.style->metrics.separatorTextPadding.y * 2.f};
            break;
        case Detail::NodeKind::Spacer:
            contentSize = {node.scalar, node.scalar};
            break;
        case Detail::NodeKind::Image:
            contentSize = node.measured;
            break;
        case Detail::NodeKind::Canvas:
            contentSize = {240.f, 160.f};
            break;
        default:
            break;
        }

        if(Detail::isContainer(node.kind) == true)
        {
            float totalPrimary = 0.f;
            float maximumSecondary = 0.f;
            float maximumBaseline = 0.f;
            float maximumBelowBaseline = 0.f;
            float firstBaseline = 0.f;
            bool firstBaselineValid = false;
            Vec2 childAvailable = {std::min(available.x, std::max(0.f, node.layout.maximum.x - node.layout.padding.left - node.layout.padding.right)), std::min(available.y, std::max(0.f, node.layout.maximum.y - node.layout.padding.top - node.layout.padding.bottom))};

            if(node.layout.width.rule == SizeRule::Fixed || node.layout.width.rule == SizeRule::Fill || node.layout.width.rule == SizeRule::Percent)
            {
                childAvailable.x = std::max(0.f, resolveDimension(node.layout.width, available.x, available.x) - node.layout.padding.left - node.layout.padding.right);
            }

            if(node.layout.height.rule == SizeRule::Fixed || node.layout.height.rule == SizeRule::Fill || node.layout.height.rule == SizeRule::Percent)
            {
                childAvailable.y = std::max(0.f, resolveDimension(node.layout.height, available.y, available.y) - node.layout.padding.top - node.layout.padding.bottom);
            }

            size_t child = node.firstChild;
            while(child != std::numeric_limits<size_t>::max())
            {
                if(nodes[child].visible == true)
                {
                    Vec2 childSize = measureNode(child, childAvailable);

                    if(node.kind == Detail::NodeKind::Root && Detail::isFloatingRootChild(nodes[child].kind) == true)
                    {
                        child = nodes[child].nextSibling;
                        continue;
                    }

                    ++childCount;

                    if(nodes[child].baselineValid == true)
                    {
                        if(firstBaselineValid == false)
                        {
                            firstBaseline = nodes[child].baseline;
                            firstBaselineValid = true;
                        }

                        maximumBaseline = std::max(maximumBaseline, nodes[child].baseline);
                        maximumBelowBaseline = std::max(maximumBelowBaseline, childSize.y - nodes[child].baseline);
                    }

                    if(Detail::isHorizontal(node.kind, node.layout.orientation) == true)
                    {
                        totalPrimary += childSize.x;
                        maximumSecondary = std::max(maximumSecondary, childSize.y);
                    }
                    else if(Detail::measuresChildrenAsOverlay(node.kind) == true)
                    {
                        totalPrimary = std::max(totalPrimary, childSize.x);
                        maximumSecondary = std::max(maximumSecondary, childSize.y);
                    }
                    else
                    {
                        totalPrimary += childSize.y;
                        maximumSecondary = std::max(maximumSecondary, childSize.x);
                    }
                }

                child = nodes[child].nextSibling;
            }

            if(childCount > 1)
            {
                if(Detail::measuresChildrenAsOverlay(node.kind) == false)
                {
                    totalPrimary += nodeGap * static_cast<float>(childCount - 1);
                }
            }

            if(node.kind == Detail::NodeKind::Tree)
            {
                float childHeight = childCount == 0 ? 0.f : totalPrimary + nodeGap;
                contentSize = {std::max(contentSize.x, maximumSecondary + node.style->metrics.indent), contentSize.y + childHeight};
            }
            else if(node.kind == Detail::NodeKind::Grid)
            {
                uint32_t columns = std::max(1U, node.layout.columns);
                uint32_t rows = static_cast<uint32_t>((childCount + columns - 1) / columns);
                float gridHeight = 0.f;
                float rowHeight = 0.f;
                size_t visibleIndex = 0;
                for(size_t gridChild = node.firstChild; gridChild != std::numeric_limits<size_t>::max(); gridChild = nodes[gridChild].nextSibling)
                {
                    if(nodes[gridChild].visible == false)
                    {
                        continue;
                    }

                    rowHeight = std::max(rowHeight, nodes[gridChild].measured.y);
                    ++visibleIndex;

                    if(visibleIndex % columns == 0)
                    {
                        gridHeight += rowHeight;
                        rowHeight = 0.f;
                    }
                }

                if(visibleIndex % columns != 0)
                {
                    gridHeight += rowHeight;
                }

                contentSize = {maximumSecondary * static_cast<float>(columns) + nodeGap * static_cast<float>(columns - 1), childCount == 0 ? 0.f : gridHeight + nodeGap * static_cast<float>(rows - 1)};
            }
            else if(node.kind == Detail::NodeKind::Table)
            {
                TableState & tableState = this->tableState(node.id);
                uint32_t columns = std::max(1U, node.layout.columns);

                if(tableState.columns.size() < columns)
                {
                    tableState.columns.resize(columns);
                }

                FloatVector & intrinsicWidths = tableState.intrinsicWidths;
                intrinsicWidths.assign(columns, 0.f);
                FloatVector & rowHeights = tableState.rowHeights;
                rowHeights.clear();
                uint32_t maximumRow = 0;
                float headerHeight = 0.f;
                for(size_t tableChild = node.firstChild; tableChild != std::numeric_limits<size_t>::max(); tableChild = nodes[tableChild].nextSibling)
                {
                    if(nodes[tableChild].visible == false)
                    {
                        continue;
                    }

                    uint32_t column = std::min(nodes[tableChild].tableColumn, columns - 1);

                    if(nodes[tableChild].kind != Detail::NodeKind::TableRow)
                    {
                        bool headerWithoutWidth = tableState.headersSubmitted && nodes[tableChild].tableRow < tableState.headerRowCount && nodes[tableChild].tableColumnOptions.headerContributesToWidth == false;

                        if(headerWithoutWidth == false)
                        {
                            intrinsicWidths[column] = std::max(intrinsicWidths[column], nodes[tableChild].measured.x);
                        }
                    }

                    maximumRow = std::max(maximumRow, nodes[tableChild].tableRow);

                    if(tableState.rowsVirtualized == true)
                    {
                        if(nodes[tableChild].tableRow < tableState.virtualFirstRow)
                        {
                            headerHeight = std::max(headerHeight, nodes[tableChild].measured.y);
                        }
                    }
                    else
                    {
                        if(rowHeights.size() <= nodes[tableChild].tableRow)
                        {
                            rowHeights.resize(nodes[tableChild].tableRow + 1, 0.f);
                        }

                        rowHeights[nodes[tableChild].tableRow] = std::max(rowHeights[nodes[tableChild].tableRow], nodes[tableChild].measured.y);
                    }
                }
                tableState.maximumRow = maximumRow;
                size_t visibleColumns = static_cast<size_t>(std::count_if(tableState.columns.begin(), tableState.columns.begin() + columns,
                                                                                [](const Context::TableColumnState & value)
                                                                                {
                                                                                    return value.options.enabled && value.options.visible;
                                                                                }));
                float width = visibleColumns > 1 ? nodeGap * static_cast<float>(visibleColumns - 1) : 0.f;
                float fixedSameWidth = 0.f;
                for(uint32_t column = 0; column != columns; ++column)
                {
                    const TableColumnOptions & columnOptions = tableState.columns[column].options;

                    if(columnOptions.enabled == false)
                    {
                        continue;
                    }

                    if(columnOptions.visible == false)
                    {
                        continue;
                    }

                    if(columnOptions.sizing != TableSizing::FixedSame)
                    {
                        continue;
                    }

                    fixedSameWidth = std::max(fixedSameWidth, columnOptions.widthOrWeight > 0.f ? columnOptions.widthOrWeight : std::max(intrinsicWidths[column], node.style->metrics.minimumControlWidth));
                }
                for(uint32_t column = 0; column != columns; ++column)
                {
                    const TableColumnOptions & columnOptions = tableState.columns[column].options;

                    if(columnOptions.enabled == false)
                    {
                        continue;
                    }

                    if(columnOptions.visible == false)
                    {
                        continue;
                    }

                    if(columnOptions.sizing == TableSizing::FixedSame)
                    {
                        width += fixedSameWidth;
                    }
                    else if(columnOptions.sizing == TableSizing::FixedFit)
                    {
                        width += columnOptions.widthOrWeight > 0.f ? columnOptions.widthOrWeight : intrinsicWidths[column];
                    }
                    else
                    {
                        width += Detail::tableSizingFixed(columnOptions.sizing) && columnOptions.widthOrWeight > 0.f ? columnOptions.widthOrWeight : std::max(intrinsicWidths[column], node.style->metrics.minimumControlWidth);
                    }
                }
                float height = 0.f;

                if(tableState.rowsVirtualized == true)
                {
                    if(tableState.virtualFirstRow != 0)
                    {
                        height += headerHeight > 0.f ? headerHeight : node.style->metrics.controlHeight;
                    }

                    height += static_cast<float>(tableState.virtualRowCount) * std::max(tableState.virtualRowHeight, node.tableOptions.rowMinimumHeight);
                    size_t totalRows = static_cast<size_t>(tableState.virtualFirstRow) + tableState.virtualRowCount;

                    if(totalRows > 1)
                    {
                        height += nodeGap * static_cast<float>(totalRows - 1);
                    }
                }
                else
                {
                    for(size_t row = 0; row != rowHeights.size(); ++row)
                    {
                        float requested = row < tableState.requestedRowHeights.size() ? tableState.requestedRowHeights[row] : 0.f;
                        float padding = row < tableState.requestedRowPaddingY.size() && tableState.requestedRowPaddingY[row] >= 0.f ? tableState.requestedRowPaddingY[row] * 2.f : 0.f;
                        height += std::max({rowHeights[row] + padding, requested, node.tableOptions.rowMinimumHeight});
                    }

                    if(rowHeights.empty() == false)
                    {
                        height += nodeGap * static_cast<float>(maximumRow);
                    }
                }

                contentSize = {std::max(contentSize.x, width), std::max(contentSize.y, height)};

                if(node.tableOptions.scrollHorizontal == true)
                {
                    contentSize.x = std::max(contentSize.x, node.tableOptions.innerWidth);
                }

                if(node.tableOptions.extendHostHorizontal == false)
                {
                    contentSize.x = std::min(contentSize.x, available.x);
                }

                if(node.tableOptions.extendHostVertical == false)
                {
                    contentSize.y = std::min(contentSize.y, available.y);
                }
            }
            else if(Detail::isHorizontal(node.kind, node.layout.orientation) == true)
            {
                contentSize = {std::max(contentSize.x, totalPrimary), std::max(contentSize.y, maximumSecondary)};

                if(node.layout.crossAxisAlignment == CrossAxisAlignment::Baseline && maximumBaseline > 0.f)
                {
                    contentSize.y = std::max(contentSize.y, maximumBaseline + maximumBelowBaseline);
                }
            }
            else if(Detail::measuresChildrenAsOverlay(node.kind) == true)
            {
                contentSize = {std::max(contentSize.x, totalPrimary), std::max(contentSize.y, maximumSecondary)};
            }
            else
            {
                contentSize = {std::max(contentSize.x, maximumSecondary), std::max(contentSize.y, totalPrimary)};
            }

            if(node.kind == Detail::NodeKind::Row && maximumBaseline > 0.f)
            {
                node.baseline = node.layout.padding.top + maximumBaseline;
                node.baselineValid = true;
            }
            else if(Detail::inheritsFirstBaseline(node.kind) == true)
            {
                if(firstBaselineValid == true)
                {
                    node.baseline = node.layout.padding.top + firstBaseline;
                    node.baselineValid = true;
                }
            }
        }

        contentSize.x += node.layout.padding.left + node.layout.padding.right;
        contentSize.y += node.layout.padding.top + node.layout.padding.bottom;

        if(node.kind == Detail::NodeKind::Window && node.windowTitleVisible == true)
        {
            contentSize.y += node.style->metrics.windowTitleHeight;
        }

        float labelWidth = Detail::itemLabelWidth(node);
        float measuredWidth = 0.f;

        if(node.itemWidthRequested == true)
        {
            float controlWidth = node.itemWidth < 0.f ? std::max(1.f, available.x + node.itemWidth) : std::max(1.f, node.itemWidth);
            measuredWidth = controlWidth + labelWidth;
        }
        else if(labelWidth > 0.f && node.layout.width.rule != SizeRule::Content && node.layout.width.rule != SizeRule::Auto)
        {
            measuredWidth = resolveDimension(node.layout.width, std::max(0.f, contentSize.x - labelWidth), available.x) + labelWidth;
        }
        else
        {
            measuredWidth = resolveDimension(node.layout.width, contentSize.x, available.x);
        }

        node.measured.x = std::clamp(measuredWidth, node.layout.minimum.x, node.layout.maximum.x);
        node.measured.y = std::clamp(resolveDimension(node.layout.height, contentSize.y, available.y), node.layout.minimum.y, node.layout.maximum.y);

        if(node.baselineValid == false)
        {
            bool textBaselineRequired = false;
            switch(node.kind)
            {
            case Detail::NodeKind::Text:
            case Detail::NodeKind::Bullet:
            case Detail::NodeKind::BulletText:
            case Detail::NodeKind::Button:
            case Detail::NodeKind::Selectable:
            case Detail::NodeKind::Tab:
            case Detail::NodeKind::Tree:
            case Detail::NodeKind::InputText:
            case Detail::NodeKind::InputMultiline:
            case Detail::NodeKind::DragValue:
            case Detail::NodeKind::Combo:
            case Detail::NodeKind::Checkbox:
            case Detail::NodeKind::Radio:
            case Detail::NodeKind::Toggle:
                textBaselineRequired = true;
                break;
            default:
                break;
            }

            if(textBaselineRequired == true)
            {
                FontMetrics fontMetrics = {node.style->metrics.fontSize, std::max(0.f, node.style->metrics.lineHeight - node.style->metrics.fontSize), 0.f};

                if(fontProvider != nullptr)
                {
                    (void)fontProvider->metrics(node.style->metrics.font, node.style->metrics.fontSize, &fontMetrics);
                }

                float fontLineHeight = std::max(1.f, fontMetrics.lineHeight());
                float textBaseline = std::max(0.f, (node.style->metrics.lineHeight - fontLineHeight) * 0.5f) + fontMetrics.ascent;
                switch(node.kind)
                {
                case Detail::NodeKind::Text:
                case Detail::NodeKind::Bullet:
                case Detail::NodeKind::BulletText:
                    node.baseline = node.layout.padding.top + textBaseline;
                    break;
                case Detail::NodeKind::Button:
                case Detail::NodeKind::Selectable:
                case Detail::NodeKind::Tab:
                case Detail::NodeKind::Tree:
                case Detail::NodeKind::InputText:
                case Detail::NodeKind::InputMultiline:
                case Detail::NodeKind::DragValue:
                case Detail::NodeKind::Combo:
                    node.baseline = node.layout.padding.top + node.style->metrics.framePadding.top + textBaseline;
                    break;
                case Detail::NodeKind::Checkbox:
                case Detail::NodeKind::Radio:
                case Detail::NodeKind::Toggle:
                    node.baseline = node.layout.padding.top + std::max(0.f, (node.style->metrics.controlHeight - node.style->metrics.lineHeight) * 0.5f) + textBaseline;
                    break;
                default:
                    break;
                }
                node.baselineValid = true;
            }
        }

        return node.measured;
    }

    //////////////////////////////////////////////////////////////////////////
    void Context::cullNode(size_t index, const Rect & bounds)
    {
        Node & node = nodes[index];
        node.response.flags |= Detail::CulledNodeFlag;
        node.bounds = bounds;
        node.content = {};
        node.clip = {};
        node.visualClip = {};
        node.childrenClip = {};
        Persistent * persistentState = node.persistentState;

        if(persistentState != nullptr)
        {
            persistentState->lastClip = {};
            persistentState->lastFrame = frame.number;
        }

        for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
        {
            cullNode(child, nodes[child].bounds);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::arrangeNode(size_t index, const Rect & bounds, const Rect & inheritedClip)
    {
        Node & node = nodes[index];
        node.response.flags &= ~Detail::CulledNodeFlag;
        node.bounds = bounds;
        node.clip = Rect::intersection(inheritedClip, bounds);

        if(node.kind == Detail::NodeKind::Clip && node.explicitClip.empty() == false)
        {
            node.clip = Rect::intersection(node.clip, node.explicitClip);
        }

        node.visualClip = Detail::usesAncestorVisualClip(node.kind) ? inheritedClip : node.clip;
        // A node's own BB and the clip inherited by its descendants are different
        // concepts. Layout-only wrappers such as Scope, Style and Row must not clip
        // children to their estimated bounds: a horizontally scrolled, partially
        // visible child would otherwise disappear before reaching the viewport edge.
        node.childrenClip = Detail::clipsDescendants(node.kind) ? node.clip : inheritedClip;
        node.content = Detail::inset(bounds, node.layout.padding);

        if(node.kind == Detail::NodeKind::Window && node.windowTitleVisible == true)
        {
            node.content.y += node.style->metrics.windowTitleHeight;
            node.content.height = std::max(0.f, node.content.height - node.style->metrics.windowTitleHeight);
        }

        Persistent * nodeState = node.persistentState;

        if(nodeState == nullptr && (Detail::scrollsContent(node) == true || node.kind == Detail::NodeKind::Table))
        {
            nodeState = &state(node);
        }

        if(nodeState != nullptr)
        {
            nodeState->lastFrame = frame.number;

            if(node.kind == Detail::NodeKind::Tree)
            {
                node.treeFrameBounds = {node.bounds.x, node.bounds.y, node.bounds.width, node.style->metrics.controlHeight};

                if(node.treeSpanLabelWidth == true)
                {
                    node.treeFrameBounds.width = std::min(node.treeFrameBounds.width, node.textSize.x + node.style->metrics.padding * 2.f + node.style->metrics.indent);
                }

                node.treeFrameClip = node.clip;
                node.treeLabelClip = node.clip;
                nodeState->lastBounds = node.treeFrameBounds;
            }
            else if(node.kind == Detail::NodeKind::Combo)
            {
                nodeState->lastBounds = Detail::comboGeometry(node, node.bounds).control;
            }
            else
            {
                nodeState->lastBounds = node.bounds;
            }

            nodeState->lastClip = node.clip;

            if(Detail::scrollsContent(node) == true)
            {
                nodeState->scrollOrientation = node.scrollOptions.axes == ScrollAxes::Horizontal ? Orientation::Horizontal : Orientation::Vertical;
                nodeState->scrollbarTrackBounds = {};
                nodeState->scrollbarThumbBounds = {};
                nodeState->verticalScrollbarTrack = {};
                nodeState->verticalScrollbarThumb = {};
                nodeState->horizontalScrollbarTrack = {};
                nodeState->horizontalScrollbarThumb = {};
            }
        }

        bool preserveEmptyScrollContent = Detail::scrollsContent(node) == true && node.windowContentSizeExplicit == true;

        if(node.firstChild == std::numeric_limits<size_t>::max() && preserveEmptyScrollContent == false)
        {
            if(Detail::scrollsContent(node) == true)
            {
                Persistent & persistentState = *nodeState;
                persistentState.scroll = 0.f;
                persistentState.scrollExtent = 0.f;
                persistentState.scrollPosition = {};
                persistentState.scrollTarget = {};
                persistentState.scrollVelocity = {};
                persistentState.scrollRange = {};
                persistentState.scrollContentSize = {};
                persistentState.scrollTargetInitialized = false;
                persistentState.scrollToEndX = false;
                persistentState.scrollToEndY = false;
                persistentState.draggingScrollbar = false;
            }

            return;
        }

        size_t childrenBegin = layoutChildren.size();
        for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
        {
            bool floatingRootChild = node.kind == Detail::NodeKind::Root && Detail::isFloatingRootChild(nodes[child].kind) == true;

            if(nodes[child].visible == true && floatingRootChild == false)
            {
                layoutChildren.push_back(child);
            }
        }
        NodeIndexSpan children = childrenBegin == layoutChildren.size() ? NodeIndexSpan{} : NodeIndexSpan(layoutChildren.data() + childrenBegin, layoutChildren.size() - childrenBegin);

        bool preserveEmptyScrollChildren = Detail::scrollsContent(node) == true && node.windowContentSizeExplicit == true;

        if(children.empty() == true && preserveEmptyScrollChildren == false)
        {
            if(Detail::scrollsContent(node) == true)
            {
                Persistent & persistentState = *nodeState;
                persistentState.scroll = 0.f;
                persistentState.scrollExtent = 0.f;
                persistentState.scrollPosition = {};
                persistentState.scrollTarget = {};
                persistentState.scrollVelocity = {};
                persistentState.scrollRange = {};
                persistentState.scrollContentSize = {};
                persistentState.scrollTargetInitialized = false;
                persistentState.scrollToEndX = false;
                persistentState.scrollToEndY = false;
                persistentState.draggingScrollbar = false;
            }

            return;
        }

        float nodeGap = gap(node);
        Rect childArea = node.content;
        Rect childClip = node.childrenClip;

        if(node.kind == Detail::NodeKind::Tree)
        {
            float treeIndent = node.tableColumnOptions.indent == TableColumnIndent::Disable ? 0.f : node.style->metrics.indent;
            childArea.x += treeIndent;
            childArea.width = std::max(0.f, childArea.width - treeIndent);
            childArea.y += node.style->metrics.controlHeight + nodeGap;
            childArea.height = std::max(0.f, childArea.height - node.style->metrics.controlHeight - nodeGap);
        }

        if(Detail::scrollsContent(node) == true)
        {
            Persistent & persistentState = *nodeState;
            float contentWidth = 0.f;
            float contentHeight = 0.f;

            if(node.layout.orientation == Orientation::Vertical)
            {
                for(size_t child : children)
                {
                    contentWidth = std::max(contentWidth, nodes[child].measured.x);
                    contentHeight += nodes[child].measured.y;
                }

                if(children.empty() == false)
                {
                    contentHeight += nodeGap * static_cast<float>(children.size() - 1);
                }
            }
            else
            {
                for(size_t child : children)
                {
                    contentWidth += nodes[child].measured.x;
                    contentHeight = std::max(contentHeight, nodes[child].measured.y);
                }

                if(children.empty() == false)
                {
                    contentWidth += nodeGap * static_cast<float>(children.size() - 1);
                }
            }

            if(node.windowContentSizeExplicit == true)
            {
                contentWidth = std::max(contentWidth, node.windowContentSize.x);
                contentHeight = std::max(contentHeight, node.windowContentSize.y);
            }

            bool allowHorizontal = node.scrollOptions.axes == ScrollAxes::Horizontal || node.scrollOptions.axes == ScrollAxes::Both;
            bool allowVertical = node.scrollOptions.axes == ScrollAxes::Vertical || node.scrollOptions.axes == ScrollAxes::Both;
            bool forceScrollbars = node.scrollOptions.visibility == ScrollbarVisibility::Always;
            bool hideScrollbars = node.scrollOptions.visibility == ScrollbarVisibility::Hidden;
            float scrollbarSize = std::max(8.f, node.style->metrics.scrollbarWidth);
            bool verticalScrollbar = allowVertical && hideScrollbars == false && (forceScrollbars || contentHeight > childArea.height);
            bool horizontalScrollbar = allowHorizontal && hideScrollbars == false && (forceScrollbars || contentWidth > childArea.width - (verticalScrollbar ? scrollbarSize + nodeGap : 0.f));
            verticalScrollbar = allowVertical && hideScrollbars == false && (forceScrollbars || contentHeight > childArea.height - (horizontalScrollbar ? scrollbarSize + nodeGap : 0.f));

            Rect viewportArea = {childArea.x, childArea.y, std::max(0.f, childArea.width - (verticalScrollbar ? scrollbarSize + nodeGap : 0.f)), std::max(0.f, childArea.height - (horizontalScrollbar ? scrollbarSize + nodeGap : 0.f))};
            persistentState.scrollContentSize = {contentWidth, contentHeight};
            childClip = Rect::intersection(node.childrenClip, viewportArea);
            node.childrenClip = childClip;
            persistentState.scrollRange = {allowHorizontal ? std::max(0.f, contentWidth - viewportArea.width) : 0.f, allowVertical ? std::max(0.f, contentHeight - viewportArea.height) : 0.f};

            if(persistentState.scrollToEndX == true)
            {
                persistentState.scrollTarget.x = persistentState.scrollRange.x;
                persistentState.scrollToEndX = false;
                persistentState.scrollTargetInitialized = true;
            }

            if(persistentState.scrollToEndY == true)
            {
                persistentState.scrollTarget.y = persistentState.scrollRange.y;
                persistentState.scrollToEndY = false;
                persistentState.scrollTargetInitialized = true;
            }

            persistentState.scrollPosition = {std::clamp(persistentState.scrollPosition.x, 0.f, persistentState.scrollRange.x), std::clamp(persistentState.scrollPosition.y, 0.f, persistentState.scrollRange.y)};

            if(persistentState.scrollTargetInitialized == false)
            {
                persistentState.scrollTarget = persistentState.scrollPosition;
                persistentState.scrollTargetInitialized = true;
            }

            persistentState.scrollTarget = {std::clamp(persistentState.scrollTarget.x, 0.f, persistentState.scrollRange.x), std::clamp(persistentState.scrollTarget.y, 0.f, persistentState.scrollRange.y)};
            persistentState.scroll = node.layout.orientation == Orientation::Vertical ? persistentState.scrollPosition.y : persistentState.scrollPosition.x;
            persistentState.scrollExtent = node.layout.orientation == Orientation::Vertical ? persistentState.scrollRange.y : persistentState.scrollRange.x;

            persistentState.verticalScrollbarTrack = {};
            persistentState.verticalScrollbarThumb = {};
            persistentState.horizontalScrollbarTrack = {};
            persistentState.horizontalScrollbarThumb = {};

            if(verticalScrollbar == true)
            {
                persistentState.verticalScrollbarTrack = {node.bounds.right() - scrollbarSize, viewportArea.y, scrollbarSize, viewportArea.height};
                float trackExtent = persistentState.verticalScrollbarTrack.height;
                float thumbExtent = std::clamp(trackExtent * viewportArea.height / std::max(1.f, contentHeight), std::min(node.style->metrics.grabMinimumSize, trackExtent), trackExtent);
                float travel = std::max(0.f, trackExtent - thumbExtent);
                float offset = persistentState.scrollRange.y <= 0.f ? 0.f : travel * persistentState.scrollPosition.y / persistentState.scrollRange.y;
                persistentState.verticalScrollbarThumb = {persistentState.verticalScrollbarTrack.x, persistentState.verticalScrollbarTrack.y + offset, persistentState.verticalScrollbarTrack.width, thumbExtent};
            }

            if(horizontalScrollbar == true)
            {
                persistentState.horizontalScrollbarTrack = {viewportArea.x, node.bounds.bottom() - scrollbarSize, viewportArea.width, scrollbarSize};
                float trackExtent = persistentState.horizontalScrollbarTrack.width;
                float thumbExtent = std::clamp(trackExtent * viewportArea.width / std::max(1.f, contentWidth), std::min(node.style->metrics.grabMinimumSize, trackExtent), trackExtent);
                float travel = std::max(0.f, trackExtent - thumbExtent);
                float offset = persistentState.scrollRange.x <= 0.f ? 0.f : travel * persistentState.scrollPosition.x / persistentState.scrollRange.x;
                persistentState.horizontalScrollbarThumb = {persistentState.horizontalScrollbarTrack.x + offset, persistentState.horizontalScrollbarTrack.y, thumbExtent, persistentState.horizontalScrollbarTrack.height};
            }

            bool primaryVertical = node.scrollOptions.axes != ScrollAxes::Horizontal;
            persistentState.scrollbarTrackBounds = primaryVertical ? persistentState.verticalScrollbarTrack : persistentState.horizontalScrollbarTrack;
            persistentState.scrollbarThumbBounds = primaryVertical ? persistentState.verticalScrollbarThumb : persistentState.horizontalScrollbarThumb;

            if(persistentState.scrollRange.x <= 0.f && persistentState.scrollRange.y <= 0.f)
            {
                persistentState.draggingScrollbar = false;
                persistentState.draggingScrollAxis = 0;
            }

            childArea = viewportArea;
            childArea.x -= persistentState.scrollPosition.x;
            childArea.y -= persistentState.scrollPosition.y;
            childArea.width = allowHorizontal ? std::max(childArea.width, contentWidth) : viewportArea.width;
            childArea.height = allowVertical ? std::max(childArea.height, contentHeight) : viewportArea.height;
        }

        if(Detail::arrangesChildrenAsOverlay(node.kind) == true)
        {
            for(size_t child : children)
            {
                float width = resolveDimension(nodes[child].layout.width, nodes[child].measured.x, childArea.width);
                float height = resolveDimension(nodes[child].layout.height, nodes[child].measured.y, childArea.height);
                Rect childBounds = {childArea.x + nodes[child].layout.offset.x, childArea.y + nodes[child].layout.offset.y, std::min(width, childArea.width), std::min(height, childArea.height)};

                if(Rect::intersection(childBounds, node.childrenClip).empty() == true)
                {
                    cullNode(child, childBounds);
                }
                else
                {
                    arrangeNode(child, childBounds, node.childrenClip);
                }
            }

            return;
        }

        if(node.kind == Detail::NodeKind::Absolute)
        {
            for(size_t child : children)
            {
                Rect rectangle = nodes[child].layout.absoluteRect;
                rectangle.x += childArea.x + nodes[child].layout.offset.x;
                rectangle.y += childArea.y + nodes[child].layout.offset.y;

                if(rectangle.width <= 0.f)
                {
                    rectangle.width = nodes[child].measured.x;
                }

                if(rectangle.height <= 0.f)
                {
                    rectangle.height = nodes[child].measured.y;
                }

                if(Rect::intersection(rectangle, node.childrenClip).empty() == true)
                {
                    cullNode(child, rectangle);
                }
                else
                {
                    arrangeNode(child, rectangle, node.childrenClip);
                }
            }

            return;
        }

        if(node.kind == Detail::NodeKind::Grid)
        {
            uint32_t columns = std::max(1U, node.layout.columns);
            float cellWidth = std::max(0.f, (childArea.width - nodeGap * static_cast<float>(columns - 1)) / static_cast<float>(columns));
            size_t rowCount = (children.size() + columns - 1) / columns;
            size_t rowOffset = layoutFloatScratch.size();
            layoutFloatScratch.resize(rowOffset + rowCount, 0.f);
            for(size_t childIndex = 0; childIndex != children.size(); ++childIndex)
            {
                float & rowHeight = layoutFloatScratch[rowOffset + childIndex / columns];
                rowHeight = std::max(rowHeight, nodes[children[childIndex]].measured.y);
            }
            float y = childArea.y;
            for(size_t childIndex = 0; childIndex != children.size(); ++childIndex)
            {
                size_t row = childIndex / columns;
                size_t column = childIndex % columns;
                const Node & child = nodes[children[childIndex]];
                Rect cellBounds = {childArea.x + static_cast<float>(column) * (cellWidth + nodeGap) + child.layout.offset.x, y + child.layout.offset.y, cellWidth, layoutFloatScratch[rowOffset + row]};

                if(Rect::intersection(cellBounds, node.childrenClip).empty() == true)
                {
                    cullNode(children[childIndex], cellBounds);
                }
                else
                {
                    arrangeNode(children[childIndex], cellBounds, node.childrenClip);
                }

                if(column + 1 == columns || childIndex + 1 == children.size())
                {
                    y += layoutFloatScratch[rowOffset + row] + nodeGap;
                }
            }
            layoutFloatScratch.resize(rowOffset);

            return;
        }

        if(node.kind == Detail::NodeKind::Table)
        {
            Persistent & tablePersistent = state(node);
            TableState & tableState = this->tableState(node.id);
            uint32_t columnCount = std::max(1U, node.layout.columns);
            float horizontalGap = node.tableOptions.padInnerHorizontal ? nodeGap : 0.f;

            if(node.tableOptions.padOuterHorizontal == true)
            {
                childArea.x += node.style->metrics.cellPadding.left;
                childArea.width = std::max(0.f, childArea.width - node.style->metrics.cellPadding.left - node.style->metrics.cellPadding.right);
            }

            if(tableState.columns.size() < columnCount)
            {
                tableState.columns.resize(columnCount);
            }

            SizeVector & displayOrder = tableState.displayOrder;

            if(tableState.displayOrderDirty == true || displayOrder.size() != columnCount)
            {
                displayOrder.resize(columnCount);
                std::iota(displayOrder.begin(), displayOrder.end(), size_t{0});
                std::sort(displayOrder.begin(), displayOrder.end(),
                          [&tableState](size_t first, size_t second)
                          {
                              uint32_t firstOrder = tableState.columns[first].order;
                              uint32_t secondOrder = tableState.columns[second].order;

                              return firstOrder == secondOrder ? first < second : firstOrder < secondOrder;
                          });
                tableState.displayOrderDirty = false;
            }

            const FloatVector & intrinsicWidths = tableState.intrinsicWidths;
            uint32_t maximumRow = tableState.maximumRow;

            size_t visibleColumnCount = static_cast<size_t>(std::count_if(tableState.columns.begin(), tableState.columns.begin() + columnCount,
                                                                                [](const Context::TableColumnState & value)
                                                                                {
                                                                                    return value.options.enabled && value.options.visible;
                                                                                }));
            float gaps = visibleColumnCount > 1 ? horizontalGap * static_cast<float>(visibleColumnCount - 1) : 0.f;
            float availableWidth = std::max(0.f, (node.tableOptions.scrollHorizontal ? std::max(childArea.width, node.tableOptions.innerWidth) : childArea.width) - gaps);
            float fixedWidth = 0.f;
            float totalWeight = 0.f;
            float fixedSameWidth = 0.f;
            for(uint32_t column = 0; column != columnCount; ++column)
            {
                const Context::TableColumnState & columnState = tableState.columns[column];

                if(columnState.options.enabled == false)
                {
                    continue;
                }

                if(columnState.options.visible == false)
                {
                    continue;
                }

                if(columnState.options.sizing != TableSizing::FixedSame)
                {
                    continue;
                }

                fixedSameWidth = std::max(fixedSameWidth, columnState.options.widthOrWeight > 0.f ? columnState.options.widthOrWeight : intrinsicWidths[column]);
            }
            for(uint32_t column = 0; column != columnCount; ++column)
            {
                Context::TableColumnState & columnState = tableState.columns[column];

                if(columnState.options.enabled == false)
                {
                    columnState.resolvedWidth = 0.f;
                    continue;
                }

                if(columnState.options.visible == false)
                {
                    columnState.resolvedWidth = 0.f;
                    continue;
                }

                if(Detail::tableSizingFixed(columnState.options.sizing) == true)
                {
                    columnState.resolvedWidth = columnState.options.sizing == TableSizing::FixedSame ? fixedSameWidth : columnState.options.widthOrWeight > 0.f ? columnState.options.widthOrWeight : columnState.options.sizing == TableSizing::FixedFit ? intrinsicWidths[column] : std::max(intrinsicWidths[column], node.style->metrics.minimumControlWidth);
                    fixedWidth += columnState.resolvedWidth;
                }
                else
                {
                    totalWeight += Detail::tableStretchWeight(columnState.options, intrinsicWidths[column]);
                }
            }
            float stretchWidth = std::max(0.f, availableWidth - fixedWidth);
            float physicalPixel = 1.f / std::max(1.f, viewport.dpiScale);
            float assignedStretchWidth = 0.f;
            uint32_t stretchColumnCount = 0;
            for(uint32_t column = 0; column != columnCount; ++column)
            {
                Context::TableColumnState & columnState = tableState.columns[column];

                if(columnState.options.enabled == true && columnState.options.visible == true && Detail::tableSizingStretch(columnState.options.sizing) == true)
                {
                    float weight = Detail::tableStretchWeight(columnState.options, intrinsicWidths[column]);
                    float exactWidth = stretchWidth * weight / totalWeight;
                    columnState.resolvedWidth = std::floor(exactWidth / physicalPixel) * physicalPixel;
                    assignedStretchWidth += columnState.resolvedWidth;
                    ++stretchColumnCount;
                }
            }

            if(node.tableOptions.preciseWidths == false && stretchColumnCount != 0)
            {
                float remainder = std::max(0.f, stretchWidth - assignedStretchWidth);
                uint32_t lastStretchColumn = 0;
                for(size_t orderedColumn : displayOrder)
                {
                    uint32_t column = static_cast<uint32_t>(orderedColumn);
                    Context::TableColumnState & columnState = tableState.columns[column];

                    if(columnState.options.enabled == false)
                    {
                        continue;
                    }

                    if(columnState.options.visible == false)
                    {
                        continue;
                    }

                    if(Detail::tableSizingStretch(columnState.options.sizing) == false)
                    {
                        continue;
                    }

                    lastStretchColumn = column;

                    if(remainder >= physicalPixel)
                    {
                        columnState.resolvedWidth += physicalPixel;
                        remainder -= physicalPixel;
                    }
                }
                tableState.columns[lastStretchColumn].resolvedWidth += remainder;
            }

            if(node.tableOptions.keepColumnsVisible == true && node.tableOptions.scrollHorizontal == false)
            {
                float resolvedTotal = gaps;
                for(uint32_t column = 0; column != columnCount; ++column)
                {
                    if(tableState.columns[column].options.enabled == true && tableState.columns[column].options.visible == true)
                    {
                        resolvedTotal += tableState.columns[column].resolvedWidth;
                    }
                }

                if(resolvedTotal > childArea.width && resolvedTotal > gaps)
                {
                    float scale = std::max(0.f, childArea.width - gaps) / (resolvedTotal - gaps);
                    for(uint32_t column = 0; column != columnCount; ++column)
                    {
                        if(tableState.columns[column].options.enabled == true && tableState.columns[column].options.visible == true)
                        {
                            tableState.columns[column].resolvedWidth *= scale;
                        }
                    }
                }
            }

            FloatVector & columnX = tableState.columnPositions;
            columnX.assign(columnCount, childArea.x);
            float x = childArea.x;
            size_t visibleColumn = 0;
            for(size_t orderedColumn : displayOrder)
            {
                uint32_t column = static_cast<uint32_t>(orderedColumn);

                if(tableState.columns[column].options.enabled == false)
                {
                    continue;
                }

                if(tableState.columns[column].options.visible == false)
                {
                    continue;
                }

                columnX[column] = x;
                x += tableState.columns[column].resolvedWidth;

                if(++visibleColumn < visibleColumnCount)
                {
                    x += horizontalGap;
                }
            }
            float contentWidth = std::max(0.f, x - childArea.x);

            FloatVector & rowHeights = tableState.rowHeights;
            FloatVector & rowY = tableState.rowPositions;
            rowY.clear();
            float virtualHeaderHeight = 0.f;
            float contentHeight = 0.f;

            if(tableState.rowsVirtualized == true)
            {
                for(size_t child : children)
                {
                    if(nodes[child].tableRow < tableState.virtualFirstRow)
                    {
                        virtualHeaderHeight = std::max(virtualHeaderHeight, nodes[child].measured.y);
                    }
                }

                if(tableState.virtualFirstRow != 0 && virtualHeaderHeight <= 0.f)
                {
                    virtualHeaderHeight = node.style->metrics.controlHeight;
                }

                contentHeight = static_cast<float>(tableState.virtualRowCount) * std::max(tableState.virtualRowHeight, node.tableOptions.rowMinimumHeight);

                if(tableState.virtualFirstRow != 0)
                {
                    contentHeight += virtualHeaderHeight;
                }

                size_t totalRows = static_cast<size_t>(tableState.virtualFirstRow) + tableState.virtualRowCount;

                if(totalRows > 1)
                {
                    contentHeight += nodeGap * static_cast<float>(totalRows - 1);
                }
            }
            else
            {
                if(rowHeights.empty() == true)
                {
                    rowHeights.resize(static_cast<size_t>(maximumRow) + 1, 0.f);
                }

                for(size_t row = 0; row != rowHeights.size(); ++row)
                {
                    float requested = row < tableState.requestedRowHeights.size() ? tableState.requestedRowHeights[row] : 0.f;
                    float padding = row < tableState.requestedRowPaddingY.size() && tableState.requestedRowPaddingY[row] >= 0.f ? tableState.requestedRowPaddingY[row] * 2.f : 0.f;
                    rowHeights[row] = std::max({rowHeights[row] + padding, requested, node.tableOptions.rowMinimumHeight});
                }
                rowY.assign(maximumRow + 1, childArea.y);
                float y = childArea.y;
                for(uint32_t rowIndex = 0; rowIndex <= maximumRow; ++rowIndex)
                {
                    rowY[rowIndex] = y;
                    y += rowHeights[rowIndex];

                    if(rowIndex != maximumRow)
                    {
                        y += nodeGap;
                    }
                }
                contentHeight = std::max(0.f, y - childArea.y);
            }

            auto rowHeight = [&](uint32_t row) noexcept
            {
                if(tableState.rowsVirtualized == false)
                {
                    auto returnedValue = row < rowHeights.size() ? rowHeights[row] : 0.f;

                    return returnedValue;
                }

                auto returnedValue = row < tableState.virtualFirstRow ? virtualHeaderHeight : std::max(tableState.virtualRowHeight, node.tableOptions.rowMinimumHeight);

                return returnedValue;
            };
            auto rowPosition = [&](uint32_t row) noexcept
            {
                if(tableState.rowsVirtualized == false)
                {
                    auto returnedValue = row < rowY.size() ? rowY[row] : childArea.y;

                    return returnedValue;
                }

                if(row < tableState.virtualFirstRow)
                {
                    return childArea.y;
                }

                float headerExtent = tableState.virtualFirstRow == 0 ? 0.f : virtualHeaderHeight + nodeGap;
                auto returnedValue = childArea.y + headerExtent + static_cast<float>(row - tableState.virtualFirstRow) * (std::max(tableState.virtualRowHeight, node.tableOptions.rowMinimumHeight) + nodeGap);

                return returnedValue;
            };

            bool allowHorizontal = node.tableOptions.scrollHorizontal;
            bool allowVertical = node.tableOptions.scrollVertical;
            float scrollbarSize = std::max(8.f, node.style->metrics.scrollbarWidth);
            bool verticalScrollbar = allowVertical && contentHeight > childArea.height;
            bool horizontalScrollbar = allowHorizontal && contentWidth > childArea.width - (verticalScrollbar ? scrollbarSize + nodeGap : 0.f);
            verticalScrollbar = allowVertical && contentHeight > childArea.height - (horizontalScrollbar ? scrollbarSize + nodeGap : 0.f);
            Rect tableViewport = {childArea.x, childArea.y, std::max(0.f, childArea.width - (verticalScrollbar ? scrollbarSize + nodeGap : 0.f)), std::max(0.f, childArea.height - (horizontalScrollbar ? scrollbarSize + nodeGap : 0.f))};
            node.childrenClip = Rect::intersection(node.childrenClip, tableViewport);
            tablePersistent.scrollRange = {allowHorizontal ? std::max(0.f, contentWidth - tableViewport.width) : 0.f, allowVertical ? std::max(0.f, contentHeight - tableViewport.height) : 0.f};

            if(tablePersistent.scrollToEndX == true)
            {
                tablePersistent.scrollTarget.x = tablePersistent.scrollRange.x;
                tablePersistent.scrollToEndX = false;
                tablePersistent.scrollTargetInitialized = true;
            }

            if(tablePersistent.scrollToEndY == true)
            {
                tablePersistent.scrollTarget.y = tablePersistent.scrollRange.y;
                tablePersistent.scrollToEndY = false;
                tablePersistent.scrollTargetInitialized = true;
            }

            tablePersistent.scrollPosition = {std::clamp(tablePersistent.scrollPosition.x, 0.f, tablePersistent.scrollRange.x), std::clamp(tablePersistent.scrollPosition.y, 0.f, tablePersistent.scrollRange.y)};

            if(tablePersistent.scrollTargetInitialized == false)
            {
                tablePersistent.scrollTarget = tablePersistent.scrollPosition;
                tablePersistent.scrollTargetInitialized = true;
            }

            tablePersistent.scrollTarget = {std::clamp(tablePersistent.scrollTarget.x, 0.f, tablePersistent.scrollRange.x), std::clamp(tablePersistent.scrollTarget.y, 0.f, tablePersistent.scrollRange.y)};
            tablePersistent.verticalScrollbarTrack = {};
            tablePersistent.verticalScrollbarThumb = {};
            tablePersistent.horizontalScrollbarTrack = {};
            tablePersistent.horizontalScrollbarThumb = {};

            if(verticalScrollbar == true)
            {
                tablePersistent.verticalScrollbarTrack = {node.bounds.right() - scrollbarSize, node.bounds.y, scrollbarSize, tableViewport.height};
                float trackExtent = tablePersistent.verticalScrollbarTrack.height;
                float thumbExtent = std::clamp(trackExtent * tableViewport.height / std::max(1.f, contentHeight), std::min(node.style->metrics.grabMinimumSize, trackExtent), trackExtent);
                float travel = std::max(0.f, trackExtent - thumbExtent);
                float offset = tablePersistent.scrollRange.y <= 0.f ? 0.f : travel * tablePersistent.scrollPosition.y / tablePersistent.scrollRange.y;
                tablePersistent.verticalScrollbarThumb = {tablePersistent.verticalScrollbarTrack.x, tablePersistent.verticalScrollbarTrack.y + offset, tablePersistent.verticalScrollbarTrack.width, thumbExtent};
            }

            if(horizontalScrollbar == true)
            {
                tablePersistent.horizontalScrollbarTrack = {node.bounds.x, node.bounds.bottom() - scrollbarSize, tableViewport.width, scrollbarSize};
                float trackExtent = tablePersistent.horizontalScrollbarTrack.width;
                float thumbExtent = std::clamp(trackExtent * tableViewport.width / std::max(1.f, contentWidth), std::min(node.style->metrics.grabMinimumSize, trackExtent), trackExtent);
                float travel = std::max(0.f, trackExtent - thumbExtent);
                float offset = tablePersistent.scrollRange.x <= 0.f ? 0.f : travel * tablePersistent.scrollPosition.x / tablePersistent.scrollRange.x;
                tablePersistent.horizontalScrollbarThumb = {tablePersistent.horizontalScrollbarTrack.x + offset, tablePersistent.horizontalScrollbarTrack.y, thumbExtent, tablePersistent.horizontalScrollbarTrack.height};
            }

            float frozenWidth = 0.f;
            for(uint32_t column = 0; column != columnCount; ++column)
            {
                const Context::TableColumnState & columnState = tableState.columns[column];

                if(columnState.options.enabled == true && columnState.options.visible == true && columnState.order < node.tableOptions.frozenColumns)
                {
                    frozenWidth = std::max(frozenWidth, columnX[column] - childArea.x + columnState.resolvedWidth + horizontalGap);
                }
            }
            float frozenHeight = 0.f;
            size_t rowCount = tableState.rowsVirtualized ? static_cast<size_t>(tableState.virtualFirstRow) + tableState.virtualRowCount : rowHeights.size();
            for(uint32_t rowIndex = 0; rowIndex < std::min<uint32_t>(node.tableOptions.frozenRows, static_cast<uint32_t>(rowCount)); ++rowIndex)
            {
                frozenHeight += rowHeight(rowIndex);

                if(rowIndex + 1 < node.tableOptions.frozenRows)
                {
                    frozenHeight += nodeGap;
                }
            }

            for(uint32_t column = 0; column != columnCount; ++column)
            {
                Context::TableColumnState & columnState = tableState.columns[column];
                columnState.bodyBounds = {};

                if(columnState.options.enabled == false)
                {
                    continue;
                }

                if(columnState.options.visible == false)
                {
                    continue;
                }

                bool frozenColumn = columnState.order < node.tableOptions.frozenColumns;
                Rect columnBounds = {columnX[column] - (frozenColumn ? 0.f : tablePersistent.scrollPosition.x), tableViewport.y, columnState.resolvedWidth, tableViewport.height};
                columnState.bodyBounds = Rect::intersection(columnBounds, node.childrenClip);
            }

            for(size_t child : children)
            {
                Node & tableChild = nodes[child];

                if(tableChild.kind == Detail::NodeKind::TableRow)
                {
                    bool frozenRow = tableChild.tableRow < node.tableOptions.frozenRows;
                    Rect rowClip = node.childrenClip;
                    Rect rowBounds = {tableViewport.x, rowPosition(tableChild.tableRow) - (frozenRow ? 0.f : tablePersistent.scrollPosition.y), tableViewport.width, rowHeight(tableChild.tableRow)};

                    if(Rect::intersection(rowBounds, rowClip).empty() == true)
                    {
                        cullNode(child, rowBounds);
                    }
                    else
                    {
                        arrangeNode(child, rowBounds, rowClip);
                    }

                    continue;
                }

                uint32_t column = std::min(tableChild.tableColumn, columnCount - 1);
                const Context::TableColumnState & columnState = tableState.columns[column];
                tableChild.visible = columnState.options.enabled && columnState.options.visible;

                if(tableChild.visible == false)
                {
                    continue;
                }

                bool frozenColumn = columnState.order < node.tableOptions.frozenColumns;
                bool frozenRow = tableChild.tableRow < node.tableOptions.frozenRows;
                Rect cellClip = node.childrenClip;

                if(frozenColumn == false && frozenWidth > 0.f)
                {
                    float left = tableViewport.x + frozenWidth;
                    float right = cellClip.right();
                    cellClip.x = std::max(cellClip.x, left);
                    cellClip.width = std::max(0.f, right - cellClip.x);
                }

                if(frozenRow == false && frozenHeight > 0.f)
                {
                    float top = tableViewport.y + frozenHeight;
                    float bottom = cellClip.bottom();
                    cellClip.y = std::max(cellClip.y, top);
                    cellClip.height = std::max(0.f, bottom - cellClip.y);
                }

                float rowPaddingY = tableChild.tableRow < tableState.requestedRowPaddingY.size() && tableState.requestedRowPaddingY[tableChild.tableRow] >= 0.f ? tableState.requestedRowPaddingY[tableChild.tableRow] : 0.f;
                Rect cellBounds = {columnX[column] - (frozenColumn ? 0.f : tablePersistent.scrollPosition.x) + tableChild.layout.offset.x, rowPosition(tableChild.tableRow) - (frozenRow ? 0.f : tablePersistent.scrollPosition.y) + tableChild.layout.offset.y + rowPaddingY, columnState.resolvedWidth, std::max(0.f, rowHeight(tableChild.tableRow) - rowPaddingY * 2.f)};

                if(node.tableOptions.clipCells == true && columnState.options.clip == true)
                {
                    cellClip = Rect::intersection(cellClip, cellBounds);
                }

                if(Rect::intersection(cellBounds, cellClip).empty() == true)
                {
                    cullNode(child, cellBounds);
                }
                else
                {
                    arrangeNode(child, cellBounds, cellClip);

                    if(tableChild.kind == Detail::NodeKind::Tree)
                    {
                        Rect rowSpan = {tableViewport.x, tableChild.bounds.y, tableViewport.width, tableChild.style->metrics.controlHeight};

                        if(tableChild.treeSpanAllColumns == true)
                        {
                            tableChild.treeFrameBounds = rowSpan;
                            tableChild.treeFrameClip = node.childrenClip;
                        }

                        if(tableChild.treeLabelSpanAllColumns == true)
                        {
                            tableChild.treeLabelClip = node.childrenClip;
                        }

                        if(tableChild.persistentState != nullptr)
                        {
                            tableChild.persistentState->lastBounds = tableChild.treeFrameBounds;
                            tableChild.persistentState->lastClip = tableChild.treeFrameClip;
                        }
                    }
                    else if(tableChild.kind == Detail::NodeKind::Selectable && tableChild.selectableSpanAllColumns == true)
                    {
                        tableChild.bounds = {tableViewport.x, tableChild.bounds.y, tableViewport.width, tableChild.bounds.height};
                        tableChild.clip = node.childrenClip;
                        tableChild.visualClip = node.childrenClip;

                        if(tableChild.persistentState != nullptr)
                        {
                            tableChild.persistentState->lastBounds = tableChild.bounds;
                            tableChild.persistentState->lastClip = tableChild.clip;
                        }
                    }
                }

                if(tableChild.tableRow < tableState.headerRowCount && tableState.headersSubmitted == true)
                {
                    tableState.columns[column].lastBounds = tableChild.bounds;
                }
            }

            return;
        }

        bool horizontal = Detail::isHorizontal(node.kind, node.layout.orientation) || (Detail::scrollsContent(node) && node.layout.orientation == Orientation::Horizontal);
        bool bothCollapsedVertical = node.kind == Detail::NodeKind::Split && node.layout.orientation == Orientation::Vertical && children.size() >= 2 && nodes[children.front()].kind == Detail::NodeKind::Window && nodes[children.front()].windowCollapsed && nodes[children[1]].kind == Detail::NodeKind::Window && nodes[children[1]].windowCollapsed;
        float spacing = node.kind == Detail::NodeKind::Split ? 0.f : nodeGap;
        float availablePrimary = (horizontal ? childArea.width : childArea.height) - spacing * static_cast<float>(children.size() - 1);
        float fixedPrimary = 0.f;
        size_t fillCount = 0;
        for(size_t child : children)
        {
            const Dimension & dimension = horizontal ? nodes[child].layout.width : nodes[child].layout.height;

            if(dimension.rule == SizeRule::Fill)
            {
                ++fillCount;
            }
            else
            {
                fixedPrimary += resolveDimension(dimension, horizontal ? nodes[child].measured.x : nodes[child].measured.y, availablePrimary);
            }
        }
        float fillSize = fillCount == 0 ? 0.f : std::max(0.f, availablePrimary - fixedPrimary) / static_cast<float>(fillCount);
        float cursor = horizontal ? childArea.x : childArea.y;

        for(size_t childIndex = 0; childIndex != children.size(); ++childIndex)
        {
            Node & child = nodes[children[childIndex]];
            const Dimension & primaryDimension = horizontal ? child.layout.width : child.layout.height;
            float primary = primaryDimension.rule == SizeRule::Fill ? fillSize : resolveDimension(primaryDimension, horizontal ? child.measured.x : child.measured.y, availablePrimary);

            if(node.kind == Detail::NodeKind::Split && children.size() >= 2)
            {
                float total = std::max(0.f, availablePrimary - node.style->metrics.splitterWidth);
                float minimumRatio = total <= 0.f ? 0.f : std::clamp(node.splitMinimumFirst / total, 0.f, 1.f);
                float maximumRatio = total <= 0.f ? 1.f : std::clamp(1.f - node.splitMinimumSecond / total, 0.f, 1.f);
                float lowerRatio = std::min(minimumRatio, maximumRatio);
                float upperRatio = std::max(minimumRatio, maximumRatio);
                float ratio = std::clamp(node.layout.splitRatio, lowerRatio, upperRatio);
                primary = childIndex == 0 ? total * ratio : total * (1.f - ratio);

                if(node.layout.orientation == Orientation::Vertical)
                {
                    const Node & first = nodes[children.front()];
                    const Node & second = nodes[children[1]];
                    bool firstCollapsed = first.kind == Detail::NodeKind::Window && first.windowCollapsed;
                    bool secondCollapsed = second.kind == Detail::NodeKind::Window && second.windowCollapsed;

                    if(firstCollapsed == true && secondCollapsed == true)
                    {
                        primary = node.style->metrics.windowTitleHeight;
                    }
                    else if(firstCollapsed != secondCollapsed)
                    {
                        float collapsedHeight = node.style->metrics.windowTitleHeight;
                        primary = childIndex == 0 ? (firstCollapsed ? collapsedHeight : std::max(0.f, total - collapsedHeight)) : (secondCollapsed ? collapsedHeight : std::max(0.f, total - collapsedHeight));
                    }
                }
            }
            else if(node.kind == Detail::NodeKind::Split && children.size() == 1)
            {
                primary = availablePrimary;
            }

            if(Detail::scrollsContent(node) == false && node.kind != Detail::NodeKind::Split)
            {
                float primaryEnd = horizontal ? childArea.right() : childArea.bottom();
                float remainingGaps = nodeGap * static_cast<float>(children.size() - childIndex - 1);
                float remainingPrimary = std::max(0.f, primaryEnd - cursor - remainingGaps);
                primary = std::min(primary, remainingPrimary);
            }

            const Dimension & secondaryDimension = horizontal ? child.layout.height : child.layout.width;
            float availableSecondary = horizontal ? childArea.height : childArea.width;
            float secondary = resolveDimension(secondaryDimension, horizontal ? child.measured.y : child.measured.x, availableSecondary);

            if(secondaryDimension.rule == SizeRule::Fill)
            {
                secondary = availableSecondary;
            }

            if(node.kind == Detail::NodeKind::Split)
            {
                secondary = availableSecondary;
            }

            secondary = std::min(secondary, availableSecondary);

            // Rows use the same cross-axis centering as framed Mosaic controls. A short text
            // label beside a taller input therefore shares its visual center instead of being
            // pinned to the input's top edge.
            float crossOffset = 0.f;

            if(node.kind == Detail::NodeKind::Row)
            {
                switch(node.layout.crossAxisAlignment)
                {
                case CrossAxisAlignment::Start:
                    break;
                case CrossAxisAlignment::Center:
                    crossOffset = std::max(0.f, (availableSecondary - secondary) * 0.5f);
                    break;
                case CrossAxisAlignment::End:
                    crossOffset = std::max(0.f, availableSecondary - secondary);
                    break;
                case CrossAxisAlignment::Baseline:
                    if(node.baselineValid == true && child.baselineValid == true)
                    {
                        crossOffset = std::max(0.f, node.baseline - node.layout.padding.top - child.baseline);
                    }

                    break;
                }
            }

            float childCursor = bothCollapsedVertical && childIndex == 1 ? childArea.bottom() - primary : cursor;
            Rect childBounds = horizontal ? Rect{childCursor + child.layout.offset.x, childArea.y + crossOffset + child.layout.offset.y, primary, secondary} : Rect{childArea.x + child.layout.offset.x, childCursor + child.layout.offset.y, secondary, primary};

            if(Rect::intersection(childBounds, node.childrenClip).empty() == true)
            {
                cullNode(children[childIndex], childBounds);
            }
            else
            {
                arrangeNode(children[childIndex], childBounds, node.childrenClip);
            }

            cursor += primary + (node.kind == Detail::NodeKind::Split ? node.style->metrics.splitterWidth : nodeGap);
        }

        if(node.kind == Detail::NodeKind::Split && children.size() >= 2)
        {
            Persistent & persistentState = nodeState == nullptr ? state(node) : *nodeState;

            if(bothCollapsedVertical == true)
            {
                persistentState.splitterBounds = {};
            }
            else
            {
                const Rect & firstBounds = nodes[children.front()].bounds;
                persistentState.splitterBounds = horizontal ? Rect{firstBounds.right(), childArea.y, node.style->metrics.splitterWidth, childArea.height} : Rect{childArea.x, firstBounds.bottom(), childArea.width, node.style->metrics.splitterWidth};
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
