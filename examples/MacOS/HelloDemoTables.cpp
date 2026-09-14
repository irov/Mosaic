#include "HelloDemoTables.hpp"
#include "Charconv.hpp"

#include <algorithm>
#include <limits>

namespace MosaicExample
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        template<class T> [[nodiscard]] Mosaic::String tableDemoNumber(T value)
        {
            char buffer[64] = {};
            auto result = Mosaic::Detail::toChars(buffer, buffer + sizeof(buffer), value);
            auto returnedValue = result.ec == std::errc{} ? Mosaic::String(buffer, result.ptr) : Mosaic::String("?");

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::String tableDemoFixed(double value, int precision)
        {
            char buffer[64] = {};
            auto result = Mosaic::Detail::toChars(buffer, buffer + sizeof(buffer), value, Mosaic::Detail::NumberFormat::Fixed, precision);
            auto returnedValue = result.ec == std::errc{} ? Mosaic::String(buffer, result.ptr) : Mosaic::String("?");

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::String tableDemoCell(uint32_t column, size_t row, Mosaic::StringView prefix = "Hello ")
        {
            Mosaic::String value(prefix);
            value += Detail::tableDemoNumber(column);
            value += ",";
            value += Detail::tableDemoNumber(row);

            return value;
        }
        //////////////////////////////////////////////////////////////////////////
        void tableDemoSetupColumns(Mosaic::Context * ui, uint32_t count, Mosaic::TableSizing sizing, float widthOrWeight = 1.f)
        {
            for(uint32_t column = 0; column != count; ++column)
            {
                Mosaic::TableColumnOptions columnOptions;
                columnOptions.sizing = sizing;
                columnOptions.widthOrWeight = widthOrWeight;
                Mosaic::String name = column == 0 ? "One" : column == 1 ? "Two" : column == 2 ? "Three" : "Column ";

                if(column > 2)
                {
                    name += Detail::tableDemoNumber(column);
                }

                Mosaic::tableSetupColumn(ui, column, name, columnOptions);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void tableDemoRows(Mosaic::Context * ui, size_t firstRow, size_t lastRow, uint32_t columnCount)
        {
            for(size_t row = firstRow; row != lastRow; ++row)
            {
                Mosaic::tableNextRow(ui, Mosaic::Key(row));
                for(uint32_t column = 0; column != columnCount; ++column)
                {
                    if(Mosaic::tableSetColumn(ui, column) == false)
                    {
                        continue;
                    }

                    auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));
                    Mosaic::text(ui, Detail::tableDemoCell(column, row));
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void drawResizableTableDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions, bool fixed)
        {
            static bool stretchResizable = true;
            static bool stretchBordersVertical = true;
            static bool fixedNoHostExtendHorizontal = false;
            Mosaic::TableOptions options = baseOptions;
            options.resizable = true;
            options.contextMenuInBody = true;
            options.bordersOuterHorizontal = true;
            options.bordersOuterVertical = true;
            options.bordersInnerVertical = true;

            if(fixed == true)
            {
                Mosaic::helpMarker(ui, "Fixed-width columns retain their widths. Double-click a separator "
                                       "to fit the column to its contents.");
                Mosaic::checkbox(ui, "MosaicTableFlags_NoHostExtendX", &fixedNoHostExtendHorizontal);
                options.extendHostHorizontal = fixedNoHostExtendHorizontal == false;
            }
            else
            {
                Mosaic::checkbox(ui, "MosaicTableFlags_Resizable", &stretchResizable);
                Mosaic::checkbox(ui, "MosaicTableFlags_BordersV", &stretchBordersVertical);
                options.resizable = stretchResizable;
                options.bordersInnerVertical = stretchBordersVertical || stretchResizable;
                options.bordersOuterVertical = stretchBordersVertical;
            }

            Mosaic::LayoutOptions layout;
            layout.width = fixed && fixedNoHostExtendHorizontal ? Mosaic::Dimension(Mosaic::SizeRule::Content) : Mosaic::Dimension(Mosaic::SizeRule::Fill);
            auto table = Mosaic::table(ui, fixed ? "Resizable fixed table" : "Resizable stretch table", 3, options, layout);
            Detail::tableDemoSetupColumns(ui, 3, fixed ? Mosaic::TableSizing::FixedFit : Mosaic::TableSizing::StretchSame, fixed ? 0.f : 1.f);
            Detail::tableDemoRows(ui, 0, 5, 3);
        }
        //////////////////////////////////////////////////////////////////////////
        void drawVerticalScrollingTableDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            static bool scrollVertical = true;
            Mosaic::helpMarker(ui, "Vertical scrolling uses a fixed-height table and clips submission to "
                                   "the rows visible in the current scroll range.");
            Mosaic::checkbox(ui, "MosaicTableFlags_ScrollY", &scrollVertical);
            Mosaic::TableOptions options = baseOptions;
            options.scrollVertical = scrollVertical;
            options.rowBackground = true;
            options.resizable = true;
            options.reorderable = true;
            options.hideable = true;
            options.bordersOuterHorizontal = true;
            options.bordersOuterVertical = true;
            options.bordersInnerVertical = true;
            options.frozenRows = 1;
            Mosaic::LayoutOptions layout;
            layout.width = Mosaic::SizeRule::Fill;
            layout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 8.f);
            auto table = Mosaic::table(ui, "Vertical scrolling table", 3, options, layout);
            Detail::tableDemoSetupColumns(ui, 3, Mosaic::TableSizing::StretchSame);
            Mosaic::tableHeadersRow(ui);
            constexpr size_t rowCount = 1000;
            Mosaic::VisibleRange rows = {0, rowCount};

            if(scrollVertical == true)
            {
                (void)Mosaic::tableVisibleRows(ui, rowCount, Mosaic::getTheme(ui).metrics.controlHeight, &rows);
            }

            Detail::tableDemoRows(ui, rows.begin, rows.end, 3);
        }
        //////////////////////////////////////////////////////////////////////////
        void drawHorizontalScrollingTableDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            static bool resizable = true;
            static bool scrollHorizontal = true;
            static bool scrollVertical = true;
            static int32_t frozenColumns = 1;
            static int32_t frozenRows = 1;
            Mosaic::helpMarker(ui, "Horizontal scrolling keeps fixed-width columns and can freeze leading "
                                   "columns and header rows independently.");
            Mosaic::checkbox(ui, "MosaicTableFlags_Resizable", &resizable);
            Mosaic::checkbox(ui, "MosaicTableFlags_ScrollX", &scrollHorizontal);
            Mosaic::checkbox(ui, "MosaicTableFlags_ScrollY", &scrollVertical);
            Mosaic::dragValue(ui, "freeze_cols", &frozenColumns, int32_t{0}, int32_t{7});
            Mosaic::dragValue(ui, "freeze_rows", &frozenRows, int32_t{0}, int32_t{9});

            Mosaic::TableOptions options = baseOptions;
            options.resizable = resizable;
            options.scrollHorizontal = scrollHorizontal;
            options.scrollVertical = scrollVertical;
            options.rowBackground = true;
            options.bordersOuterHorizontal = true;
            options.bordersOuterVertical = true;
            options.bordersInnerVertical = true;
            options.frozenColumns = static_cast<uint32_t>(std::max(frozenColumns, int32_t{0}));
            options.frozenRows = static_cast<uint32_t>(std::max(frozenRows, int32_t{0}));
            options.innerWidth = scrollHorizontal ? 900.f : 0.f;
            Mosaic::LayoutOptions layout;
            layout.width = Mosaic::SizeRule::Fill;
            layout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 8.f);
            auto table = Mosaic::table(ui, "Horizontal scrolling table", 7, options, layout);
            constexpr Mosaic::Array<Mosaic::StringView, 7> headers = {"Line #", "One", "Two", "Three", "Four", "Five", "Six"};
            for(uint32_t column = 0; column != headers.size(); ++column)
            {
                Mosaic::TableColumnOptions columnOptions;
                columnOptions.sizing = Mosaic::TableSizing::Fixed;
                columnOptions.widthOrWeight = column == 0 ? 72.f : 118.f;
                columnOptions.hideable = column != 0;
                Mosaic::tableSetupColumn(ui, column, headers[column], columnOptions);
            }
            Mosaic::tableHeadersRow(ui);
            Mosaic::VisibleRange rows = {0, 20};

            if(scrollVertical == true)
            {
                (void)Mosaic::tableVisibleRows(ui, 20, Mosaic::getTheme(ui).metrics.controlHeight, &rows);
            }

            for(size_t row = rows.begin; row != rows.end; ++row)
            {
                Mosaic::tableNextRow(ui, Mosaic::Key(row));
                for(uint32_t column = 0; column != headers.size(); ++column)
                {
                    if(Mosaic::tableSetColumn(ui, column) == false && column != 0)
                    {
                        continue;
                    }

                    auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));

                    if(column == 0)
                    {
                        Mosaic::String value = "Line ";
                        value += Detail::tableDemoNumber(row);
                        Mosaic::text(ui, value);
                    }
                    else
                    {
                        Mosaic::text(ui, Detail::tableDemoCell(column, row, "Hello world "));
                    }
                }
            }

            Mosaic::spacer(ui, 2.f);
            Mosaic::separatorText(ui, "Stretch + ScrollX");
            Mosaic::helpMarker(ui, "Stretch columns can be combined with horizontal scrolling only when the table has an explicit inner width larger than its visible outer width.");
            static bool stretchScrollHorizontal = true;
            static float stretchInnerWidth = 1000.f;
            Mosaic::checkbox(ui, Mosaic::Key("stretch scrolling flag"), "MosaicTableFlags_ScrollX", &stretchScrollHorizontal);
            Mosaic::SliderOptions innerWidthOptions;
            innerWidthOptions.minimum = 0.0;
            innerWidthOptions.maximum = std::numeric_limits<float>::max();
            innerWidthOptions.dragSpeed = 1.0;
            Mosaic::dragValue(ui, "inner_width", &stretchInnerWidth, innerWidthOptions);
            Mosaic::TableOptions stretchOptions = baseOptions;
            stretchOptions.scrollHorizontal = stretchScrollHorizontal;
            stretchOptions.scrollVertical = true;
            stretchOptions.rowBackground = true;
            stretchOptions.bordersOuterHorizontal = true;
            stretchOptions.bordersOuterVertical = true;
            stretchOptions.contextMenuInBody = true;
            stretchOptions.innerWidth = stretchScrollHorizontal ? std::max(0.f, stretchInnerWidth) : 0.f;
            {
                auto table = Mosaic::table(ui, "Stretch horizontal scrolling table", 7, stretchOptions, layout);
                for(uint32_t column = 0; column != 7; ++column)
                {
                    Mosaic::TableColumnOptions columnOptions;
                    columnOptions.sizing = Mosaic::TableSizing::StretchSame;
                    columnOptions.widthOrWeight = 1.f;
                    Mosaic::tableSetupColumn(ui, column, headers[column], columnOptions);
                }

                for(size_t row = 0; row != 20; ++row)
                {
                    Mosaic::tableNextRow(ui, Mosaic::Key(row));
                    for(uint32_t column = 0; column != 7; ++column)
                    {
                        if(Mosaic::tableSetColumn(ui, column) == false)
                        {
                            continue;
                        }

                        Mosaic::text(ui, Detail::tableDemoCell(column, row, "Hello world "));
                    }
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void drawOuterSizeTableDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            static bool noHostExtendHorizontal = true;
            static bool noHostExtendVertical = false;
            Mosaic::text(ui, "Using NoHostExtendX and NoHostExtendY:");
            Mosaic::checkbox(ui, "MosaicTableFlags_NoHostExtendX", &noHostExtendHorizontal);
            Mosaic::checkbox(ui, "MosaicTableFlags_NoHostExtendY", &noHostExtendVertical);
            {
                Mosaic::TableOptions options = baseOptions;
                options.rowBackground = true;
                options.resizable = true;
                options.contextMenuInBody = true;
                options.bordersInnerHorizontal = true;
                options.bordersInnerVertical = true;
                options.bordersOuterHorizontal = true;
                options.bordersOuterVertical = true;
                options.extendHostHorizontal = noHostExtendHorizontal == false;
                options.extendHostVertical = noHostExtendVertical == false;
                Mosaic::LayoutOptions layout;
                layout.width = noHostExtendHorizontal ? Mosaic::Dimension(Mosaic::SizeRule::Content) : Mosaic::Dimension::fixed(360.f);
                layout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 5.5f);
                auto table = Mosaic::table(ui, "Outer size constrained table", 3, options, layout);
                Detail::tableDemoSetupColumns(ui, 3, Mosaic::TableSizing::FixedFit, 0.f);
                Detail::tableDemoRows(ui, 0, 10, 3);
                Mosaic::sameLine(ui);
                Mosaic::text(ui, "Hello!");
            }

            Mosaic::text(ui, "Using explicit size:");
            for(size_t instance = 0; instance != 2; ++instance)
            {
                if(instance != 0)
                {
                    Mosaic::sameLine(ui);
                }

                auto instanceScope = Mosaic::scope(ui, Mosaic::Key(instance));
                Mosaic::TableOptions options = baseOptions;
                options.rowBackground = true;
                options.bordersInnerHorizontal = true;
                options.bordersInnerVertical = true;
                options.bordersOuterHorizontal = true;
                options.bordersOuterVertical = true;
                Mosaic::LayoutOptions layout;
                layout.width = Mosaic::Dimension::fixed(250.f);
                auto table = Mosaic::table(ui, "Explicit size table", 3, options, layout);
                Detail::tableDemoSetupColumns(ui, 3, Mosaic::TableSizing::StretchSame);
                size_t rowCount = instance == 0 ? 5 : 3;
                for(size_t row = 0; row != rowCount; ++row)
                {
                    if(instance == 1)
                    {
                        Mosaic::TableRowOptions rowOptions;
                        rowOptions.minimumHeight = Mosaic::getTheme(ui).metrics.lineHeight * 1.5f;
                        Mosaic::tableNextRow(ui, Mosaic::Key(row), rowOptions);
                        for(uint32_t column = 0; column != 3; ++column)
                        {
                            if(Mosaic::tableSetColumn(ui, column) == false)
                            {
                                continue;
                            }

                            Mosaic::text(ui, Detail::tableDemoCell(column, row, "Cell "));
                        }
                    }
                    else
                    {
                        Detail::tableDemoRows(ui, row, row + 1, 3);
                    }
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void drawBackgroundColorTableDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            static bool borders = false;
            static bool rowBackground = true;
            static int rowBackgroundType = 1;
            static int rowBackgroundTarget = 1;
            static int cellBackgroundType = 1;
            constexpr Mosaic::Array<Mosaic::StringView, 3> rowTypes = {"None", "Red", "Gradient"};
            constexpr Mosaic::Array<Mosaic::StringView, 2> rowTargets = {"RowBg0", "RowBg1"};
            constexpr Mosaic::Array<Mosaic::StringView, 2> cellTypes = {"None", "Blue"};
            Mosaic::checkbox(ui, "MosaicTableFlags_Borders", &borders);
            Mosaic::checkbox(ui, "MosaicTableFlags_RowBg", &rowBackground);
            Mosaic::comboBox(ui, "row bg type", &rowBackgroundType, rowTypes);
            Mosaic::comboBox(ui, "row bg target", &rowBackgroundTarget, rowTargets);
            Mosaic::comboBox(ui, "cell bg type", &cellBackgroundType, cellTypes);

            Mosaic::TableOptions options = baseOptions;
            options.headers = false;
            options.rowBackground = rowBackground;
            options.bordersInnerHorizontal = borders;
            options.bordersInnerVertical = borders;
            options.bordersOuterHorizontal = borders;
            options.bordersOuterVertical = borders;
            auto table = Mosaic::table(ui, "Background color table", 5, options);
            for(size_t row = 0; row != 6; ++row)
            {
                Mosaic::tableNextRow(ui, Mosaic::Key(row));

                if(rowBackgroundType != 0)
                {
                    uint8_t red = rowBackgroundType == 1 ? uint8_t{178} : static_cast<uint8_t>(51 + row * 20);
                    Mosaic::tableSetRowBackground(ui, rowBackgroundTarget == 0 ? Mosaic::TableBackgroundTarget::Row0 : Mosaic::TableBackgroundTarget::Row1, Mosaic::Color::fromBytes(red, 76, 76, 166));
                }

                for(uint32_t column = 0; column != 5; ++column)
                {
                    if(Mosaic::tableSetColumn(ui, column) == false)
                    {
                        continue;
                    }

                    if(cellBackgroundType == 1 && row >= 1 && row <= 2 && column >= 1 && column <= 2)
                    {
                        Mosaic::tableSetCellBackground(ui, Mosaic::Color::fromBytes(76, 76, 178, 166));
                    }

                    auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));
                    Mosaic::String value;
                    value.push_back(static_cast<char>('A' + row));
                    value.push_back(static_cast<char>('0' + column));
                    Mosaic::text(ui, value);
                }
            }
        }

        struct TableDemoTreeNode
        {
            Mosaic::StringView name;
            Mosaic::StringView type;
            int32_t size = -1;
            size_t firstChild = 0;
            size_t childCount = 0;
        };
        //////////////////////////////////////////////////////////////////////////
        void drawTreeViewTableDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            static bool spanFullWidth = false;
            static bool spanLabelWidth = false;
            static bool spanAllColumns = true;
            static bool labelSpanAllColumns = false;
            Mosaic::checkbox(ui, "MosaicTreeNodeFlags_SpanFullWidth", &spanFullWidth);
            Mosaic::checkbox(ui, "MosaicTreeNodeFlags_SpanLabelWidth", &spanLabelWidth);
            Mosaic::checkbox(ui, "MosaicTreeNodeFlags_SpanAllColumns", &spanAllColumns);
            Mosaic::checkbox(ui, "MosaicTreeNodeFlags_LabelSpanAllColumns", &labelSpanAllColumns);
            Mosaic::helpMarker(ui, "The first column contains a file-system tree while the other columns "
                                   "preserve size and type metadata.");

            constexpr Mosaic::Array<TableDemoTreeNode, 9> nodes = {{{"Root with Long Name", "Folder", -1, 1, 3}, {"Music", "Folder", -1, 4, 2}, {"Textures", "Folder", -1, 6, 3}, {"desktop.ini", "System file", 1024, 0, 0}, {"File1_a.wav", "Audio file", 123000, 0, 0}, {"File1_b.wav", "Audio file", 456000, 0, 0}, {"Image001.png", "Image file", 203128, 0, 0}, {"Copy of Image001.png", "Image file", 203256, 0, 0}, {"Copy of Image001 (Final2).png", "Image file", 203512, 0, 0}}};
            Mosaic::TableOptions options = baseOptions;
            options.rowBackground = true;
            options.resizable = true;
            options.bordersOuterHorizontal = true;
            options.bordersInnerVertical = true;
            options.bordersOuterVertical = true;
            options.bordersInBody = false;
            auto table = Mosaic::table(ui, "File system tree table", 3, options);
            Mosaic::TableColumnOptions nameColumn;
            nameColumn.hideable = false;
            Mosaic::tableSetupColumn(ui, 0, "Name", nameColumn);
            Mosaic::TableColumnOptions sizeColumn;
            sizeColumn.sizing = Mosaic::TableSizing::Fixed;
            sizeColumn.widthOrWeight = 92.f;
            Mosaic::tableSetupColumn(ui, 1, "Size", sizeColumn);
            Mosaic::TableColumnOptions typeColumn;
            typeColumn.sizing = Mosaic::TableSizing::Fixed;
            typeColumn.widthOrWeight = 150.f;
            Mosaic::tableSetupColumn(ui, 2, "Type", typeColumn);
            Mosaic::tableHeadersRow(ui);

            auto drawNode = [ui, &nodes](const auto & self, size_t index, size_t depth) -> void
            {
                const TableDemoTreeNode & value = nodes[index];
                Mosaic::tableNextRow(ui, Mosaic::Key(index));
                (void)Mosaic::tableSetColumn(ui, 0);
                Mosaic::TreeNodeOptions nodeOptions;
                nodeOptions.defaultExpanded = true;
                nodeOptions.leaf = value.childCount == 0;
                nodeOptions.bullet = nodeOptions.leaf;
                nodeOptions.spanFullWidth = spanFullWidth;
                nodeOptions.spanLabelWidth = spanLabelWidth;
                nodeOptions.spanAllColumns = spanAllColumns && index == 0;
                nodeOptions.labelSpanAllColumns = labelSpanAllColumns && index == 0;
                (void)depth;
                auto node = Mosaic::treeNode(ui, Mosaic::Key(index), value.name, nodeOptions);
                bool expanded = node.expanded();

                if(nodeOptions.labelSpanAllColumns == false)
                {
                    (void)Mosaic::tableSetColumn(ui, 1);
                    Mosaic::text(ui, value.childCount == 0 ? Detail::tableDemoNumber(value.size) : Mosaic::String("--"));
                    (void)Mosaic::tableSetColumn(ui, 2);
                    Mosaic::text(ui, value.type);
                }

                if(expanded == true)
                {
                    for(size_t child = 0; child != value.childCount; ++child)
                    {
                        self(self, value.firstChild + child, depth + 1);
                    }
                }
            };
            drawNode(drawNode, 0, 0);
        }
        //////////////////////////////////////////////////////////////////////////
        void tableDemoCustomContextMenu(Mosaic::Context * ui, Mosaic::Id, uint32_t column, void *)
        {
            Mosaic::String label = "Custom column ";
            label += Detail::tableDemoNumber(column);
            Mosaic::text(ui, label);
            Mosaic::menuItem(ui, "Custom action");
        }
        //////////////////////////////////////////////////////////////////////////
        void drawContextMenuTableDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            static bool contextMenuInBody = true;
            Mosaic::helpMarker(ui, "Right-click the header or body to open table actions.");
            Mosaic::checkbox(ui, "MosaicTableFlags_ContextMenuInBody", &contextMenuInBody);
            Mosaic::TableOptions firstOptions = baseOptions;
            firstOptions.contextMenuInBody = contextMenuInBody;
            firstOptions.contextMenu = &Detail::tableDemoCustomContextMenu;
            firstOptions.bordersInnerHorizontal = true;
            firstOptions.bordersInnerVertical = true;
            firstOptions.bordersOuterHorizontal = true;
            firstOptions.bordersOuterVertical = true;
            {
                auto table = Mosaic::table(ui, "Table context menu", 3, firstOptions);
                Detail::tableDemoSetupColumns(ui, 3, Mosaic::TableSizing::StretchSame);
                Mosaic::tableHeadersRow(ui);
                Detail::tableDemoRows(ui, 0, 4, 3);
            }

            Mosaic::helpMarker(ui, "The second table combines its header menu, a per-item popup and "
                                   "custom per-column body actions.");
            Mosaic::TableOptions secondOptions = firstOptions;
            secondOptions.contextMenuInBody = false;
            constexpr uint32_t columnCount = 3;
            static size_t popupRow = 0;
            static uint32_t popupColumn = 0;
            static Mosaic::Id cellPopupOwner = Mosaic::InvalidId;
            static uint32_t bodyPopupColumn = 0;
            const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
            bool itemHovered = false;
            Mosaic::Id tableId = Mosaic::InvalidId;
            {
                auto table = Mosaic::table(ui, "Mixed context menus", columnCount, secondOptions);
                tableId = table.id();
                Detail::tableDemoSetupColumns(ui, columnCount, Mosaic::TableSizing::FixedFit, 0.f);
                Mosaic::tableHeadersRow(ui);
                for(size_t row = 0; row != 4; ++row)
                {
                    Mosaic::tableNextRow(ui, Mosaic::Key(row));
                    for(uint32_t column = 0; column != columnCount; ++column)
                    {
                        if(Mosaic::tableSetColumn(ui, column) == false)
                        {
                            continue;
                        }

                        auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));
                        auto cell = Mosaic::row(ui);
                        Mosaic::Response cellText = Mosaic::text(ui, Detail::tableDemoCell(column, row, "Cell "));
                        Mosaic::ButtonOptions buttonOptions;
                        buttonOptions.width = Mosaic::Dimension::fixed(24.f);
                        Mosaic::Response more = Mosaic::button(ui, Mosaic::Key("cell context button"), "..", buttonOptions);
                        itemHovered = itemHovered || cellText.hovered() || more.hovered();

                        if(pointer != nullptr && more.hovered() == true && pointer->isPressed(Mosaic::PointerButton::Secondary) == true)
                        {
                            popupRow = row;
                            popupColumn = column;
                            cellPopupOwner = more.id;
                            Mosaic::PopupOptions popupOptions;
                            popupOptions.owner = cellPopupOwner;
                            popupOptions.placement = Mosaic::PopupPlacement::Cursor;
                            Mosaic::openPopup(ui, Mosaic::Key("Cell context"), popupOptions);
                        }
                    }
                }
            }
            {
                Mosaic::PopupOptions cellOptions;
                cellOptions.owner = cellPopupOwner;
                cellOptions.placement = Mosaic::PopupPlacement::Cursor;
                auto cellPopup = Mosaic::popup(ui, Mosaic::Key("Cell context"), cellOptions);

                if(cellPopup.visible() == true)
                {
                    Mosaic::String description = "This is the popup for Button(\"..\") in Cell ";
                    description += Detail::tableDemoNumber(popupColumn);
                    description += ",";
                    description += Detail::tableDemoNumber(popupRow);
                    Mosaic::text(ui, description);

                    if(Mosaic::button(ui, "Close").clicked() == true)
                    {
                        Mosaic::closeCurrentPopup(ui);
                    }
                }
            }

            int32_t hoveredColumn = -1;
            for(uint32_t column = 0; column != columnCount; ++column)
            {
                Mosaic::TableColumnStatus status;
                if(Mosaic::tableColumnStatus(ui, tableId, column, &status) == true && status.hovered == true)
                {
                    hoveredColumn = static_cast<int32_t>(column);
                }
            }
            Mosaic::Rect tableBounds;
            bool hasTableBounds = Mosaic::debugBounds(ui, tableId, &tableBounds);

            if(hoveredColumn < 0 && pointer != nullptr && hasTableBounds == true && tableBounds.contains(pointer->position) == true)
            {
                hoveredColumn = static_cast<int32_t>(columnCount);
            }

            if(pointer != nullptr && hoveredColumn >= 0 && itemHovered == false && pointer->isPressed(Mosaic::PointerButton::Secondary) == true)
            {
                bodyPopupColumn = static_cast<uint32_t>(hoveredColumn);
                Mosaic::PopupOptions options;
                options.owner = tableId;
                options.placement = Mosaic::PopupPlacement::Cursor;
                Mosaic::openPopup(ui, Mosaic::Key("Custom column popup"), options);
            }

            {
                Mosaic::PopupOptions bodyOptions;
                bodyOptions.owner = tableId;
                bodyOptions.placement = Mosaic::PopupPlacement::Cursor;
                auto bodyPopup = Mosaic::popup(ui, Mosaic::Key("Custom column popup"), bodyOptions);

                if(bodyPopup.visible() == true)
                {
                    if(bodyPopupColumn == columnCount)
                    {
                        Mosaic::text(ui, "This is a custom popup for unused space after the last column.");
                    }
                    else
                    {
                        Mosaic::String description = "This is a custom popup for Column ";
                        description += Detail::tableDemoNumber(bodyPopupColumn);
                        Mosaic::text(ui, description);
                    }

                    if(Mosaic::button(ui, Mosaic::Key("close custom column popup"), "Close").clicked() == true)
                    {
                        Mosaic::closeCurrentPopup(ui);
                    }
                }
            }

            Mosaic::String hovered = "Hovered column: ";
            hovered += Detail::tableDemoNumber(hoveredColumn);
            Mosaic::text(ui, hovered);
        }

        struct TableDemoSortItem
        {
            int32_t id = 0;
            Mosaic::StringView name;
            int32_t quantity = 0;
        };
        //////////////////////////////////////////////////////////////////////////
        void drawNestedTablesDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            Mosaic::helpMarker(ui, "A nested table is an ordinary item owned by one outer table cell.");
            Mosaic::TableOptions outerOptions = baseOptions;
            outerOptions.resizable = true;
            outerOptions.reorderable = true;
            outerOptions.hideable = true;
            outerOptions.bordersInnerHorizontal = true;
            outerOptions.bordersInnerVertical = true;
            outerOptions.bordersOuterHorizontal = true;
            outerOptions.bordersOuterVertical = true;
            auto outer = Mosaic::table(ui, "Nested A", 2, outerOptions);
            Mosaic::tableSetupColumn(ui, 0, "A0");
            Mosaic::tableSetupColumn(ui, 1, "A1");
            Mosaic::tableHeadersRow(ui);
            Mosaic::tableNextRow(ui, Mosaic::Key(0));

            if(Mosaic::tableSetColumn(ui, 0) == true)
            {
                Mosaic::text(ui, "A0 Row 0");
                Mosaic::TableOptions innerOptions = outerOptions;
                auto inner = Mosaic::table(ui, "Nested B", 2, innerOptions);
                Mosaic::tableSetupColumn(ui, 0, "B0");
                Mosaic::tableSetupColumn(ui, 1, "B1");
                Mosaic::tableHeadersRow(ui);
                for(size_t row = 0; row != 2; ++row)
                {
                    Mosaic::TableRowOptions rowOptions;
                    rowOptions.minimumHeight = Mosaic::getTheme(ui).metrics.lineHeight * 2.f + Mosaic::getTheme(ui).metrics.cellPadding.top * 2.f;
                    Mosaic::tableNextRow(ui, Mosaic::Key(row), rowOptions);
                    for(uint32_t column = 0; column != 2; ++column)
                    {
                        if(Mosaic::tableSetColumn(ui, column) == false)
                        {
                            continue;
                        }

                        Mosaic::String value = column == 0 ? "B0 Row " : "B1 Row ";
                        value += Detail::tableDemoNumber(row);
                        Mosaic::text(ui, value);
                    }
                }
            }

            if(Mosaic::tableSetColumn(ui, 1) == true)
            {
                Mosaic::text(ui, "A1 Row 0");
            }

            Mosaic::tableNextRow(ui, Mosaic::Key(1));

            if(Mosaic::tableSetColumn(ui, 0) == true)
            {
                Mosaic::text(ui, "A0 Row 1");
            }

            if(Mosaic::tableSetColumn(ui, 1) == true)
            {
                Mosaic::text(ui, "A1 Row 1");
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void drawRowHeightDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            Mosaic::helpMarker(ui, "Rows accept a minimum height and an optional per-row vertical cell padding.");
            Mosaic::TableOptions options = baseOptions;
            options.bordersInnerHorizontal = true;
            options.bordersInnerVertical = true;
            options.bordersOuterHorizontal = true;
            options.bordersOuterVertical = true;
            {
                auto table = Mosaic::table(ui, "Minimum row height", 1, options);
                for(size_t row = 0; row != 8; ++row)
                {
                    Mosaic::TableRowOptions rowOptions;
                    rowOptions.minimumHeight = std::floor(Mosaic::getTheme(ui).metrics.lineHeight * 0.3f * static_cast<float>(row) + Mosaic::getTheme(ui).metrics.cellPadding.top * 2.f);
                    Mosaic::tableNextRow(ui, Mosaic::Key(row), rowOptions);

                    if(Mosaic::tableSetColumn(ui, 0) == false)
                    {
                        continue;
                    }

                    Mosaic::String value = "min_row_height = ";
                    value += Detail::tableDemoFixed(rowOptions.minimumHeight, 2);
                    Mosaic::text(ui, value);
                }
            }
            Mosaic::helpMarker(ui, "A cell may contain multiple lines while the table row keeps one shared height.");
            {
                auto table = Mosaic::table(ui, "Shared line height", 2, options);
                for(size_t row = 0; row != 2; ++row)
                {
                    Mosaic::tableNextRow(ui, Mosaic::Key(row));

                    if(Mosaic::tableSetColumn(ui, 0) == true)
                    {
                        Mosaic::Color swatch = Mosaic::Color::fromBytes(33, 66, 102);
                        Mosaic::ColorEditOptions swatchOptions;
                        swatchOptions.width = Mosaic::Dimension::fixed(40.f);
                        swatchOptions.height = Mosaic::Dimension::fixed(40.f);
                        swatchOptions.showInputs = false;
                        swatchOptions.openPickerOnClick = false;
                        Mosaic::colorButton(ui, Mosaic::Key(row), &swatch, true, swatchOptions);
                    }

                    if(Mosaic::tableSetColumn(ui, 1) == true)
                    {
                        if(row == 0)
                        {
                            Mosaic::text(ui, "Line 1");
                            Mosaic::text(ui, "Line 2");
                        }
                        else
                        {
                            Mosaic::SameLineOptions sameLine;
                            sameLine.spacing = 0.f;
                            Mosaic::sameLine(ui, sameLine);
                            Mosaic::text(ui, "Line 1, with SameLine(0,0)");
                            Mosaic::text(ui, "Line 2");
                        }
                    }
                }
            }
            Mosaic::helpMarker(ui, "CellPadding.y may change between rows; horizontal padding remains a table property.");
            {
                auto table = Mosaic::table(ui, "Changing vertical cell padding", 1, options);
                for(size_t row = 0; row != 8; ++row)
                {
                    Mosaic::TableRowOptions rowOptions;
                    rowOptions.cellPaddingY = row % 3 == 2 ? 20.f : Mosaic::getTheme(ui).metrics.cellPadding.top;
                    Mosaic::tableNextRow(ui, Mosaic::Key(row), rowOptions);

                    if(Mosaic::tableSetColumn(ui, 0) == false)
                    {
                        continue;
                    }

                    Mosaic::String value = "CellPadding.y = ";
                    value += Detail::tableDemoFixed(rowOptions.cellPaddingY, 2);
                    Mosaic::text(ui, value);
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void drawItemWidthDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            Mosaic::helpMarker(ui, "Item width is configured once per column and inherited by widgets in that column.");
            Mosaic::TableOptions options = baseOptions;
            options.bordersInnerHorizontal = true;
            options.bordersInnerVertical = true;
            options.bordersOuterHorizontal = true;
            options.bordersOuterVertical = true;
            auto table = Mosaic::table(ui, "Per-column item width", 3, options);
            Mosaic::TableColumnOptions small;
            small.itemWidth = Mosaic::getTheme(ui).metrics.fontSize * 3.f;
            Mosaic::tableSetupColumn(ui, 0, "small", small);
            Mosaic::TableColumnOptions half;
            half.itemWidth = -100.f;
            Mosaic::tableSetupColumn(ui, 1, "half", half);
            Mosaic::TableColumnOptions aligned;
            aligned.itemWidth = -1.f;
            Mosaic::tableSetupColumn(ui, 2, "right-align", aligned);
            Mosaic::tableHeadersRow(ui);
            static float value = 0.f;
            for(size_t row = 0; row != 3; ++row)
            {
                Mosaic::tableNextRow(ui, Mosaic::Key(row));
                for(uint32_t column = 0; column != 3; ++column)
                {
                    if(Mosaic::tableSetColumn(ui, column) == false)
                    {
                        continue;
                    }

                    auto cell = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(row, column)));
                    Mosaic::SliderOptions sliderOptions;
                    sliderOptions.width = Mosaic::SizeRule::Fill;
                    sliderOptions.minimum = 0.0;
                    sliderOptions.maximum = 1.0;
                    Mosaic::slider(ui, column == 0 ? "float0" : column == 1 ? "float1" : Mosaic::StringView{}, &value, sliderOptions);
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void drawSortingTableDemo(Mosaic::Context * ui, const Mosaic::TableOptions & baseOptions)
        {
            static bool multiSort = true;
            static bool tristate = false;
            Mosaic::checkbox(ui, "MosaicTableFlags_SortMulti", &multiSort);
            Mosaic::checkbox(ui, "MosaicTableFlags_SortTristate", &tristate);
            constexpr Mosaic::Array<Mosaic::StringView, 15> names = {"Banana", "Apple", "Cherry", "Watermelon", "Grapefruit", "Strawberry", "Mango", "Kiwi", "Orange", "Pineapple", "Blueberry", "Plum", "Coconut", "Pear", "Apricot"};
            static Mosaic::Array<TableDemoSortItem, 50> items = {};
            static Mosaic::Array<size_t, 50> order = {};
            static bool initialized = false;

            if(initialized == false)
            {
                for(size_t index = 0; index != items.size(); ++index)
                {
                    items[index].id = static_cast<int32_t>(index);
                    items[index].name = names[index % names.size()];
                    items[index].quantity = static_cast<int32_t>((index * index - index) % 20);
                    order[index] = index;
                }
                initialized = true;
            }

            Mosaic::TableOptions options = baseOptions;
            options.sortable = true;
            options.multiSort = multiSort;
            options.sortTristate = tristate;
            options.scrollVertical = true;
            options.rowBackground = true;
            options.bordersOuterHorizontal = true;
            options.bordersOuterVertical = true;
            options.bordersInnerVertical = true;
            options.bordersInBody = false;
            options.frozenRows = 1;
            Mosaic::LayoutOptions layout;
            layout.width = Mosaic::SizeRule::Fill;
            layout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 15.f);
            auto table = Mosaic::table(ui, "Sorting table", 4, options, layout);
            Mosaic::TableColumnOptions idColumn;
            idColumn.userId = Mosaic::Key("sorting id").value();
            idColumn.sizing = Mosaic::TableSizing::FixedFit;
            idColumn.widthOrWeight = 0.f;
            idColumn.defaultSort = true;
            Mosaic::tableSetupColumn(ui, 0, "ID", idColumn);
            Mosaic::TableColumnOptions nameColumn = idColumn;
            nameColumn.userId = Mosaic::Key("sorting name").value();
            nameColumn.defaultSort = false;
            Mosaic::tableSetupColumn(ui, 1, "Name", nameColumn);
            Mosaic::TableColumnOptions actionColumn = idColumn;
            actionColumn.userId = Mosaic::Key("sorting action").value();
            actionColumn.defaultSort = false;
            actionColumn.sortable = false;
            Mosaic::tableSetupColumn(ui, 2, "Action", actionColumn);
            Mosaic::TableColumnOptions quantityColumn;
            quantityColumn.userId = Mosaic::Key("sorting quantity").value();
            quantityColumn.sizing = Mosaic::TableSizing::Stretch;
            quantityColumn.widthOrWeight = 1.f;
            quantityColumn.preferredSort = Mosaic::SortDirection::Descending;
            Mosaic::tableSetupColumn(ui, 3, "Quantity", quantityColumn);
            Mosaic::tableHeadersRow(ui);

            Mosaic::TableSortState sortState;
            if(Mosaic::tableSortState(ui, table.id(), &sortState) == true && sortState.dirty == true)
            {
                for(size_t index = 0; index != order.size(); ++index)
                {
                    order[index] = index;
                }
                for(size_t specIndex = sortState.specifications.size(); specIndex > 0; --specIndex)
                {
                    Mosaic::TableSortSpec spec = sortState.specifications[specIndex - 1];
                    std::stable_sort(order.begin(), order.end(),
                                     [spec](size_t first, size_t second)
                                     {
                                         const TableDemoSortItem & left = items[first];
                                         const TableDemoSortItem & right = items[second];
                                         int comparison = 0;

                                         if(spec.userId == Mosaic::Key("sorting id").value())
                                         {
                                             comparison = left.id - right.id;
                                         }
                                         else if(spec.userId == Mosaic::Key("sorting name").value())
                                         {
                                             comparison = left.name.compare(right.name);
                                         }
                                         else if(spec.userId == Mosaic::Key("sorting quantity").value())
                                         {
                                             comparison = left.quantity - right.quantity;
                                         }

                                         return spec.direction == Mosaic::SortDirection::Ascending ? comparison < 0 : comparison > 0;
                                     });
                }
                Mosaic::tableSortSpecsHandled(ui, table.id());
            }

            Mosaic::VisibleRange rows;
            (void)Mosaic::tableVisibleRows(ui, order.size(), Mosaic::getTheme(ui).metrics.controlHeight, &rows);
            for(size_t visibleRow = rows.begin; visibleRow != rows.end; ++visibleRow)
            {
                const TableDemoSortItem & item = items[order[visibleRow]];
                Mosaic::tableNextRow(ui, Mosaic::Key(static_cast<Mosaic::Id>(item.id)));
                (void)Mosaic::tableSetColumn(ui, 0);
                Mosaic::String id = Detail::tableDemoNumber(item.id);
                while(id.size() < 4)
                {
                    id.insert(id.begin(), '0');
                }
                Mosaic::text(ui, id);
                (void)Mosaic::tableSetColumn(ui, 1);
                Mosaic::text(ui, item.name);
                (void)Mosaic::tableSetColumn(ui, 2);
                Mosaic::button(ui, Mosaic::Key("sorting action"), "None");
                (void)Mosaic::tableSetColumn(ui, 3);
                Mosaic::text(ui, Detail::tableDemoNumber(item.quantity));
            }
        }

        //////////////////////////////////////////////////////////////////////////
        bool drawReferenceTableDemo(Mosaic::Context * ui, Mosaic::StringView label, const Mosaic::TableOptions & baseOptions)
        {
            if(label == "Resizable, stretch")
            {
                Detail::drawResizableTableDemo(ui, baseOptions, false);
            }
            else if(label == "Resizable, fixed")
            {
                Detail::drawResizableTableDemo(ui, baseOptions, true);
            }
            else if(label == "Vertical scrolling, with clipping")
            {
                Detail::drawVerticalScrollingTableDemo(ui, baseOptions);
            }
            else if(label == "Horizontal scrolling")
            {
                Detail::drawHorizontalScrollingTableDemo(ui, baseOptions);
            }
            else if(label == "Outer size")
            {
                Detail::drawOuterSizeTableDemo(ui, baseOptions);
            }
            else if(label == "Background color")
            {
                Detail::drawBackgroundColorTableDemo(ui, baseOptions);
            }
            else if(label == "Tree view")
            {
                Detail::drawTreeViewTableDemo(ui, baseOptions);
            }
            else if(label == "Nested tables")
            {
                Detail::drawNestedTablesDemo(ui, baseOptions);
            }
            else if(label == "Row height")
            {
                Detail::drawRowHeightDemo(ui, baseOptions);
            }
            else if(label == "Item width")
            {
                Detail::drawItemWidthDemo(ui, baseOptions);
            }
            else if(label == "Context menus")
            {
                Detail::drawContextMenuTableDemo(ui, baseOptions);
            }
            else if(label == "Sorting")
            {
                Detail::drawSortingTableDemo(ui, baseOptions);
            }
            else
            {
                return false;
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
} // namespace MosaicExample
