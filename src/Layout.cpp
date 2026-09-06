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
            return kind <= NodeKind::Style || kind == NodeKind::Tree || kind == NodeKind::Table || kind == NodeKind::TableRow || kind == NodeKind::TableCell || kind == NodeKind::Canvas || kind == NodeKind::NativeSurface;
        }
        //////////////////////////////////////////////////////////////////////////
        bool clipsDescendants(NodeKind kind) noexcept
        {
            return kind == NodeKind::Root || kind == NodeKind::Window || kind == NodeKind::Scroll || kind == NodeKind::Clip || kind == NodeKind::Table || kind == NodeKind::TableRow || kind == NodeKind::TableCell || kind == NodeKind::Canvas || kind == NodeKind::NativeSurface;
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
            case NodeKind::NativeSurface:
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
            case NodeKind::NativeSurface:
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
            auto returnedValue = node.kind == NodeKind::Scroll || (node.kind == NodeKind::Window && node.windowData().scrollable == true);

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
            auto returnedValue = std::max(0.f, available + dimension.value);

            return returnedValue;
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
                node.valueData().textWrapWidth = std::max(1.f, resolvedWidth - node.layout.padding.left - node.layout.padding.right);
                prepareText(node, node.label);
            }

            contentSize = node.textSize;

            if(node.alignTextToFramePadding == true)
            {
                contentSize.y = std::max(contentSize.y, node.style->metrics.lineHeight + node.style->metrics.framePadding.top + node.style->metrics.framePadding.bottom);
            }

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
                float shortcutExtent = node.valueText.empty() == true ? 0.f : node.valueData().valueTextSize.x + node.style->metrics.innerSpacing.x;
                float submenuExtent = node.menuSubmenu ? node.style->metrics.controlHeight * 0.75f : 0.f;
                contentSize.x += checkExtent + shortcutExtent + submenuExtent;
            }

            if(node.kind == Detail::NodeKind::Selectable && node.tableItem().header == true && node.tableItem().columnOptions.angledHeader == true)
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
            float previewWidth = node.comboShowPreview ? (node.comboWidthFitPreview ? node.valueData().valueTextSize.x + node.style->metrics.framePadding.left + node.style->metrics.framePadding.right : std::max(200.f, node.style->metrics.minimumControlWidth) - arrowWidth) : 0.f;
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
            contentSize = {node.valueData().scalar, node.valueData().scalar};
            break;
        case Detail::NodeKind::Image:
            contentSize = node.measured;
            break;
        case Detail::NodeKind::Canvas:
        case Detail::NodeKind::NativeSurface:
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
            float flowLineWidth = 0.f;
            float flowLineHeight = 0.f;
            float flowLineBaseline = 0.f;
            float flowLineBelowBaseline = 0.f;
            size_t flowLineCount = 0;
            Vec2 childAvailable = {std::min(available.x, std::max(0.f, node.layout.maximum.x - node.layout.padding.left - node.layout.padding.right)), std::min(available.y, std::max(0.f, node.layout.maximum.y - node.layout.padding.top - node.layout.padding.bottom))};

            if(node.layout.width.rule == SizeRule::Fixed || node.layout.width.rule == SizeRule::Fill || node.layout.width.rule == SizeRule::Percent)
            {
                childAvailable.x = std::max(0.f, resolveDimension(node.layout.width, available.x, available.x) - node.layout.padding.left - node.layout.padding.right);
            }

            if(node.layout.height.rule == SizeRule::Fixed || node.layout.height.rule == SizeRule::Fill || node.layout.height.rule == SizeRule::Percent)
            {
                childAvailable.y = std::max(0.f, resolveDimension(node.layout.height, available.y, available.y) - node.layout.padding.top - node.layout.padding.bottom);
            }

            bool horizontalLayout = Detail::isHorizontal(node.kind, node.layout.orientation);
            bool overlayLayout = Detail::measuresChildrenAsOverlay(node.kind);
            auto finishFlowLine = [&]()
            {
                if(flowLineCount == 0)
                {
                    return;
                }

                if(flowLineCount > 1)
                {
                    totalPrimary += nodeGap;
                }

                float alignedHeight = flowLineBaseline > 0.f ? flowLineBaseline + flowLineBelowBaseline : flowLineHeight;
                totalPrimary += std::max(flowLineHeight, alignedHeight);
                maximumSecondary = std::max(maximumSecondary, flowLineWidth);
            };

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

                    if(horizontalLayout == true)
                    {
                        totalPrimary += childSize.x;
                        maximumSecondary = std::max(maximumSecondary, childSize.y);
                    }
                    else if(overlayLayout == true)
                    {
                        totalPrimary = std::max(totalPrimary, childSize.x);
                        maximumSecondary = std::max(maximumSecondary, childSize.y);
                    }
                    else
                    {
                        bool continueLine = nodes[child].sameLine == true && flowLineCount != 0;

                        if(continueLine == false)
                        {
                            finishFlowLine();
                            ++flowLineCount;
                            flowLineWidth = childSize.x;
                            flowLineHeight = childSize.y;
                            flowLineBaseline = nodes[child].baselineValid == true ? nodes[child].baseline : 0.f;
                            flowLineBelowBaseline = nodes[child].baselineValid == true ? childSize.y - nodes[child].baseline : 0.f;
                        }
                        else
                        {
                            float spacing = nodes[child].sameLineSpacing < 0.f ? nodeGap : nodes[child].sameLineSpacing;
                            float position = nodes[child].sameLineOffset > 0.f ? nodes[child].sameLineOffset : flowLineWidth + spacing;
                            flowLineWidth = std::max(flowLineWidth, position + childSize.x);
                            flowLineHeight = std::max(flowLineHeight, childSize.y);

                            if(nodes[child].baselineValid == true)
                            {
                                flowLineBaseline = std::max(flowLineBaseline, nodes[child].baseline);
                                flowLineBelowBaseline = std::max(flowLineBelowBaseline, childSize.y - nodes[child].baseline);
                            }
                        }
                    }
                }

                child = nodes[child].nextSibling;
            }

            if(horizontalLayout == false && overlayLayout == false)
            {
                finishFlowLine();
            }

            if(childCount > 1)
            {
                if(overlayLayout == false && horizontalLayout == true)
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
                FloatVector cellCommittedHeights;
                FloatVector cellLineHeights;
                FloatVector cellLineWidths;
                FloatVector cellMaximumWidths;
                SizeVector cellLineCounts;
                FloatVector rowLastLineHeights;
                FloatVector rowLastLineBaselines;
                FloatVector rowLastLineBelowBaselines;
                FloatVector cellLineBaselines;
                FloatVector cellLineBelowBaselines;
                uint32_t maximumRow = 0;
                float headerHeight = 0.f;
                for(size_t tableChild = node.firstChild; tableChild != std::numeric_limits<size_t>::max(); tableChild = nodes[tableChild].nextSibling)
                {
                    if(nodes[tableChild].visible == false)
                    {
                        continue;
                    }

                    uint32_t column = std::min(nodes[tableChild].tableItem().column, columns - 1);

                    if(nodes[tableChild].kind == Detail::NodeKind::TableRow)
                    {
                        maximumRow = std::max(maximumRow, nodes[tableChild].tableItem().row);
                        continue;
                    }

                    uint32_t row = nodes[tableChild].tableItem().row;
                    maximumRow = std::max(maximumRow, row);
                    size_t cellIndex = static_cast<size_t>(row) * columns + column;
                    size_t requiredCells = cellIndex + 1;

                    if(cellCommittedHeights.size() < requiredCells)
                    {
                        cellCommittedHeights.resize(requiredCells, 0.f);
                        cellLineHeights.resize(requiredCells, 0.f);
                        cellLineWidths.resize(requiredCells, 0.f);
                        cellMaximumWidths.resize(requiredCells, 0.f);
                        cellLineCounts.resize(requiredCells, 0);
                        cellLineBaselines.resize(requiredCells, 0.f);
                        cellLineBelowBaselines.resize(requiredCells, 0.f);
                    }

                    size_t requiredRows = static_cast<size_t>(row) + 1;

                    if(rowLastLineHeights.size() < requiredRows)
                    {
                        rowLastLineHeights.resize(requiredRows, 0.f);
                        rowLastLineBaselines.resize(requiredRows, 0.f);
                        rowLastLineBelowBaselines.resize(requiredRows, 0.f);
                    }

                    bool reusePreviousCellLine = nodes[tableChild].sameLine == true && cellLineCounts[cellIndex] == 0 && rowLastLineHeights[row] > 0.f;

                    if(reusePreviousCellLine == true)
                    {
                        cellLineCounts[cellIndex] = 1;
                        cellLineHeights[cellIndex] = rowLastLineHeights[row];
                        cellLineBaselines[cellIndex] = rowLastLineBaselines[row];
                        cellLineBelowBaselines[cellIndex] = rowLastLineBelowBaselines[row];
                    }

                    bool continueLine = nodes[tableChild].sameLine == true && cellLineCounts[cellIndex] != 0;

                    if(continueLine == true)
                    {
                        float spacing = nodes[tableChild].sameLineSpacing < 0.f ? nodeGap : nodes[tableChild].sameLineSpacing;
                        float position = 0.f;

                        if(reusePreviousCellLine == false)
                        {
                            position = nodes[tableChild].sameLineOffset > 0.f ? nodes[tableChild].sameLineOffset : cellLineWidths[cellIndex] + spacing;
                        }
                        else if(nodes[tableChild].sameLineOffset > 0.f)
                        {
                            position = nodes[tableChild].sameLineOffset;
                        }

                        cellLineWidths[cellIndex] = std::max(cellLineWidths[cellIndex], position + nodes[tableChild].measured.x);
                        cellLineHeights[cellIndex] = std::max(cellLineHeights[cellIndex], nodes[tableChild].measured.y);
                    }
                    else
                    {
                        if(cellLineCounts[cellIndex] != 0)
                        {
                            cellCommittedHeights[cellIndex] += cellLineHeights[cellIndex] + nodeGap;
                            cellMaximumWidths[cellIndex] = std::max(cellMaximumWidths[cellIndex], cellLineWidths[cellIndex]);
                        }

                        ++cellLineCounts[cellIndex];
                        cellLineWidths[cellIndex] = nodes[tableChild].measured.x;
                        cellLineHeights[cellIndex] = nodes[tableChild].measured.y;
                        cellLineBaselines[cellIndex] = 0.f;
                        cellLineBelowBaselines[cellIndex] = 0.f;
                    }

                    if(nodes[tableChild].baselineValid == true)
                    {
                        cellLineBaselines[cellIndex] = std::max(cellLineBaselines[cellIndex], nodes[tableChild].baseline);
                        cellLineBelowBaselines[cellIndex] = std::max(cellLineBelowBaselines[cellIndex], nodes[tableChild].measured.y - nodes[tableChild].baseline);
                        cellLineHeights[cellIndex] = std::max(cellLineHeights[cellIndex], cellLineBaselines[cellIndex] + cellLineBelowBaselines[cellIndex]);
                    }

                    rowLastLineHeights[row] = cellLineHeights[cellIndex];
                    rowLastLineBaselines[row] = cellLineBaselines[cellIndex];
                    rowLastLineBelowBaselines[row] = cellLineBelowBaselines[cellIndex];

                    bool headerWithoutWidth = tableState.headersSubmitted == true;
                    headerWithoutWidth = headerWithoutWidth == true && row < tableState.headerRowCount;
                    headerWithoutWidth = headerWithoutWidth == true && nodes[tableChild].tableItem().columnOptions.headerContributesToWidth == false;

                    if(headerWithoutWidth == false)
                    {
                        float cellWidth = std::max(cellMaximumWidths[cellIndex], cellLineWidths[cellIndex]);
                        intrinsicWidths[column] = std::max(intrinsicWidths[column], cellWidth);
                    }

                    if(tableState.rowsVirtualized == true)
                    {
                        if(row < tableState.virtualFirstRow)
                        {
                            headerHeight = std::max(headerHeight, nodes[tableChild].measured.y);
                        }
                    }
                    else
                    {
                        if(rowHeights.size() <= row)
                        {
                            rowHeights.resize(row + 1, 0.f);
                        }

                        float cellHeight = cellCommittedHeights[cellIndex] + cellLineHeights[cellIndex];
                        rowHeights[row] = std::max(rowHeights[row], cellHeight);
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

                    height += static_cast<float>(tableState.virtualRowCount) * std::max(tableState.virtualRowHeight, node.tableOptions().rowMinimumHeight);
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
                        height += std::max({rowHeights[row] + padding, requested, node.tableOptions().rowMinimumHeight});
                    }

                    if(rowHeights.empty() == false)
                    {
                        height += nodeGap * static_cast<float>(maximumRow);
                    }
                }

                contentSize = {std::max(contentSize.x, width), std::max(contentSize.y, height)};

                if(node.tableOptions().scrollHorizontal == true)
                {
                    contentSize.x = std::max(contentSize.x, node.tableOptions().innerWidth);
                }

                if(node.tableOptions().extendHostHorizontal == false)
                {
                    contentSize.x = std::min(contentSize.x, available.x);
                }

                if(node.tableOptions().extendHostVertical == false)
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

        if(node.kind == Detail::NodeKind::Window && node.windowData().titleVisible == true)
        {
            contentSize.y += node.style->metrics.windowTitleHeight;
        }

        float labelWidth = Detail::itemLabelWidth(node);
        float measuredWidth = 0.f;

        if(node.valueData().itemWidthRequested == true)
        {
            float controlWidth = node.valueData().itemWidth < 0.f ? std::max(1.f, available.x + node.valueData().itemWidth) : std::max(1.f, node.valueData().itemWidth);
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
                    node.baseline = node.layout.padding.top + textBaseline + (node.alignTextToFramePadding == true ? node.style->metrics.framePadding.top : 0.f);
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

        if(node.kind == Detail::NodeKind::Window && node.windowData().titleVisible == true)
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
                node.mutableTreeData().frameBounds = {node.bounds.x, node.bounds.y, node.bounds.width, node.style->metrics.controlHeight};

                if(node.treeData().spanLabelWidth == true)
                {
                    node.mutableTreeData().frameBounds.width = std::min(node.mutableTreeData().frameBounds.width, node.textSize.x + node.style->metrics.padding * 2.f + node.style->metrics.indent);
                }

                node.mutableTreeData().frameClip = node.clip;
                node.mutableTreeData().labelClip = node.clip;
                nodeState->lastBounds = node.mutableTreeData().frameBounds;
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
                nodeState->scrollData().orientation = node.scrollOptions().axes == ScrollAxes::Horizontal ? Orientation::Horizontal : Orientation::Vertical;
                nodeState->scrollData().trackBounds = {};
                nodeState->scrollData().thumbBounds = {};
                nodeState->scrollData().verticalTrack = {};
                nodeState->scrollData().verticalThumb = {};
                nodeState->scrollData().horizontalTrack = {};
                nodeState->scrollData().horizontalThumb = {};
            }
        }

        bool preserveEmptyScrollContent = Detail::scrollsContent(node) == true && node.windowData().contentSizeExplicit == true;

        if(node.firstChild == std::numeric_limits<size_t>::max() && preserveEmptyScrollContent == false)
        {
            if(Detail::scrollsContent(node) == true)
            {
                Persistent & persistentState = *nodeState;
                persistentState.scrollData().value = 0.f;
                persistentState.scrollData().extent = 0.f;
                persistentState.scrollData().position = {};
                persistentState.scrollData().target = {};
                persistentState.scrollData().velocity = {};
                persistentState.scrollData().range = {};
                persistentState.scrollData().contentSize = {};
                persistentState.scrollData().targetInitialized = false;
                persistentState.scrollData().toEndX = false;
                persistentState.scrollData().toEndY = false;
                persistentState.scrollData().draggingScrollbar = false;
            }

            return;
        }

        size_t fixedMenuChild = std::numeric_limits<size_t>::max();
        size_t childrenBegin = layoutChildren.size();
        for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
        {
            bool floatingRootChild = node.kind == Detail::NodeKind::Root && Detail::isFloatingRootChild(nodes[child].kind) == true;

            if(nodes[child].visible == true && floatingRootChild == false)
            {
                bool windowMenuContainer = node.kind == Detail::NodeKind::Window && node.windowData().menuBar == true;
                bool childMenuContainer = node.kind == Detail::NodeKind::Scroll;
                bool fixedWindowMenu = (windowMenuContainer == true || childMenuContainer == true) && nodes[child].menuBar == true && fixedMenuChild == std::numeric_limits<size_t>::max();

                if(fixedWindowMenu == true)
                {
                    fixedMenuChild = child;

                    continue;
                }

                layoutChildren.push_back(child);
            }
        }
        NodeIndexSpan children = childrenBegin == layoutChildren.size() ? NodeIndexSpan{} : NodeIndexSpan(layoutChildren.data() + childrenBegin, layoutChildren.size() - childrenBegin);

        bool preserveEmptyScrollChildren = Detail::scrollsContent(node) == true && node.windowData().contentSizeExplicit == true;

        if(children.empty() == true && preserveEmptyScrollChildren == false && fixedMenuChild == std::numeric_limits<size_t>::max())
        {
            if(Detail::scrollsContent(node) == true)
            {
                Persistent & persistentState = *nodeState;
                persistentState.scrollData().value = 0.f;
                persistentState.scrollData().extent = 0.f;
                persistentState.scrollData().position = {};
                persistentState.scrollData().target = {};
                persistentState.scrollData().velocity = {};
                persistentState.scrollData().range = {};
                persistentState.scrollData().contentSize = {};
                persistentState.scrollData().targetInitialized = false;
                persistentState.scrollData().toEndX = false;
                persistentState.scrollData().toEndY = false;
                persistentState.scrollData().draggingScrollbar = false;
            }

            return;
        }

        float nodeGap = gap(node);
        Rect childArea = node.content;
        Rect childClip = node.childrenClip;

        if(fixedMenuChild != std::numeric_limits<size_t>::max())
        {
            Node & menuNode = nodes[fixedMenuChild];
            float menuHeight = resolveDimension(menuNode.layout.height, menuNode.measured.y, childArea.height);
            menuHeight = std::clamp(menuHeight, 0.f, childArea.height);
            Rect menuBounds = {childArea.x, childArea.y, childArea.width, menuHeight};
            arrangeNode(fixedMenuChild, menuBounds, node.childrenClip);
            childArea.y += menuHeight;
            childArea.height = std::max(0.f, childArea.height - menuHeight);
            childClip = Rect::intersection(childClip, childArea);
        }

        if(node.kind == Detail::NodeKind::Tree)
        {
            float treeIndent = node.tableItem().columnOptions.indent == TableColumnIndent::Disable ? 0.f : node.style->metrics.indent;
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

            if(node.windowData().contentSizeExplicit == true)
            {
                contentWidth = std::max(contentWidth, node.windowData().contentSize.x);
                contentHeight = std::max(contentHeight, node.windowData().contentSize.y);
            }

            bool allowHorizontal = node.scrollOptions().axes == ScrollAxes::Horizontal || node.scrollOptions().axes == ScrollAxes::Both;
            bool allowVertical = node.scrollOptions().axes == ScrollAxes::Vertical || node.scrollOptions().axes == ScrollAxes::Both;
            ScrollbarVisibility horizontalVisibility = node.scrollOptions().horizontalScrollbar;
            ScrollbarVisibility verticalVisibility = node.scrollOptions().verticalScrollbar;

            if(node.scrollOptions().visibility != ScrollbarVisibility::Automatic)
            {
                horizontalVisibility = node.scrollOptions().visibility;
                verticalVisibility = node.scrollOptions().visibility;
            }

            bool forceHorizontalScrollbar = horizontalVisibility == ScrollbarVisibility::Always || node.scrollOptions().alwaysHorizontalScrollbar == true;
            bool forceVerticalScrollbar = verticalVisibility == ScrollbarVisibility::Always || node.scrollOptions().alwaysVerticalScrollbar == true;
            bool hideHorizontalScrollbar = horizontalVisibility == ScrollbarVisibility::Hidden;
            bool hideVerticalScrollbar = verticalVisibility == ScrollbarVisibility::Hidden;
            float scrollbarSize = std::max(8.f, node.style->metrics.scrollbarWidth);
            bool verticalScrollbar = allowVertical && hideVerticalScrollbar == false && (forceVerticalScrollbar || contentHeight > childArea.height);
            bool horizontalScrollbar = allowHorizontal && hideHorizontalScrollbar == false && (forceHorizontalScrollbar || contentWidth > childArea.width - (verticalScrollbar ? scrollbarSize + nodeGap : 0.f));
            verticalScrollbar = allowVertical && hideVerticalScrollbar == false && (forceVerticalScrollbar || contentHeight > childArea.height - (horizontalScrollbar ? scrollbarSize + nodeGap : 0.f));

            Rect viewportArea = {childArea.x, childArea.y, std::max(0.f, childArea.width - (verticalScrollbar ? scrollbarSize + nodeGap : 0.f)), std::max(0.f, childArea.height - (horizontalScrollbar ? scrollbarSize + nodeGap : 0.f))};
            persistentState.scrollData().contentSize = {contentWidth, contentHeight};
            childClip = Rect::intersection(node.childrenClip, viewportArea);
            node.childrenClip = childClip;
            persistentState.scrollData().range = {allowHorizontal ? std::max(0.f, contentWidth - viewportArea.width) : 0.f, allowVertical ? std::max(0.f, contentHeight - viewportArea.height) : 0.f};

            if(persistentState.scrollData().toEndX == true)
            {
                persistentState.scrollData().target.x = persistentState.scrollData().range.x;
                persistentState.scrollData().toEndX = false;
                persistentState.scrollData().targetInitialized = true;
            }

            if(persistentState.scrollData().toEndY == true)
            {
                persistentState.scrollData().target.y = persistentState.scrollData().range.y;
                persistentState.scrollData().toEndY = false;
                persistentState.scrollData().targetInitialized = true;
            }

            persistentState.scrollData().position = {std::clamp(persistentState.scrollData().position.x, 0.f, persistentState.scrollData().range.x), std::clamp(persistentState.scrollData().position.y, 0.f, persistentState.scrollData().range.y)};

            if(persistentState.scrollData().targetInitialized == false)
            {
                persistentState.scrollData().target = persistentState.scrollData().position;
                persistentState.scrollData().targetInitialized = true;
            }

            persistentState.scrollData().target = {std::clamp(persistentState.scrollData().target.x, 0.f, persistentState.scrollData().range.x), std::clamp(persistentState.scrollData().target.y, 0.f, persistentState.scrollData().range.y)};
            persistentState.scrollData().value = node.layout.orientation == Orientation::Vertical ? persistentState.scrollData().position.y : persistentState.scrollData().position.x;
            persistentState.scrollData().extent = node.layout.orientation == Orientation::Vertical ? persistentState.scrollData().range.y : persistentState.scrollData().range.x;

            persistentState.scrollData().verticalTrack = {};
            persistentState.scrollData().verticalThumb = {};
            persistentState.scrollData().horizontalTrack = {};
            persistentState.scrollData().horizontalThumb = {};

            if(verticalScrollbar == true)
            {
                persistentState.scrollData().verticalTrack = {node.bounds.right() - scrollbarSize, viewportArea.y, scrollbarSize, viewportArea.height};
                float trackExtent = persistentState.scrollData().verticalTrack.height;
                float thumbExtent = std::clamp(trackExtent * viewportArea.height / std::max(1.f, contentHeight), std::min(node.style->metrics.grabMinimumSize, trackExtent), trackExtent);
                float travel = std::max(0.f, trackExtent - thumbExtent);
                float offset = persistentState.scrollData().range.y <= 0.f ? 0.f : travel * persistentState.scrollData().position.y / persistentState.scrollData().range.y;
                persistentState.scrollData().verticalThumb = {persistentState.scrollData().verticalTrack.x, persistentState.scrollData().verticalTrack.y + offset, persistentState.scrollData().verticalTrack.width, thumbExtent};
            }

            if(horizontalScrollbar == true)
            {
                persistentState.scrollData().horizontalTrack = {viewportArea.x, node.bounds.bottom() - scrollbarSize, viewportArea.width, scrollbarSize};
                float trackExtent = persistentState.scrollData().horizontalTrack.width;
                float thumbExtent = std::clamp(trackExtent * viewportArea.width / std::max(1.f, contentWidth), std::min(node.style->metrics.grabMinimumSize, trackExtent), trackExtent);
                float travel = std::max(0.f, trackExtent - thumbExtent);
                float offset = persistentState.scrollData().range.x <= 0.f ? 0.f : travel * persistentState.scrollData().position.x / persistentState.scrollData().range.x;
                persistentState.scrollData().horizontalThumb = {persistentState.scrollData().horizontalTrack.x + offset, persistentState.scrollData().horizontalTrack.y, thumbExtent, persistentState.scrollData().horizontalTrack.height};
            }

            bool primaryVertical = node.scrollOptions().axes != ScrollAxes::Horizontal;
            persistentState.scrollData().trackBounds = primaryVertical ? persistentState.scrollData().verticalTrack : persistentState.scrollData().horizontalTrack;
            persistentState.scrollData().thumbBounds = primaryVertical ? persistentState.scrollData().verticalThumb : persistentState.scrollData().horizontalThumb;

            if(persistentState.scrollData().range.x <= 0.f && persistentState.scrollData().range.y <= 0.f)
            {
                persistentState.scrollData().draggingScrollbar = false;
                persistentState.scrollData().draggingAxis = 0;
            }

            childArea = viewportArea;
            childArea.x -= persistentState.scrollData().position.x;
            childArea.y -= persistentState.scrollData().position.y;
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
            float horizontalGap = node.tableOptions().padInnerHorizontal ? nodeGap : 0.f;

            if(node.tableOptions().padOuterHorizontal == true)
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
            float availableWidth = std::max(0.f, (node.tableOptions().scrollHorizontal ? std::max(childArea.width, node.tableOptions().innerWidth) : childArea.width) - gaps);
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

            if(node.tableOptions().preciseWidths == false && stretchColumnCount != 0)
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

            if(node.tableOptions().keepColumnsVisible == true && node.tableOptions().scrollHorizontal == false)
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
                    if(nodes[child].tableItem().row < tableState.virtualFirstRow)
                    {
                        virtualHeaderHeight = std::max(virtualHeaderHeight, nodes[child].measured.y);
                    }
                }

                if(tableState.virtualFirstRow != 0 && virtualHeaderHeight <= 0.f)
                {
                    virtualHeaderHeight = node.style->metrics.controlHeight;
                }

                contentHeight = static_cast<float>(tableState.virtualRowCount) * std::max(tableState.virtualRowHeight, node.tableOptions().rowMinimumHeight);

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
                    rowHeights[row] = std::max({rowHeights[row] + padding, requested, node.tableOptions().rowMinimumHeight});
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

                auto returnedValue = row < tableState.virtualFirstRow ? virtualHeaderHeight : std::max(tableState.virtualRowHeight, node.tableOptions().rowMinimumHeight);

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
                auto returnedValue = childArea.y + headerExtent + static_cast<float>(row - tableState.virtualFirstRow) * (std::max(tableState.virtualRowHeight, node.tableOptions().rowMinimumHeight) + nodeGap);

                return returnedValue;
            };

            bool allowHorizontal = node.tableOptions().scrollHorizontal;
            bool allowVertical = node.tableOptions().scrollVertical;
            float scrollbarSize = std::max(8.f, node.style->metrics.scrollbarWidth);
            bool verticalScrollbar = allowVertical && contentHeight > childArea.height;
            bool horizontalScrollbar = allowHorizontal && contentWidth > childArea.width - (verticalScrollbar ? scrollbarSize + nodeGap : 0.f);
            verticalScrollbar = allowVertical && contentHeight > childArea.height - (horizontalScrollbar ? scrollbarSize + nodeGap : 0.f);
            Rect tableViewport = {childArea.x, childArea.y, std::max(0.f, childArea.width - (verticalScrollbar ? scrollbarSize + nodeGap : 0.f)), std::max(0.f, childArea.height - (horizontalScrollbar ? scrollbarSize + nodeGap : 0.f))};
            node.childrenClip = Rect::intersection(node.childrenClip, tableViewport);
            tablePersistent.scrollData().range = {allowHorizontal ? std::max(0.f, contentWidth - tableViewport.width) : 0.f, allowVertical ? std::max(0.f, contentHeight - tableViewport.height) : 0.f};

            if(tablePersistent.scrollData().toEndX == true)
            {
                tablePersistent.scrollData().target.x = tablePersistent.scrollData().range.x;
                tablePersistent.scrollData().toEndX = false;
                tablePersistent.scrollData().targetInitialized = true;
            }

            if(tablePersistent.scrollData().toEndY == true)
            {
                tablePersistent.scrollData().target.y = tablePersistent.scrollData().range.y;
                tablePersistent.scrollData().toEndY = false;
                tablePersistent.scrollData().targetInitialized = true;
            }

            tablePersistent.scrollData().position = {std::clamp(tablePersistent.scrollData().position.x, 0.f, tablePersistent.scrollData().range.x), std::clamp(tablePersistent.scrollData().position.y, 0.f, tablePersistent.scrollData().range.y)};

            if(tablePersistent.scrollData().targetInitialized == false)
            {
                tablePersistent.scrollData().target = tablePersistent.scrollData().position;
                tablePersistent.scrollData().targetInitialized = true;
            }

            tablePersistent.scrollData().target = {std::clamp(tablePersistent.scrollData().target.x, 0.f, tablePersistent.scrollData().range.x), std::clamp(tablePersistent.scrollData().target.y, 0.f, tablePersistent.scrollData().range.y)};
            tablePersistent.scrollData().verticalTrack = {};
            tablePersistent.scrollData().verticalThumb = {};
            tablePersistent.scrollData().horizontalTrack = {};
            tablePersistent.scrollData().horizontalThumb = {};

            if(verticalScrollbar == true)
            {
                tablePersistent.scrollData().verticalTrack = {node.bounds.right() - scrollbarSize, node.bounds.y, scrollbarSize, tableViewport.height};
                float trackExtent = tablePersistent.scrollData().verticalTrack.height;
                float thumbExtent = std::clamp(trackExtent * tableViewport.height / std::max(1.f, contentHeight), std::min(node.style->metrics.grabMinimumSize, trackExtent), trackExtent);
                float travel = std::max(0.f, trackExtent - thumbExtent);
                float offset = tablePersistent.scrollData().range.y <= 0.f ? 0.f : travel * tablePersistent.scrollData().position.y / tablePersistent.scrollData().range.y;
                tablePersistent.scrollData().verticalThumb = {tablePersistent.scrollData().verticalTrack.x, tablePersistent.scrollData().verticalTrack.y + offset, tablePersistent.scrollData().verticalTrack.width, thumbExtent};
            }

            if(horizontalScrollbar == true)
            {
                tablePersistent.scrollData().horizontalTrack = {node.bounds.x, node.bounds.bottom() - scrollbarSize, tableViewport.width, scrollbarSize};
                float trackExtent = tablePersistent.scrollData().horizontalTrack.width;
                float thumbExtent = std::clamp(trackExtent * tableViewport.width / std::max(1.f, contentWidth), std::min(node.style->metrics.grabMinimumSize, trackExtent), trackExtent);
                float travel = std::max(0.f, trackExtent - thumbExtent);
                float offset = tablePersistent.scrollData().range.x <= 0.f ? 0.f : travel * tablePersistent.scrollData().position.x / tablePersistent.scrollData().range.x;
                tablePersistent.scrollData().horizontalThumb = {tablePersistent.scrollData().horizontalTrack.x + offset, tablePersistent.scrollData().horizontalTrack.y, thumbExtent, tablePersistent.scrollData().horizontalTrack.height};
            }

            float frozenWidth = 0.f;
            for(uint32_t column = 0; column != columnCount; ++column)
            {
                const Context::TableColumnState & columnState = tableState.columns[column];

                if(columnState.options.enabled == true && columnState.options.visible == true && columnState.order < node.tableOptions().frozenColumns)
                {
                    frozenWidth = std::max(frozenWidth, columnX[column] - childArea.x + columnState.resolvedWidth + horizontalGap);
                }
            }
            float frozenHeight = 0.f;
            size_t rowCount = tableState.rowsVirtualized ? static_cast<size_t>(tableState.virtualFirstRow) + tableState.virtualRowCount : rowHeights.size();
            for(uint32_t rowIndex = 0; rowIndex < std::min<uint32_t>(node.tableOptions().frozenRows, static_cast<uint32_t>(rowCount)); ++rowIndex)
            {
                frozenHeight += rowHeight(rowIndex);

                if(rowIndex + 1 < node.tableOptions().frozenRows)
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

                bool frozenColumn = columnState.order < node.tableOptions().frozenColumns;
                Rect columnBounds = {columnX[column] - (frozenColumn ? 0.f : tablePersistent.scrollData().position.x), tableViewport.y, columnState.resolvedWidth, tableViewport.height};
                columnState.bodyBounds = Rect::intersection(columnBounds, node.childrenClip);
            }

            size_t cellCount = (static_cast<size_t>(tableState.maximumRow) + 1) * columnCount;
            FloatVector cellCursorY(cellCount, 0.f);
            FloatVector cellLineRight(cellCount, 0.f);
            FloatVector cellLineHeight(cellCount, 0.f);
            FloatVector cellLineBaseline(cellCount, 0.f);
            FloatVector cellLineBelowBaseline(cellCount, 0.f);
            SizeVector cellLineCounts(cellCount, 0);
            size_t tableRowCount = static_cast<size_t>(tableState.maximumRow) + 1;
            FloatVector rowLastLineHeight(tableRowCount, 0.f);
            FloatVector rowLastLineBaseline(tableRowCount, 0.f);
            FloatVector rowLastLineBelowBaseline(tableRowCount, 0.f);
            for(size_t child : children)
            {
                Node & tableChild = nodes[child];

                if(tableChild.kind == Detail::NodeKind::TableRow)
                {
                    bool frozenRow = tableChild.tableItem().row < node.tableOptions().frozenRows;
                    Rect rowClip = node.childrenClip;
                    Rect rowBounds = {tableViewport.x, rowPosition(tableChild.tableItem().row) - (frozenRow ? 0.f : tablePersistent.scrollData().position.y), tableViewport.width, rowHeight(tableChild.tableItem().row)};

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

                uint32_t column = std::min(tableChild.tableItem().column, columnCount - 1);
                const Context::TableColumnState & columnState = tableState.columns[column];
                tableChild.visible = columnState.options.enabled && columnState.options.visible;

                if(tableChild.visible == false)
                {
                    continue;
                }

                bool frozenColumn = columnState.order < node.tableOptions().frozenColumns;
                bool frozenRow = tableChild.tableItem().row < node.tableOptions().frozenRows;
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

                float rowPaddingY = tableChild.tableItem().row < tableState.requestedRowPaddingY.size() && tableState.requestedRowPaddingY[tableChild.tableItem().row] >= 0.f ? tableState.requestedRowPaddingY[tableChild.tableItem().row] : 0.f;
                size_t cellIndex = static_cast<size_t>(tableChild.tableItem().row) * columnCount + column;
                bool reusePreviousCellLine = tableChild.sameLine == true && cellLineCounts[cellIndex] == 0 && rowLastLineHeight[tableChild.tableItem().row] > 0.f;

                if(reusePreviousCellLine == true)
                {
                    cellLineCounts[cellIndex] = 1;
                    cellLineHeight[cellIndex] = rowLastLineHeight[tableChild.tableItem().row];
                    cellLineBaseline[cellIndex] = rowLastLineBaseline[tableChild.tableItem().row];
                    cellLineBelowBaseline[cellIndex] = rowLastLineBelowBaseline[tableChild.tableItem().row];
                }

                bool continueLine = tableChild.sameLine == true && cellLineCounts[cellIndex] != 0;

                if(continueLine == false)
                {
                    if(cellLineCounts[cellIndex] != 0)
                    {
                        cellCursorY[cellIndex] += cellLineHeight[cellIndex] + nodeGap;
                    }

                    ++cellLineCounts[cellIndex];
                    cellLineRight[cellIndex] = 0.f;
                    cellLineHeight[cellIndex] = 0.f;
                    cellLineBaseline[cellIndex] = 0.f;
                    cellLineBelowBaseline[cellIndex] = 0.f;
                }

                float spacing = tableChild.sameLineSpacing < 0.f ? nodeGap : tableChild.sameLineSpacing;
                float itemX = continueLine == true && reusePreviousCellLine == false ? cellLineRight[cellIndex] + spacing : 0.f;

                if(continueLine == true && tableChild.sameLineOffset > 0.f)
                {
                    itemX = tableChild.sameLineOffset;
                }

                float cellHeight = std::max(0.f, rowHeight(tableChild.tableItem().row) - rowPaddingY * 2.f);
                float availableWidth = std::max(0.f, columnState.resolvedWidth - itemX);
                float availableHeight = std::max(0.f, cellHeight - cellCursorY[cellIndex]);
                float itemWidth = resolveDimension(tableChild.layout.width, tableChild.measured.x, availableWidth);
                float itemHeight = resolveDimension(tableChild.layout.height, tableChild.measured.y, availableHeight);

                if(tableChild.layout.width.rule == SizeRule::Fill)
                {
                    itemWidth = availableWidth;
                }

                if(tableChild.layout.height.rule == SizeRule::Fill)
                {
                    itemHeight = availableHeight;
                }

                float lineBaseline = cellLineBaseline[cellIndex];

                if(tableChild.baselineValid == true)
                {
                    lineBaseline = std::max(lineBaseline, tableChild.baseline);
                }

                float baselineOffset = tableChild.baselineValid == true ? std::max(0.f, lineBaseline - tableChild.baseline) : 0.f;

                float cellX = columnX[column] - (frozenColumn ? 0.f : tablePersistent.scrollData().position.x);
                float cellY = rowPosition(tableChild.tableItem().row) - (frozenRow ? 0.f : tablePersistent.scrollData().position.y) + rowPaddingY;
                Rect cellBounds = {cellX + itemX + tableChild.layout.offset.x, cellY + cellCursorY[cellIndex] + baselineOffset + tableChild.layout.offset.y, itemWidth, itemHeight};
                cellLineRight[cellIndex] = std::max(cellLineRight[cellIndex], itemX + itemWidth);
                cellLineHeight[cellIndex] = std::max(cellLineHeight[cellIndex], itemHeight);

                if(tableChild.baselineValid == true)
                {
                    cellLineBaseline[cellIndex] = std::max(cellLineBaseline[cellIndex], tableChild.baseline);
                    cellLineBelowBaseline[cellIndex] = std::max(cellLineBelowBaseline[cellIndex], itemHeight - tableChild.baseline);
                    cellLineHeight[cellIndex] = std::max(cellLineHeight[cellIndex], cellLineBaseline[cellIndex] + cellLineBelowBaseline[cellIndex]);
                }

                rowLastLineHeight[tableChild.tableItem().row] = cellLineHeight[cellIndex];
                rowLastLineBaseline[tableChild.tableItem().row] = cellLineBaseline[cellIndex];
                rowLastLineBelowBaseline[tableChild.tableItem().row] = cellLineBelowBaseline[cellIndex];

                if(node.tableOptions().clipCells == true && columnState.options.clip == true)
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

                        if(tableChild.treeData().spanAllColumns == true)
                        {
                            tableChild.mutableTreeData().frameBounds = rowSpan;
                            tableChild.mutableTreeData().frameClip = node.childrenClip;
                        }

                        if(tableChild.treeData().labelSpanAllColumns == true)
                        {
                            tableChild.mutableTreeData().labelClip = node.childrenClip;
                        }

                        if(tableChild.persistentState != nullptr)
                        {
                            tableChild.persistentState->lastBounds = tableChild.mutableTreeData().frameBounds;
                            tableChild.persistentState->lastClip = tableChild.mutableTreeData().frameClip;
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

                if(tableChild.tableItem().row < tableState.headerRowCount && tableState.headersSubmitted == true)
                {
                    tableState.columns[column].lastBounds = tableChild.bounds;
                }
            }

            return;
        }

        bool horizontal = Detail::isHorizontal(node.kind, node.layout.orientation) || (Detail::scrollsContent(node) && node.layout.orientation == Orientation::Horizontal);
        bool bothCollapsedVertical = node.kind == Detail::NodeKind::Split && node.layout.orientation == Orientation::Vertical && children.size() >= 2 && nodes[children.front()].kind == Detail::NodeKind::Window && nodes[children.front()].windowData().collapsed && nodes[children[1]].kind == Detail::NodeKind::Window && nodes[children[1]].windowData().collapsed;
        bool hasSameLine = std::any_of(children.begin(), children.end(), [this](size_t child)
                                       {
                                           return nodes[child].sameLine;
                                       });

        if(horizontal == false && node.kind != Detail::NodeKind::Split && hasSameLine == true)
        {
            float cursorY = childArea.y;
            size_t lineBegin = 0;

            while(lineBegin < children.size())
            {
                size_t lineEnd = lineBegin + 1;

                while(lineEnd < children.size() && nodes[children[lineEnd]].sameLine == true)
                {
                    ++lineEnd;
                }

                float lineRight = 0.f;
                float lineHeight = 0.f;
                float lineBaseline = 0.f;
                float lineBelowBaseline = 0.f;
                for(size_t childIndex = lineBegin; childIndex != lineEnd; ++childIndex)
                {
                    Node & child = nodes[children[childIndex]];
                    float spacing = child.sameLineSpacing < 0.f ? nodeGap : child.sameLineSpacing;
                    float x = childIndex == lineBegin ? 0.f : child.sameLineOffset > 0.f ? child.sameLineOffset : lineRight + spacing;
                    float availableWidth = std::max(0.f, childArea.width - x);
                    float width = resolveDimension(child.layout.width, child.measured.x, availableWidth);

                    if(child.layout.width.rule == SizeRule::Fill)
                    {
                        width = availableWidth;
                    }

                    float height = resolveDimension(child.layout.height, child.measured.y, childArea.height);
                    lineRight = std::max(lineRight, x + width);
                    lineHeight = std::max(lineHeight, height);

                    if(child.baselineValid == true)
                    {
                        lineBaseline = std::max(lineBaseline, child.baseline);
                        lineBelowBaseline = std::max(lineBelowBaseline, height - child.baseline);
                    }
                }

                if(lineBaseline > 0.f)
                {
                    lineHeight = std::max(lineHeight, lineBaseline + lineBelowBaseline);
                }

                lineRight = 0.f;
                for(size_t childIndex = lineBegin; childIndex != lineEnd; ++childIndex)
                {
                    Node & child = nodes[children[childIndex]];
                    float spacing = child.sameLineSpacing < 0.f ? nodeGap : child.sameLineSpacing;
                    float x = childIndex == lineBegin ? 0.f : child.sameLineOffset > 0.f ? child.sameLineOffset : lineRight + spacing;
                    float availableWidth = std::max(0.f, childArea.width - x);
                    float width = resolveDimension(child.layout.width, child.measured.x, availableWidth);

                    if(child.layout.width.rule == SizeRule::Fill)
                    {
                        width = availableWidth;
                    }

                    float height = resolveDimension(child.layout.height, child.measured.y, lineHeight);
                    float baselineOffset = lineBaseline > 0.f && child.baselineValid == true ? lineBaseline - child.baseline : 0.f;
                    Rect childBounds = {childArea.x + x + child.layout.offset.x, cursorY + baselineOffset + child.layout.offset.y, width, height};

                    if(Rect::intersection(childBounds, node.childrenClip).empty() == true)
                    {
                        cullNode(children[childIndex], childBounds);
                    }
                    else
                    {
                        arrangeNode(children[childIndex], childBounds, node.childrenClip);
                    }

                    lineRight = std::max(lineRight, x + width);
                }

                cursorY += lineHeight + nodeGap;
                lineBegin = lineEnd;
            }

            return;
        }

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
                float minimumRatio = total <= 0.f ? 0.f : std::clamp(node.valueData().splitMinimumFirst / total, 0.f, 1.f);
                float maximumRatio = total <= 0.f ? 1.f : std::clamp(1.f - node.valueData().splitMinimumSecond / total, 0.f, 1.f);
                float lowerRatio = std::min(minimumRatio, maximumRatio);
                float upperRatio = std::max(minimumRatio, maximumRatio);
                float ratio = std::clamp(node.layout.splitRatio, lowerRatio, upperRatio);
                primary = childIndex == 0 ? total * ratio : total * (1.f - ratio);

                if(node.layout.orientation == Orientation::Vertical)
                {
                    const Node & first = nodes[children.front()];
                    const Node & second = nodes[children[1]];
                    bool firstCollapsed = first.kind == Detail::NodeKind::Window && first.windowData().collapsed;
                    bool secondCollapsed = second.kind == Detail::NodeKind::Window && second.windowData().collapsed;

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
