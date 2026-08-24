#include "Mosaic/SelectionModel.hpp"

#include <algorithm>

namespace Mosaic
{
    //////////////////////////////////////////////////////////////////////////
    SelectionModel::SelectionModel(SelectionMode mode) noexcept : m_mode(mode)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void SelectionModel::select(Id id, bool extend, bool toggle)
    {
        bool wasSelected = m_membership.contains(id);

        if(m_mode == SelectionMode::Single || extend == false)
        {
            m_values.clear();
            m_membership.clear();
        }

        if(toggle == true && wasSelected == true)
        {
            if(extend == true)
            {
                m_values.erase(std::remove(m_values.begin(), m_values.end(), id), m_values.end());
                m_membership.erase(id);
            }
        }
        else if(m_membership.insert(id).second)
        {
            m_values.push_back(id);
        }

        m_anchor = id;
    }
    //////////////////////////////////////////////////////////////////////////
    void SelectionModel::selectRange(IdSpan orderedIds, Id to)
    {
        if(m_mode == SelectionMode::Single)
        {
            select(to);

            return;
        }

        if(m_anchor == InvalidId)
        {
            select(to);

            return;
        }

        auto first = std::find(orderedIds.begin(), orderedIds.end(), m_anchor);
        auto last = std::find(orderedIds.begin(), orderedIds.end(), to);

        if(first == orderedIds.end())
        {
            select(to);

            return;
        }

        if(last == orderedIds.end())
        {
            select(to);

            return;
        }

        auto begin = std::min(first, last);
        auto end = std::max(first, last);
        m_values.assign(begin, end + 1);
        rebuildMembership();
    }
    //////////////////////////////////////////////////////////////////////////
    void SelectionModel::selectAll(IdSpan ids)
    {
        if(m_mode == SelectionMode::Single)
        {
            if(ids.empty() == false)
            {
                select(ids.front());
            }

            return;
        }

        m_values.assign(ids.begin(), ids.end());
        rebuildMembership();
        m_anchor = ids.empty() == true ? InvalidId : ids.front();
    }
    //////////////////////////////////////////////////////////////////////////
    void SelectionModel::remove(Id id)
    {
        if(m_membership.erase(id) == 0)
        {
            return;
        }

        m_values.erase(std::remove(m_values.begin(), m_values.end(), id), m_values.end());

        if(m_anchor == id)
        {
            m_anchor = m_values.empty() == true ? InvalidId : m_values.back();
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void SelectionModel::clear() noexcept
    {
        m_values.clear();
        m_membership.clear();
        m_anchor = InvalidId;
    }
    //////////////////////////////////////////////////////////////////////////
    bool SelectionModel::selected(Id id) const noexcept
    {
        auto returnedValue = m_membership.contains(id);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool SelectionModel::empty() const noexcept
    {
        auto returnedValue = m_values.empty() == true;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    size_t SelectionModel::size() const noexcept
    {
        auto returnedValue = m_values.size();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    SelectionMode SelectionModel::mode() const noexcept
    {
        return m_mode;
    }
    //////////////////////////////////////////////////////////////////////////
    Id SelectionModel::anchor() const noexcept
    {
        return m_anchor;
    }
    //////////////////////////////////////////////////////////////////////////
    IdSpan SelectionModel::values() const noexcept
    {
        return m_values;
    }
    //////////////////////////////////////////////////////////////////////////
    void SelectionModel::rebuildMembership()
    {
        m_membership.clear();
        m_membership.reserve(m_values.size());
        m_membership.insert(m_values.begin(), m_values.end());
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
