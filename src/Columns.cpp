#include "Context.hpp"

#include <algorithm>
#include <limits>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t currentColumnsNode(const Context * ui) noexcept
        {
            if(ui == nullptr)
            {
                auto returnedValue = std::numeric_limits<size_t>::max();

                return returnedValue;
            }

            if(ui->currentParent >= ui->nodes.size())
            {
                auto returnedValue = std::numeric_limits<size_t>::max();

                return returnedValue;
            }

            if(ui->nodes[ui->currentParent].kind != Detail::NodeKind::Table)
            {
                auto returnedValue = std::numeric_limits<size_t>::max();

                return returnedValue;
            }

            return ui->currentParent;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] uint32_t resolvedColumn(const Context * ui, const Context::TableState & state, int32_t column) noexcept
        {
            uint32_t count = ui->nodes[ui->currentParent].layout.columns;

            if(count == 0)
            {
                return 0;
            }

            if(column < 0)
            {
                auto returnedValue = std::min(state.currentColumn, count - 1);

                return returnedValue;
            }

            auto returnedValue = std::min(static_cast<uint32_t>(column), count - 1);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float storedColumnWidth(const Context::TableState & state, uint32_t column) noexcept
        {
            if(column >= state.columns.size())
            {
                return 0.f;
            }

            const Context::TableColumnState & value = state.columns[column];

            if(value.resolvedWidth > 0.f)
            {
                return value.resolvedWidth;
            }

            bool storedWidthAvailable = value.options.widthOrWeight > 0.f;
            bool fixedSizing = value.options.sizing == TableSizing::Fixed;

            if(value.options.sizing == TableSizing::FixedFit)
            {
                fixedSizing = true;
            }

            if(value.options.sizing == TableSizing::FixedSame)
            {
                fixedSizing = true;
            }

            if(fixedSizing == false)
            {
                storedWidthAvailable = false;
            }

            if(storedWidthAvailable == true)
            {
                return value.options.widthOrWeight;
            }

            return value.lastBounds.width;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Scope columns(Context * ui, StringView label, uint32_t count, const ColumnsOptions & options, const LayoutOptions & layout, const SourceLocation & location)
    {
        TableOptions tableOptions;
        tableOptions.headers = false;
        tableOptions.resizable = options.resizable;
        tableOptions.reorderable = false;
        tableOptions.hideable = false;
        tableOptions.sortable = false;
        tableOptions.bordersInnerHorizontal = options.horizontalBorders;
        tableOptions.bordersOuterHorizontal = options.horizontalBorders;
        tableOptions.bordersInnerVertical = options.border;
        tableOptions.bordersOuterVertical = options.border;
        tableOptions.clipCells = options.clip;
        tableOptions.saveSettings = true;
        Scope result = Mosaic::table(ui, label, std::max(1U, count), tableOptions, layout, location);
        for(uint32_t column = 0; column != std::max(1U, count); ++column)
        {
            TableColumnOptions columnOptions;
            columnOptions.sizing = TableSizing::Stretch;
            columnOptions.widthOrWeight = std::max(1.f, options.minimumWidth);
            columnOptions.sortable = false;
            columnOptions.reorderable = false;
            columnOptions.hideable = false;
            columnOptions.resizable = options.resizable;
            columnOptions.clip = options.clip;
            Mosaic::tableSetupColumn(ui, column, {}, columnOptions);
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool nextColumn(Context * ui)
    {
        auto returnedValue = Mosaic::tableNextColumn(ui);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    uint32_t columnIndex(const Context * ui) noexcept
    {
        size_t node = Detail::currentColumnsNode(ui);

        if(node == std::numeric_limits<size_t>::max())
        {
            return 0;
        }

        const Context::TableState * state = ui->findTableState(ui->nodes[node].id);

        return state == nullptr ? 0 : state->currentColumn;
    }
    //////////////////////////////////////////////////////////////////////////
    uint32_t columnCount(const Context * ui) noexcept
    {
        size_t node = Detail::currentColumnsNode(ui);
        auto returnedValue = node == std::numeric_limits<size_t>::max() ? 0 : ui->nodes[node].layout.columns;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    float columnWidth(const Context * ui, int32_t column) noexcept
    {
        size_t node = Detail::currentColumnsNode(ui);

        if(node == std::numeric_limits<size_t>::max())
        {
            return 0.f;
        }

        const Context::TableState * state = ui->findTableState(ui->nodes[node].id);

        if(state == nullptr)
        {
            return 0.f;
        }

        auto returnedValue = Detail::storedColumnWidth(*state, Detail::resolvedColumn(ui, *state, column));

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void setColumnWidth(Context * ui, int32_t column, float width) noexcept
    {
        size_t node = Detail::currentColumnsNode(ui);

        if(node == std::numeric_limits<size_t>::max())
        {
            return;
        }

        Context::TableState & state = ui->tableState(ui->nodes[node].id);
        uint32_t resolved = Detail::resolvedColumn(ui, state, column);

        if(resolved >= state.columns.size())
        {
            return;
        }

        Context::TableColumnState & value = state.columns[resolved];
        value.options.sizing = TableSizing::Fixed;
        value.options.widthOrWeight = std::max(1.f, width);
        value.resolvedWidth = value.options.widthOrWeight;
        value.configured = true;
    }
    //////////////////////////////////////////////////////////////////////////
    float columnOffset(const Context * ui, int32_t column) noexcept
    {
        size_t node = Detail::currentColumnsNode(ui);

        if(node == std::numeric_limits<size_t>::max())
        {
            return 0.f;
        }

        const Context::TableState * state = ui->findTableState(ui->nodes[node].id);

        if(state == nullptr)
        {
            return 0.f;
        }

        uint32_t resolved = Detail::resolvedColumn(ui, *state, column);

        if(resolved < state->columnPositions.size() && state->columnPositions.empty() == false)
        {
            auto returnedValue = state->columnPositions[resolved] - state->columnPositions.front();

            return returnedValue;
        }

        float offset = 0.f;
        for(uint32_t index = 0; index != resolved; ++index)
        {
            offset += Detail::storedColumnWidth(*state, index);
        }

        return offset;
    }
    //////////////////////////////////////////////////////////////////////////
    void setColumnOffset(Context * ui, int32_t column, float offset) noexcept
    {
        size_t node = Detail::currentColumnsNode(ui);

        if(node == std::numeric_limits<size_t>::max())
        {
            return;
        }

        Context::TableState & state = ui->tableState(ui->nodes[node].id);
        uint32_t resolved = Detail::resolvedColumn(ui, state, column);

        if(resolved == 0)
        {
            return;
        }

        float precedingOffset = 0.f;
        for(uint32_t index = 0; index + 1 < resolved; ++index)
        {
            precedingOffset += Detail::storedColumnWidth(state, index);
        }
        Mosaic::setColumnWidth(ui, static_cast<int32_t>(resolved - 1), std::max(1.f, offset - precedingOffset));
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
