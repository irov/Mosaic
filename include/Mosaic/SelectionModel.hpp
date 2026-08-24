#pragma once

#include "Mosaic/Types.hpp"

namespace Mosaic
{
    enum class SelectionMode : uint8_t
    {
        Single,
        Multiple
    };

    class SelectionModel
    {
    public:
        explicit SelectionModel(SelectionMode mode = SelectionMode::Single) noexcept;

        void select(Id id, bool extend = false, bool toggle = false);
        void selectRange(IdSpan orderedIds, Id to);
        void selectAll(IdSpan ids);
        void remove(Id id);
        void clear() noexcept;
        [[nodiscard]] bool selected(Id id) const noexcept;
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] SelectionMode mode() const noexcept;
        [[nodiscard]] Id anchor() const noexcept;
        [[nodiscard]] IdSpan values() const noexcept;

    private:
        void rebuildMembership();

    private:
        SelectionMode m_mode;
        Id m_anchor = InvalidId;
        IdVector m_values;
        IdSet m_membership;
    };
} // namespace Mosaic
