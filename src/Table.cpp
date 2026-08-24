#include "Context.hpp"
#include "ContextDetail.hpp"
#include "Layout.hpp"
#include "Utility.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool tableColumnRequestsOutput(Context * ui, const Context::Node & tableNode, const Context::TableState & tableState, uint32_t column)
        {
            if(column >= tableState.columns.size())
            {
                return true;
            }

            const Context::TableColumnState & columnState = tableState.columns[column];

            if(columnState.options.enabled == false)
            {
                return false;
            }

            if(columnState.options.visible == false)
            {
                return false;
            }

            if(tableNode.tableOptions.scrollHorizontal == false)
            {
                return true;
            }

            if(tableNode.tableOptions.clipCells == false)
            {
                return true;
            }

            if(columnState.options.clip == false)
            {
                return true;
            }

            if(columnState.order < tableNode.tableOptions.frozenColumns)
            {
                return true;
            }

            if(tableState.columns.size() == 1)
            {
                return true;
            }

            if(tableState.columnPositions.size() <= column)
            {
                return true;
            }

            if(columnState.resolvedWidth <= 0.f)
            {
                return true;
            }

            if((Detail::tableSizingFixed(columnState.options.sizing) == true && columnState.options.widthOrWeight <= 0.f))
            {
                return true;
            }

            const Context::Persistent * persistentState = ui->findState(tableNode.id);

            if(persistentState == nullptr)
            {
                return true;
            }

            if(persistentState->lastBounds.empty() == true)
            {
                return true;
            }

            if(persistentState->lastClip.empty() == true)
            {
                return true;
            }

            float scroll = columnState.order < tableNode.tableOptions.frozenColumns ? 0.f : persistentState->scrollPosition.x;
            Rect columnBounds = {tableState.columnPositions[column] - scroll, persistentState->lastBounds.y, columnState.resolvedWidth, persistentState->lastBounds.height};
            Rect viewport = Rect::intersection(persistentState->lastBounds, persistentState->lastClip);
            auto returnedValue = Rect::intersection(columnBounds, viewport).empty() == false;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        void sizeTableColumnToFit(Context::TableState & tableState, uint32_t column) noexcept
        {
            if(column >= tableState.columns.size())
            {
                return;
            }

            Context::TableColumnState & state = tableState.columns[column];
            state.options.sizing = TableSizing::Fixed;

            if(column < tableState.intrinsicWidths.size())
            {
                state.options.widthOrWeight = std::max(1.f, tableState.intrinsicWidths[column]);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void resetTableColumnOrder(Context::TableState & tableState) noexcept
        {
            for(size_t index = 0; index != tableState.columns.size(); ++index)
            {
                tableState.columns[index].order = static_cast<uint32_t>(index);
            }
            tableState.displayOrderDirty = true;
        }
        //////////////////////////////////////////////////////////////////////////
        void setTableSort(Context::TableState & tableState, uint32_t column, SortDirection direction) noexcept
        {
            if(direction == SortDirection::None)
            {
                tableState.sortSpecs.clear();
                tableState.sortSpecsDirty = true;

                return;
            }

            tableState.sortSpecs.clear();
            tableState.sortSpecs.push_back({column, direction, 0});
            tableState.sortSpecsDirty = true;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Scope table(Context * ui, StringView label, uint32_t columns, const LayoutOptions & layout, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::table(ui, label, columns, {}, layout, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope table(Context * ui, StringView label, uint32_t columns, const TableOptions & tableOptions, const LayoutOptions & layout, const SourceLocation & location)
    {
        LayoutOptions resolvedLayout = layout;
        resolvedLayout.columns = std::max(1U, columns);
        size_t node = ui->addNode(Detail::NodeKind::Table, {}, label, resolvedLayout, location, SemanticRole::Table);
        ui->nodes[node].tableSettingsId = tableOptions.settingsId;
        Context::TableState & tableState = ui->tableState(ui->nodes[node].id);
        bool settingsJustDisabled = tableOptions.saveSettings == false && tableState.options.saveSettings;
        tableState.options = tableOptions;
        tableState.currentRow = 0;
        tableState.currentColumn = 0;
        tableState.currentCellSubmitted = false;
        tableState.virtualRowCount = 0;
        tableState.virtualRowHeight = 0.f;
        tableState.virtualFirstRow = 0;
        tableState.headersSubmitted = false;
        tableState.angledHeadersSubmitted = false;
        tableState.submitAngledHeaders = false;
        tableState.headerRowCount = 0;
        tableState.rowsVirtualized = false;
        tableState.currentRowBackgroundsEnabled = {false, false};
        tableState.pendingCellBackgroundEnabled = false;
        tableState.requestedRowHeights.clear();
        tableState.requestedRowPaddingY.clear();

        if(settingsJustDisabled == true)
        {
            for(size_t index = 0; index != tableState.columns.size(); ++index)
            {
                tableState.columns[index] = {};
                tableState.columns[index].order = static_cast<uint32_t>(index);
            }
            tableState.sortSpecs.clear();
            tableState.sortSpecsDirty = true;
            tableState.displayOrderDirty = true;
        }

        if(tableState.columns.size() < resolvedLayout.columns)
        {
            size_t previousSize = tableState.columns.size();
            tableState.columns.resize(resolvedLayout.columns);
            tableState.displayOrderDirty = true;
            for(size_t index = previousSize; index != tableState.columns.size(); ++index)
            {
                tableState.columns[index].order = static_cast<uint32_t>(index);
            }
        }

        ui->nodes[node].tableOptions = tableOptions;
        ui->nodes[node].scrollOptions.axes = tableOptions.scrollHorizontal && tableOptions.scrollVertical ? ScrollAxes::Both : (tableOptions.scrollHorizontal ? ScrollAxes::Horizontal : ScrollAxes::Vertical);
        ui->nodes[node].scrollOptions.visibility = tableOptions.scrollHorizontal || tableOptions.scrollVertical ? ScrollbarVisibility::Automatic : ScrollbarVisibility::Hidden;

        if(tableOptions.scrollHorizontal == true || tableOptions.scrollVertical == true)
        {
            Context::Persistent & scrollState = ui->state(ui->nodes[node]);
            const PointerState * pointer = ui->input.primaryPointer();
            Rect hitBounds;
            uint8_t hitAxis = 0;

            if(pointer != nullptr && scrollState.verticalScrollbarTrack.contains(pointer->position) == true)
            {
                hitBounds = scrollState.verticalScrollbarTrack;
                hitAxis = 2;
            }
            else if(pointer != nullptr && scrollState.horizontalScrollbarTrack.contains(pointer->position) == true)
            {
                hitBounds = scrollState.horizontalScrollbarTrack;
                hitAxis = 1;
            }

            Response response;
            response.id = ui->nodes[node].id;

            if(hitAxis != 0 || ui->captured == response.id)
            {
                response = ui->interact(node, false, hitAxis == 0 ? nullptr : &hitBounds);
            }

            if(pointer != nullptr && response.pressed() == true && hitAxis != 0)
            {
                bool vertical = hitAxis == 2;
                const Rect & track = vertical ? scrollState.verticalScrollbarTrack : scrollState.horizontalScrollbarTrack;
                const Rect & thumb = vertical ? scrollState.verticalScrollbarThumb : scrollState.horizontalScrollbarThumb;

                if(thumb.contains(pointer->position) == true)
                {
                    scrollState.scrollbarDragOffset = (vertical ? pointer->position.y - thumb.y : pointer->position.x - thumb.x);
                    scrollState.draggingScrollbar = true;
                    scrollState.draggingScrollAxis = hitAxis;
                }
                else
                {
                    float & position = vertical ? scrollState.scrollPosition.y : scrollState.scrollPosition.x;
                    float extent = vertical ? scrollState.scrollRange.y : scrollState.scrollRange.x;
                    float pointerPosition = vertical ? pointer->position.y : pointer->position.x;
                    float thumbStart = vertical ? thumb.y : thumb.x;
                    float page = vertical ? scrollState.lastBounds.height : scrollState.lastBounds.width;
                    float previous = position;

                    if(ui->configuration.scrollbarScrollByPage == true)
                    {
                        position = std::clamp(position + (pointerPosition < thumbStart ? -page : page), 0.f, extent);
                    }
                    else
                    {
                        float trackStart = vertical ? track.y : track.x;
                        float trackExtent = vertical ? track.height : track.width;
                        float thumbExtent = vertical ? thumb.height : thumb.width;
                        float travel = std::max(0.f, trackExtent - thumbExtent);
                        position = travel <= 0.f ? 0.f : std::clamp((pointerPosition - trackStart - thumbExtent * 0.5f) / travel, 0.f, 1.f) * extent;
                    }

                    scrollState.scrollTarget = scrollState.scrollPosition;
                    scrollState.scrollVelocity = {};
                    scrollState.scrollTargetInitialized = true;

                    if(position != previous)
                    {
                        response.flags |= 1U << 6;
                    }
                }
            }

            if(pointer != nullptr && ui->captured == response.id && scrollState.draggingScrollbar == true && pointer->isDown() == true)
            {
                bool vertical = scrollState.draggingScrollAxis == 2;
                const Rect & track = vertical ? scrollState.verticalScrollbarTrack : scrollState.horizontalScrollbarTrack;
                const Rect & thumb = vertical ? scrollState.verticalScrollbarThumb : scrollState.horizontalScrollbarThumb;
                float & position = vertical ? scrollState.scrollPosition.y : scrollState.scrollPosition.x;
                float previous = position;
                position = Detail::scrollbarScrollAtPointer(track, thumb, vertical ? scrollState.scrollRange.y : scrollState.scrollRange.x, vertical, scrollState.scrollbarDragOffset, pointer->position);
                scrollState.scrollTarget = scrollState.scrollPosition;
                scrollState.scrollVelocity = {};
                scrollState.scrollTargetInitialized = true;

                if(position != previous)
                {
                    response.flags |= 1U << 6;
                }
            }

            Vec2 delta;
            float step = std::max(12.f, ui->currentStyle->metrics.controlHeight);
            float page = std::max(step, scrollState.lastBounds.height);

            if(ui->focused == ui->nodes[node].id)
            {
                if(tableOptions.scrollHorizontal == true && ui->input.keyPressed(KeyCode::Left) == true)
                {
                    delta.x -= step;
                }

                if(tableOptions.scrollHorizontal == true && ui->input.keyPressed(KeyCode::Right) == true)
                {
                    delta.x += step;
                }

                if(tableOptions.scrollVertical == true && ui->input.keyPressed(KeyCode::Up) == true)
                {
                    delta.y -= step;
                }

                if(tableOptions.scrollVertical == true && ui->input.keyPressed(KeyCode::Down) == true)
                {
                    delta.y += step;
                }

                if(tableOptions.scrollVertical == true && ui->input.keyPressed(KeyCode::PageUp) == true)
                {
                    delta.y -= page;
                }

                if(tableOptions.scrollVertical == true && ui->input.keyPressed(KeyCode::PageDown) == true)
                {
                    delta.y += page;
                }

                if(ui->input.keyPressed(KeyCode::Home) == true)
                {
                    scrollState.scrollPosition = {};
                    scrollState.scrollTarget = {};
                    scrollState.scrollVelocity = {};
                    scrollState.scrollTargetInitialized = true;
                }

                if(ui->input.keyPressed(KeyCode::End) == true)
                {
                    scrollState.scrollPosition = scrollState.scrollRange;
                    scrollState.scrollTarget = scrollState.scrollPosition;
                    scrollState.scrollVelocity = {};
                    scrollState.scrollTargetInitialized = true;
                }
            }

            if(delta != Vec2{})
            {
                Vec2 previous = scrollState.scrollPosition;
                scrollState.scrollPosition = {std::clamp(previous.x + delta.x, 0.f, scrollState.scrollRange.x), std::clamp(previous.y + delta.y, 0.f, scrollState.scrollRange.y)};
                scrollState.scrollTarget = scrollState.scrollPosition;
                scrollState.scrollVelocity = {};
                scrollState.scrollTargetInitialized = true;

                if(scrollState.scrollPosition != previous)
                {
                    response.flags |= 1U << 6;
                }
            }

            if(response.released() == true)
            {
                scrollState.draggingScrollbar = false;
                scrollState.draggingScrollAxis = 0;
            }

            if(response.changed() == true)
            {
                const Context::Node & tableNode = ui->nodes[node];
                ui->frame.events.push_back({EventType::Change, tableNode.id, ui->nodePath(ui->nodes[node]), tableNode.file, tableNode.line, ui->input.timestamp});
            }

            ui->nodes[node].response = response;
        }

        if(tableOptions.contextMenuInBody == true)
        {
            const PointerState * pointer = ui->input.primaryPointer();
            const Context::Persistent & tablePersistent = ui->state(ui->nodes[node]);

            if(pointer != nullptr && pointer->isPressed(PointerButton::Secondary) == true && tablePersistent.lastBounds.contains(pointer->position) == true)
            {
                Context::TableState & state = ui->tableState(ui->nodes[node].id);
                state.contextColumn = std::numeric_limits<uint32_t>::max();
                for(size_t column = 0; column != state.columns.size(); ++column)
                {
                    if(state.columns[column].bodyBounds.contains(pointer->position) == true)
                    {
                        state.contextColumn = static_cast<uint32_t>(column);
                        break;
                    }
                }
                PopupOptions popupOptions;
                popupOptions.owner = ui->nodes[node].id;
                popupOptions.anchor = {pointer->position.x, pointer->position.y, 1.f, 1.f};
                popupOptions.placement = PopupPlacement::Cursor;
                Mosaic::openPopup(ui, Key("Table columns"), popupOptions);
            }
        }

        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    void tableSetupColumn(Context * ui, uint32_t column, StringView label, const TableColumnOptions & options)
    {
        if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableSetupColumn must be called inside table() with a valid column");

            return;
        }

        if(column >= ui->nodes[ui->currentParent].layout.columns)
        {
            ui->frame.diagnostics.emplace_back("tableSetupColumn must be called inside table() with a valid column");

            return;
        }

        Context::TableState & tableState = ui->tableState(ui->nodes[ui->currentParent].id);

        if(tableState.columns.size() <= column)
        {
            tableState.columns.resize(column + 1);
        }

        Context::TableColumnState & columnState = tableState.columns[column];

        if(columnState.label != label)
        {
            columnState.label.assign(label);
        }

        if(columnState.configured == false && columnState.restored == false)
        {
            columnState.options = options;
        }
        else
        {
            // Capabilities belong to the application. Width, order and visibility belong to
            // the user's persistent table configuration after the first submission.
            columnState.options.sortable = options.sortable;
            columnState.options.enabled = options.enabled;
            columnState.options.sortAscending = options.sortAscending;
            columnState.options.sortDescending = options.sortDescending;
            columnState.options.resizable = options.resizable;
            columnState.options.reorderable = options.reorderable;
            columnState.options.hideable = options.hideable;
            columnState.options.clip = options.clip;
            columnState.options.defaultSort = options.defaultSort;
            columnState.options.indent = options.indent;
            columnState.options.preferredSort = options.preferredSort;
            columnState.options.headerLabelVisible = options.headerLabelVisible;
            columnState.options.headerContributesToWidth = options.headerContributesToWidth;
            columnState.options.angledHeader = options.angledHeader;
            columnState.options.itemWidth = options.itemWidth;
        }

        columnState.configured = true;

        if(columnState.order >= tableState.columns.size())
        {
            columnState.order = column;
        }

        bool applyDefaultSort = options.defaultSort;

        if(tableState.sortSpecs.empty() == false)
        {
            applyDefaultSort = false;
        }

        if(options.enabled == false)
        {
            applyDefaultSort = false;
        }

        if(options.sortable == false)
        {
            applyDefaultSort = false;
        }

        if(options.sortAscending == false)
        {
            if(options.sortDescending == false)
            {
                applyDefaultSort = false;
            }
        }

        if(applyDefaultSort == true)
        {
            SortDirection direction = options.preferredSort;

            if(direction == SortDirection::Descending && options.sortDescending == false)
            {
                direction = SortDirection::Ascending;
            }

            if(direction == SortDirection::Ascending && options.sortAscending == false)
            {
                direction = SortDirection::Descending;
            }

            tableState.sortSpecs.push_back({column, direction, 0});
            tableState.sortSpecsDirty = true;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void tableHeadersRow(Context * ui)
    {
        if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableHeadersRow must be called inside table()");

            return;
        }

        size_t tableIndex = ui->currentParent;
        Id tableId = ui->nodes[tableIndex].id;
        Context::TableState & initialState = ui->tableState(tableId);
        bool angledOnly = initialState.submitAngledHeaders;
        uint32_t headerRow = angledOnly ? 0U : initialState.angledHeadersSubmitted ? 1U : 0U;

        if(ui->nodes[tableIndex].tableOptions.headers == false)
        {
            Context::TableState & tableState = ui->tableState(tableId);
            tableState.currentRow = headerRow;
            tableState.currentColumn = 0;
            tableState.currentCellSubmitted = false;
            tableState.headersSubmitted = false;

            return;
        }

        uint32_t columnCount = ui->nodes[tableIndex].layout.columns;
        {
            Context::TableState & tableState = ui->tableState(tableId);
            tableState.currentRow = 0;
            tableState.currentColumn = 0;
            tableState.currentCellSubmitted = false;
            tableState.headersSubmitted = true;
            tableState.headerRowCount = std::max(tableState.headerRowCount, headerRow + 1U);
        }
        const PointerState * pointer = ui->input.primaryPointer();

        for(uint32_t column = 0; column != columnCount; ++column)
        {
            String generatedLabel;
            StringView label;
            Rect previousBounds;
            bool visible = true;
            {
                Context::TableState & tableState = ui->tableState(tableId);

                if(column >= tableState.columns.size())
                {
                    tableState.columns.resize(column + 1);
                    tableState.columns[column].order = column;
                }

                Context::TableColumnState & columnState = tableState.columns[column];
                label = columnState.label;
                previousBounds = columnState.lastBounds;
                visible = columnState.options.enabled && columnState.options.visible == true && (angledOnly ? columnState.options.angledHeader : initialState.angledHeadersSubmitted == false || columnState.options.angledHeader == false);

                if(visible == false)
                {
                    columnState.lastBounds = {};
                }
            }

            if(visible == false)
            {
                continue;
            }

            if(label.empty() == true)
            {
                generatedLabel = "Column ";
                char digits[16] = {};
                auto converted = std::to_chars(digits, digits + sizeof(digits), column + 1);
                generatedLabel.append(digits, converted.ptr);
                label = generatedLabel;
            }

            size_t node = ui->addNode(Detail::NodeKind::Selectable, Key(column), label, {}, SourceLocation::current(), SemanticRole::Cell, true);
            Context::Node & header = ui->nodes[node];
            header.tableRow = headerRow;
            header.tableColumn = column;
            header.tableHeader = true;
            header.tableColumnOptions = ui->tableState(tableId).columns[column].options;
            {
                const Context::TableState & tableState = ui->tableState(tableId);
                auto sort = std::find_if(tableState.sortSpecs.begin(), tableState.sortSpecs.end(),
                                               [column](const TableSortSpec & value)
                                               {
                                                   return value.column == column;
                                               });

                if(sort != tableState.sortSpecs.end())
                {
                    header.selected = true;
                    header.tableSortDirection = sort->direction;
                    header.tableSortOrder = sort->order;
                }
            }

            const Context::TableState & beforeInteraction = ui->tableState(tableId);
            Rect resizeBounds = previousBounds.empty() == true ? Rect{} : Rect{previousBounds.right() - 4.f, previousBounds.y, 8.f, previousBounds.height};
            bool resizeHit = pointer != nullptr && resizeBounds.contains(pointer->position) && header.disabled == false && header.inputBlocked == false;
            Rect interactionBounds = previousBounds;

            if(beforeInteraction.options.resizable == true && beforeInteraction.columns[column].options.resizable == true && column + 1 < columnCount && previousBounds.empty() == false)
            {
                interactionBounds.width = std::max(0.f, interactionBounds.width - 5.f);
            }

            Response response = ui->interact(node, true, interactionBounds.empty() == true ? nullptr : &interactionBounds);
            ui->nodes[node].response = response;
            Context::TableState & tableState = ui->tableState(tableId);

            if(tableState.options.reorderable == true && tableState.columns[column].options.reorderable == true && response.pressed() == true && resizeHit == false)
            {
                tableState.draggingColumn = column;
                tableState.columnDragStartX = pointer == nullptr ? 0.f : pointer->position.x;
                tableState.columnDragMoved = false;
            }

            if(tableState.draggingColumn == column && pointer != nullptr && pointer->isDown() == true && std::abs(pointer->position.x - tableState.columnDragStartX) >= 4.f)
            {
                uint32_t target = column;
                for(uint32_t candidate = 0; candidate != columnCount; ++candidate)
                {
                    const Rect & bounds = tableState.columns[candidate].lastBounds;

                    if(bounds.empty() == false && pointer->position.x >= bounds.x && pointer->position.x < bounds.right())
                    {
                        target = candidate;
                        break;
                    }
                }

                if(target != column && tableState.columns[target].options.reorderable == true)
                {
                    std::swap(tableState.columns[column].order, tableState.columns[target].order);
                    tableState.displayOrderDirty = true;
                    tableState.columnDragStartX = pointer->position.x;
                    tableState.columnDragMoved = true;
                    const Context::Node & currentTableNode = ui->nodes[tableIndex];
                    ui->frame.events.push_back({EventType::Change, tableId, ui->nodePath(ui->nodes[tableIndex]), currentTableNode.file, currentTableNode.line, ui->input.timestamp});
                }
            }

            bool contextMenuAvailable = (tableState.options.hideable && tableState.columns[column].options.hideable) || (tableState.options.resizable && tableState.columns[column].options.resizable) || (tableState.options.sortable && tableState.columns[column].options.sortable) || tableState.options.contextMenu != nullptr;

            if(contextMenuAvailable == true && pointer != nullptr && pointer->isPressed(PointerButton::Secondary) == true && previousBounds.contains(pointer->position) == true)
            {
                tableState.contextColumn = column;
                PopupOptions popupOptions;
                popupOptions.owner = tableId;
                popupOptions.anchor = {pointer->position.x, pointer->position.y, 1.f, 1.f};
                popupOptions.placement = PopupPlacement::Cursor;
                popupOptions.minimumSize = {ui->currentStyle->metrics.minimumPopupWidth, 0.f};
                Mosaic::openPopup(ui, Key("Table columns"), popupOptions);
            }

            bool finishedColumnDrag = response.released() && tableState.draggingColumn == column;
            bool activateSort = tableState.options.sortable;

            if(tableState.columns[column].options.enabled == false)
            {
                activateSort = false;
            }

            if(tableState.columns[column].options.sortable == false)
            {
                activateSort = false;
            }

            if(response.clicked() == false)
            {
                activateSort = false;
            }

            if(tableState.columnDragMoved == true)
            {
                activateSort = false;
            }

            if(activateSort == true)
            {
                const TableColumnOptions & columnOptions = tableState.columns[column].options;
                SortDirection firstDirection = columnOptions.preferredSort == SortDirection::Descending && columnOptions.sortDescending ? SortDirection::Descending : columnOptions.sortAscending ? SortDirection::Ascending : SortDirection::Descending;
                SortDirection direction = firstDirection;
                auto existing = std::find_if(tableState.sortSpecs.begin(), tableState.sortSpecs.end(),
                                                   [column](const TableSortSpec & spec)
                                                   {
                                                       return spec.column == column;
                                                   });

                if(existing != tableState.sortSpecs.end())
                {
                    if(existing->direction == SortDirection::Ascending)
                    {
                        direction = columnOptions.sortDescending ? SortDirection::Descending : tableState.options.sortTristate ? SortDirection::None : SortDirection::Ascending;
                    }
                    else
                    {
                        direction = columnOptions.sortAscending ? SortDirection::Ascending : tableState.options.sortTristate ? SortDirection::None : SortDirection::Descending;
                    }
                }

                bool replaceSort = tableState.options.multiSort == false || ui->input.modifiers.shift == false;

                if(direction == SortDirection::None)
                {
                    if(replaceSort == true)
                    {
                        tableState.sortSpecs.clear();
                    }
                    else
                    {
                        auto spec = std::find_if(tableState.sortSpecs.begin(), tableState.sortSpecs.end(),
                                                       [column](const TableSortSpec & value)
                                                       {
                                                           return value.column == column;
                                                       });

                        if(spec != tableState.sortSpecs.end())
                        {
                            tableState.sortSpecs.erase(spec);
                        }
                    }
                }
                else
                {
                    if(replaceSort == true)
                    {
                        tableState.sortSpecs.clear();
                    }

                    auto spec = std::find_if(tableState.sortSpecs.begin(), tableState.sortSpecs.end(),
                                             [column](const TableSortSpec & value)
                                             {
                                                 return value.column == column;
                                             });

                    if(spec == tableState.sortSpecs.end())
                    {
                        tableState.sortSpecs.push_back({column, direction, static_cast<uint32_t>(tableState.sortSpecs.size())});
                    }
                    else
                    {
                        spec->direction = direction;
                    }
                }
                tableState.sortSpecsDirty = true;
                const Context::Node & currentTableNode = ui->nodes[tableIndex];
                ui->frame.events.push_back({EventType::Change, tableId, ui->nodePath(ui->nodes[tableIndex]), currentTableNode.file, currentTableNode.line, ui->input.timestamp});
            }

            bool beginResize = tableState.options.resizable;

            if(tableState.columns[column].options.resizable == false)
            {
                beginResize = false;
            }

            if(column + 1 >= columnCount)
            {
                beginResize = false;
            }

            if(pointer == nullptr)
            {
                beginResize = false;
            }

            if(resizeHit == false)
            {
                beginResize = false;
            }

            if(beginResize == true)
            {
                beginResize = pointer->isPressed();
            }

            if(beginResize == true)
            {
                ui->captured = ui->nodes[node].id;
                ui->capturedPointer = pointer->id;
                tableState.columns[column].resizing = true;
                tableState.columns[column].resizeStartX = pointer->position.x;
                tableState.columns[column].resizeStartWidth = previousBounds.width;
                tableState.draggingColumn = std::numeric_limits<uint32_t>::max();
            }

            Context::TableColumnState & columnState = tableState.columns[column];

            if(columnState.resizing == true && pointer != nullptr && ui->captured == ui->nodes[node].id && pointer->isDown() == true)
            {
                columnState.options.sizing = TableSizing::Fixed;
                columnState.options.widthOrWeight = std::max(ui->currentStyle->metrics.grabMinimumSize, columnState.resizeStartWidth + pointer->position.x - columnState.resizeStartX);
            }

            if(columnState.resizing == true && pointer != nullptr && ui->captured == ui->nodes[node].id && pointer->isReleased() == true)
            {
                columnState.resizing = false;
                ui->captured = InvalidId;
                ui->capturedPointer = 0;
            }

            if(finishedColumnDrag == true)
            {
                tableState.draggingColumn = std::numeric_limits<uint32_t>::max();
                tableState.columnDragMoved = false;
            }
        }

        PopupOptions popupOptions;
        popupOptions.owner = tableId;
        popupOptions.minimumSize = {ui->currentStyle->metrics.minimumPopupWidth, 0.f};
        auto columnsPopup = Mosaic::popup(ui, Key("Table columns"), popupOptions);

        if(columnsPopup.visible() == true)
        {
            uint32_t contextColumn = ui->tableState(tableId).contextColumn;

            if(contextColumn < columnCount)
            {
                const Context::TableState & beforeActions = ui->tableState(tableId);
                String heading = beforeActions.columns[contextColumn].label;

                if(heading.empty() == true)
                {
                    heading = "Column";
                }

                Mosaic::text(ui, heading);
                Mosaic::separator(ui);
                MenuItemOptions fitColumn;
                fitColumn.enabled = beforeActions.options.resizable && beforeActions.columns[contextColumn].options.resizable;

                if(Mosaic::menuItem(ui, "Size column to fit", fitColumn).clicked() == true)
                {
                    Detail::sizeTableColumnToFit(ui->tableState(tableId), contextColumn);
                }

                MenuItemOptions sortAscending;
                sortAscending.enabled = beforeActions.options.sortable && beforeActions.columns[contextColumn].options.sortable && beforeActions.columns[contextColumn].options.sortAscending;

                if(Mosaic::menuItem(ui, "Sort ascending", sortAscending).clicked() == true)
                {
                    Detail::setTableSort(ui->tableState(tableId), contextColumn, SortDirection::Ascending);
                }

                MenuItemOptions sortDescending;
                sortDescending.enabled = beforeActions.options.sortable && beforeActions.columns[contextColumn].options.sortable && beforeActions.columns[contextColumn].options.sortDescending;

                if(Mosaic::menuItem(ui, "Sort descending", sortDescending).clicked() == true)
                {
                    Detail::setTableSort(ui->tableState(tableId), contextColumn, SortDirection::Descending);
                }

                if(beforeActions.options.contextMenu != nullptr)
                {
                    Mosaic::separator(ui);
                    beforeActions.options.contextMenu(ui, tableId, contextColumn, beforeActions.options.contextMenuUserData);
                }

                Mosaic::separator(ui);
            }

            if(Mosaic::menuItem(ui, "Size all columns to fit").clicked() == true)
            {
                Context::TableState & state = ui->tableState(tableId);
                for(uint32_t column = 0; column != columnCount; ++column)
                {
                    if(state.columns[column].options.resizable == true)
                    {
                        Detail::sizeTableColumnToFit(state, column);
                    }
                }
            }

            if(Mosaic::menuItem(ui, "Reset column order").clicked() == true)
            {
                Detail::resetTableColumnOrder(ui->tableState(tableId));
            }

            Mosaic::separatorText(ui, "Visible columns");
            const Context::TableState & visibleState = ui->tableState(tableId);
            size_t visibleCount = static_cast<size_t>(std::count_if(visibleState.columns.begin(), visibleState.columns.begin() + columnCount,
                                                                          [](const Context::TableColumnState & value)
                                                                          {
                                                                              return value.options.enabled && value.options.visible;
                                                                          }));
            for(uint32_t column = 0; column != columnCount; ++column)
            {
                const Context::TableState & beforeItem = ui->tableState(tableId);
                String generatedColumnLabel;
                StringView columnLabel = beforeItem.columns[column].label;

                if(beforeItem.columns[column].options.enabled == false)
                {
                    continue;
                }

                if(beforeItem.columns[column].options.hideable == false)
                {
                    continue;
                }

                bool visible = beforeItem.columns[column].options.visible;

                if(columnLabel.empty() == true)
                {
                    generatedColumnLabel = "Column ";
                    char digits[16] = {};
                    auto converted = std::to_chars(digits, digits + sizeof(digits), column + 1);
                    generatedColumnLabel.append(digits, converted.ptr);
                    columnLabel = generatedColumnLabel;
                }

                MenuItemOptions itemOptions;
                itemOptions.checked = &visible;
                itemOptions.enabled = visible == false || visibleCount > 1;
                itemOptions.closeOnActivate = false;
                Response item = Mosaic::menuItem(ui, Key(column), columnLabel, itemOptions);

                if(item.changed() == true)
                {
                    ui->tableState(tableId).columns[column].options.visible = visible;
                }
            }
        }
        Context::TableState & finalState = ui->tableState(tableId);
        finalState.currentRow = headerRow + 1U;
        finalState.currentColumn = 0;
        finalState.currentCellSubmitted = false;
    }

    //////////////////////////////////////////////////////////////////////////
    void tableAngledHeadersRow(Context * ui)
    {
        if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableAngledHeadersRow must be called inside table()");

            return;
        }

        Context::TableState & tableState = ui->tableState(ui->nodes[ui->currentParent].id);
        tableState.submitAngledHeaders = true;
        Mosaic::tableHeadersRow(ui);
        tableState.submitAngledHeaders = false;
        tableState.angledHeadersSubmitted = true;
    }
    //////////////////////////////////////////////////////////////////////////
    Response tableHeader(Context * ui, uint32_t column, StringView label, const SourceLocation & location)
    {
        size_t tableIndex = ui->currentParent;
        while(tableIndex < ui->nodes.size() && ui->nodes[tableIndex].kind != Detail::NodeKind::Table)
        {
            size_t parent = ui->nodes[tableIndex].parent;

            if(parent == tableIndex)
            {
                break;
            }

            tableIndex = parent;
        }

        if(tableIndex >= ui->nodes.size())
        {
            ui->frame.diagnostics.emplace_back("tableHeader must be called inside table() with a valid column");

            return {};
        }

        if(ui->nodes[tableIndex].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableHeader must be called inside table() with a valid column");

            return {};
        }

        if(column >= ui->nodes[tableIndex].layout.columns)
        {
            ui->frame.diagnostics.emplace_back("tableHeader must be called inside table() with a valid column");

            return {};
        }

        Id tableId = ui->nodes[tableIndex].id;
        Context::TableState & tableState = ui->tableState(tableId);

        if(tableState.columns.size() <= column)
        {
            tableState.columns.resize(column + 1);
            tableState.columns[column].order = column;
        }

        Context::TableColumnState & columnState = tableState.columns[column];
        tableState.headersSubmitted = true;

        if(columnState.options.enabled == false)
        {
            return {};
        }

        if(columnState.options.visible == false)
        {
            return {};
        }

        String generatedLabel;

        if(label.empty() == true)
        {
            label = columnState.label;
        }

        if(label.empty() == true)
        {
            generatedLabel = "Column ";
            char digits[16] = {};
            auto converted = std::to_chars(digits, digits + sizeof(digits), column + 1);
            generatedLabel.append(digits, converted.ptr);
            label = generatedLabel;
        }

        LayoutOptions layout;
        layout.width = SizeRule::Fill;
        size_t node = ui->addNode(Detail::NodeKind::Selectable, Key(column), label, layout, location, SemanticRole::Cell, true);
        Context::Node & header = ui->nodes[node];
        header.tableRow = 0;
        header.tableColumn = column;
        header.tableHeader = true;
        header.tableColumnOptions = columnState.options;
        auto sorted = std::find_if(tableState.sortSpecs.begin(), tableState.sortSpecs.end(),
                                         [column](const TableSortSpec & value)
                                         {
                                             return value.column == column;
                                         });

        if(sorted != tableState.sortSpecs.end())
        {
            header.selected = true;
            header.tableSortDirection = sorted->direction;
            header.tableSortOrder = sorted->order;
        }

        Response response = ui->interact(node, true);
        header.response = response;

        if(response.clicked() == true && tableState.options.sortable == true && columnState.options.sortable == true)
        {
            const TableColumnOptions & columnOptions = columnState.options;
            SortDirection firstDirection = columnOptions.preferredSort == SortDirection::Descending && columnOptions.sortDescending ? SortDirection::Descending : columnOptions.sortAscending ? SortDirection::Ascending : SortDirection::Descending;
            SortDirection direction = firstDirection;
            auto existing = std::find_if(tableState.sortSpecs.begin(), tableState.sortSpecs.end(),
                                         [column](const TableSortSpec & value)
                                         {
                                             return value.column == column;
                                         });

            if(existing != tableState.sortSpecs.end())
            {
                if(existing->direction == SortDirection::Ascending)
                {
                    direction = columnOptions.sortDescending ? SortDirection::Descending : tableState.options.sortTristate ? SortDirection::None : SortDirection::Ascending;
                }
                else
                {
                    direction = columnOptions.sortAscending ? SortDirection::Ascending : tableState.options.sortTristate ? SortDirection::None : SortDirection::Descending;
                }
            }

            bool replaceSort = tableState.options.multiSort == false || ui->input.modifiers.shift == false;

            if(direction == SortDirection::None)
            {
                if(replaceSort == true)
                {
                    tableState.sortSpecs.clear();
                }
                else
                {
                    existing = std::find_if(tableState.sortSpecs.begin(), tableState.sortSpecs.end(),
                                            [column](const TableSortSpec & value)
                                            {
                                                return value.column == column;
                                            });

                    if(existing != tableState.sortSpecs.end())
                    {
                        tableState.sortSpecs.erase(existing);
                    }
                }
            }
            else
            {
                if(replaceSort == true)
                {
                    tableState.sortSpecs.clear();
                }

                existing = std::find_if(tableState.sortSpecs.begin(), tableState.sortSpecs.end(),
                                        [column](const TableSortSpec & value)
                                        {
                                            return value.column == column;
                                        });

                if(existing == tableState.sortSpecs.end())
                {
                    tableState.sortSpecs.push_back({column, direction, static_cast<uint32_t>(tableState.sortSpecs.size())});
                }
                else
                {
                    existing->direction = direction;
                }
            }
            tableState.sortSpecsDirty = true;
            Detail::setFlag(response, 6);
            header.response = response;
            const Context::Node & tableNode = ui->nodes[tableIndex];
            ui->frame.events.push_back({EventType::Change, tableId, ui->nodePath(tableIndex), tableNode.file, tableNode.line, ui->input.timestamp});
        }

        return response;
    }

    //////////////////////////////////////////////////////////////////////////
    bool tableColumnName(const Context * ui, Id table, uint32_t column, StringView * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::TableState * tableState = ui->findTableState(table);

        if(tableState == nullptr)
        {
            return false;
        }

        if(column >= tableState->columns.size())
        {
            return false;
        }

        *_out = tableState->columns[column].label;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void tableNextRow(Context * ui)
    {
        Mosaic::tableNextRow(ui, TableRowOptions{});
    }
    //////////////////////////////////////////////////////////////////////////
    void tableNextRow(Context * ui, const TableRowOptions & options)
    {
        if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableNextRow must be called inside table()");

            return;
        }

        Context::TableState & tableState = ui->tableState(ui->nodes[ui->currentParent].id);

        if(tableState.currentCellSubmitted == true)
        {
            ++tableState.currentRow;
        }

        tableState.currentColumn = 0;
        tableState.currentCellSubmitted = false;

        if(tableState.requestedRowHeights.size() <= tableState.currentRow)
        {
            tableState.requestedRowHeights.resize(static_cast<size_t>(tableState.currentRow) + 1, 0.f);
        }

        tableState.requestedRowHeights[tableState.currentRow] = std::max(0.f, options.minimumHeight);

        if(tableState.requestedRowPaddingY.size() <= tableState.currentRow)
        {
            tableState.requestedRowPaddingY.resize(static_cast<size_t>(tableState.currentRow) + 1, -1.f);
        }

        tableState.requestedRowPaddingY[tableState.currentRow] = options.cellPaddingY;
        tableState.currentRowBackgroundsEnabled = {false, false};
        tableState.pendingCellBackgroundEnabled = false;
    }
    //////////////////////////////////////////////////////////////////////////
    void tableSetRowBackground(Context * ui, const Color & color)
    {
        Mosaic::tableSetRowBackground(ui, TableBackgroundTarget::Row0, color);
    }
    //////////////////////////////////////////////////////////////////////////
    void tableSetRowBackground(Context * ui, TableBackgroundTarget target, const Color & color)
    {
        if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableSetRowBackground must be called inside table()");

            return;
        }

        Context::TableState & tableState = ui->tableState(ui->nodes[ui->currentParent].id);
        size_t index = static_cast<size_t>(target);
        tableState.currentRowBackgrounds[index] = color;
        tableState.currentRowBackgroundsEnabled[index] = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void tableSetCellBackground(Context * ui, const Color & color)
    {
        if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableSetCellBackground must be called inside table()");

            return;
        }

        Context::TableState & tableState = ui->tableState(ui->nodes[ui->currentParent].id);
        tableState.pendingCellBackground = color;
        tableState.pendingCellBackgroundEnabled = true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool tableVisibleRows(Context * ui, size_t rowCount, float rowHeight, VisibleRange * const _out, size_t overscan)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableVisibleRows must be called inside table()");

            return false;
        }

        if(rowCount == 0)
        {

            *_out = {};

            return true;
        }

        if(rowHeight <= 0.f)
        {
            return false;
        }

        size_t tableIndex = ui->currentParent;
        const Context::Node & tableNode = ui->nodes[tableIndex];
        Context::TableState & tableState = ui->tableState(tableNode.id);
        uint32_t firstRow = tableState.headerRowCount;
        size_t maximumRows = static_cast<size_t>(std::numeric_limits<uint32_t>::max() - firstRow);
        size_t boundedRowCount = std::min(rowCount, maximumRows);

        if(boundedRowCount != rowCount)
        {
            ui->frame.diagnostics.emplace_back("tableVisibleRows row count exceeds the supported range");
        }

        const Context::Persistent & persistentState = ui->state(ui->nodes[tableIndex]);
        float spacing = ui->gap(tableNode);
        float stride = rowHeight + spacing;
        float headerExtent = tableState.headerRowCount == 0 ? 0.f : static_cast<float>(tableState.headerRowCount) * ui->currentStyle->metrics.controlHeight + static_cast<float>(tableState.headerRowCount) * spacing;
        float scroll = std::max(0.f, persistentState.scrollPosition.y - headerExtent);
        float viewportHeight = persistentState.lastBounds.height > 0.f ? persistentState.lastBounds.height : ui->viewport.bounds.height;
        size_t begin = std::min(boundedRowCount, static_cast<size_t>(std::floor(scroll / std::max(stride, 0.001f))));
        size_t end = std::min(boundedRowCount, begin + static_cast<size_t>(std::ceil(std::max(0.f, viewportHeight) / std::max(stride, 0.001f))) + 1);
        begin = begin > overscan ? begin - overscan : 0;
        end = std::min(boundedRowCount, end + overscan);

        tableState.virtualRowCount = boundedRowCount;
        tableState.virtualRowHeight = rowHeight;
        tableState.virtualFirstRow = firstRow;
        tableState.rowsVirtualized = true;
        tableState.currentRow = firstRow + static_cast<uint32_t>(begin);
        tableState.currentColumn = 0;
        tableState.currentCellSubmitted = false;
        VisibleRange range = {begin, end};

        *_out = range;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    Response tableNextRow(Context * ui, const Key & key, bool selected, const SourceLocation & location)
    {
        if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableNextRow selection must be called inside table()");

            return {};
        }

        size_t tableIndex = ui->currentParent;
        Mosaic::tableNextRow(ui);
        Context::TableState & tableState = ui->tableState(ui->nodes[tableIndex].id);
        uint32_t row = tableState.currentRow;
        LayoutOptions layout;
        layout.width = SizeRule::Fill;
        layout.height = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
        size_t node = ui->addNode(Detail::NodeKind::TableRow, key, {}, layout, location, SemanticRole::Row, tableState.options.rowSelection);
        Context::Node & rowNode = ui->nodes[node];
        rowNode.tableRow = row;
        rowNode.tableColumn = 0;
        rowNode.selected = selected;
        rowNode.inputBlocked = rowNode.inputBlocked || tableState.options.rowSelection == false;
        tableState.currentRow = row;
        tableState.currentColumn = 0;
        tableState.currentCellSubmitted = false;
        Response response = ui->interact(node);
        rowNode.response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response tableNextRow(Context * ui, const Key & key, SelectionModel * selection, Id item, IdSpan orderedItems, const SourceLocation & location)
    {
        bool selected = selection != nullptr && selection->selected(item);
        Response response = Mosaic::tableNextRow(ui, key, selected, location);

        if(response.clicked() == true && selection != nullptr)
        {
            if(ui->input.modifiers.shift == true && orderedItems.empty() == false)
            {
                selection->selectRange(orderedItems, item);
            }
            else
            {
                bool toggle = ui->input.modifiers.primary || ui->input.modifiers.control == true || ui->input.modifiers.super;
                selection->select(item, toggle, toggle);
            }

            Detail::setFlag(response, 6);
            for(Context::Node & node : ui->nodes)
            {
                if(node.id == response.id)
                {
                    node.selected = selection->selected(item);
                    node.response = response;
                    break;
                }
            }
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    bool tableSetColumn(Context * ui, uint32_t column)
    {
        const Context::Node & tableNode = ui->nodes[ui->currentParent];

        if(tableNode.kind != Detail::NodeKind::Table)
        {
            return false;
        }

        if(column >= tableNode.layout.columns)
        {
            return false;
        }

        Context::TableState & tableState = ui->tableState(tableNode.id);
        tableState.currentColumn = column;
        tableState.currentCellSubmitted = true;
        auto returnedValue = Detail::tableColumnRequestsOutput(ui, tableNode, tableState, column);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool tableNextColumn(Context * ui)
    {
        const Context::Node & tableNode = ui->nodes[ui->currentParent];

        if(tableNode.kind != Detail::NodeKind::Table)
        {
            ui->frame.diagnostics.emplace_back("tableNextColumn must be called inside table()");

            return false;
        }

        if(tableNode.layout.columns == 0)
        {
            ui->frame.diagnostics.emplace_back("tableNextColumn must be called inside table()");

            return false;
        }

        Context::TableState & tableState = ui->tableState(tableNode.id);

        if(tableState.currentCellSubmitted == true)
        {
            ++tableState.currentColumn;

            if(tableState.currentColumn >= tableNode.layout.columns)
            {
                tableState.currentColumn = 0;
                ++tableState.currentRow;
                tableState.currentRowBackgroundsEnabled = {false, false};
            }
        }

        tableState.currentCellSubmitted = true;
        tableState.pendingCellBackgroundEnabled = false;
        auto returnedValue = Detail::tableColumnRequestsOutput(ui, tableNode, tableState, tableState.currentColumn);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool tableSortSpecs(const Context * ui, Id table, TableSortSpecSpan * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::TableState * tableState = ui->findTableState(table);

        if(tableState == nullptr)
        {
            return false;
        }

        *_out = TableSortSpecSpan(tableState->sortSpecs);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool tableSortState(const Context * ui, Id table, TableSortState * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::TableState * tableState = ui->findTableState(table);

        if(tableState == nullptr)
        {
            return false;
        }

        *_out = {TableSortSpecSpan(tableState->sortSpecs), tableState->sortSpecsDirty};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void tableSortSpecsHandled(Context * ui, Id table) noexcept
    {
        ui->tableState(table).sortSpecsDirty = false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool tableColumnStatus(const Context * ui, Id table, uint32_t column, TableColumnStatus * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::TableState * tableState = ui->findTableState(table);

        if(tableState == nullptr)
        {
            return false;
        }

        if(column >= tableState->columns.size())
        {
            return false;
        }

        TableColumnStatus result;
        const Context::TableColumnState & columnState = tableState->columns[column];
        result.enabled = columnState.options.enabled;
        result.visible = result.enabled && columnState.options.visible == true && columnState.lastBounds.empty() == false;
        result.displayOrder = columnState.order;
        result.width = columnState.resolvedWidth;
        auto sort = std::find_if(tableState->sortSpecs.begin(), tableState->sortSpecs.end(),
                                       [column](const TableSortSpec & spec)
                                       {
                                           return spec.column == column;
                                       });

        if(sort != tableState->sortSpecs.end())
        {
            result.sorted = true;
            result.sortDirection = sort->direction;
        }

        const PointerState * pointer = ui->input.primaryPointer();
        result.hovered = result.visible && pointer != nullptr && columnState.bodyBounds.contains(pointer->position);

        *_out = result;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void resetTableSettings(Context * ui, Id table) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(table == InvalidId)
        {
            return;
        }

        if(const Context::Node * node = ui->findFrameNode(table); node != nullptr && node->kind == Detail::NodeKind::Table && node->tableSettingsId != InvalidId)
        {
            table = node->tableSettingsId;
        }

        auto iterator = ui->persistent.find(table);

        if(iterator == ui->persistent.end())
        {
            return;
        }

        if(iterator->second.table == nullptr)
        {
            return;
        }

        iterator->second.table.reset();
    }
    //////////////////////////////////////////////////////////////////////////
    Scope propertyGrid(Context * ui, StringView label, const PropertyGridOptions & options, const LayoutOptions & layout, const SourceLocation & location)
    {
        LayoutOptions resolvedLayout = layout;

        if(resolvedLayout.width.rule == SizeRule::Content)
        {
            resolvedLayout.width = SizeRule::Fill;
        }

        TableOptions tableOptions;
        tableOptions.headers = false;
        tableOptions.resizable = false;
        tableOptions.reorderable = false;
        tableOptions.hideable = false;
        tableOptions.sortable = false;
        Scope result = Mosaic::table(ui, label, 2, tableOptions, resolvedLayout, location);

        TableColumnOptions valueColumn;
        valueColumn.sizing = TableSizing::Stretch;
        valueColumn.widthOrWeight = std::max(0.001f, options.valueColumnWeight);
        valueColumn.sortable = false;
        Mosaic::tableSetupColumn(ui, 0, options.valueHeader, valueColumn);

        TableColumnOptions labelColumn;
        labelColumn.sizing = TableSizing::Stretch;
        labelColumn.widthOrWeight = std::max(0.001f, options.labelColumnWeight);
        labelColumn.sortable = false;
        Mosaic::tableSetupColumn(ui, 1, options.labelHeader, labelColumn);

        if(options.showHeaders == true)
        {
            Mosaic::tableNextRow(ui);
            (void)Mosaic::tableSetColumn(ui, 0);
            Mosaic::text(ui, options.valueHeader, location);
            (void)Mosaic::tableSetColumn(ui, 1);
            Mosaic::text(ui, options.labelHeader, location);
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    void propertyGridNextRow(Context * ui)
    {
        Mosaic::tableNextRow(ui);
        (void)Mosaic::tableSetColumn(ui, 0);
    }
    //////////////////////////////////////////////////////////////////////////
    bool propertyGridValue(Context * ui)
    {
        auto returnedValue = Mosaic::tableSetColumn(ui, 0);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool propertyGridLabel(Context * ui)
    {
        auto returnedValue = Mosaic::tableSetColumn(ui, 1);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
