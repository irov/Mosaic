#include "Mosaic/Docking.hpp"

#include <algorithm>
#include <cmath>

namespace Mosaic
{
    //////////////////////////////////////////////////////////////////////////
    DockModel::DockModel()
    {
        clear();
    }
    //////////////////////////////////////////////////////////////////////////
    void DockModel::clear()
    {
        m_nodes.clear();
        m_nodeIndices.clear();
        m_windowNodes.clear();
        m_nextId = 1;
        m_root = createNode(DockNodeType::Tabs);
        m_central = m_root;
        node(m_central)->central = true;
    }
    //////////////////////////////////////////////////////////////////////////
    DockNodeId DockModel::root() const noexcept
    {
        return m_root;
    }
    //////////////////////////////////////////////////////////////////////////
    DockNodeId DockModel::centralNode() const noexcept
    {
        return m_central;
    }
    //////////////////////////////////////////////////////////////////////////
    const DockNode * DockModel::node(DockNodeId id) const noexcept
    {
        auto iterator = m_nodeIndices.find(id);
        auto returnedValue = iterator == m_nodeIndices.end() || iterator->second >= m_nodes.size() ? nullptr : &m_nodes[iterator->second];

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DockNode * DockModel::node(DockNodeId id) noexcept
    {
        auto iterator = m_nodeIndices.find(id);
        auto returnedValue = iterator == m_nodeIndices.end() || iterator->second >= m_nodes.size() ? nullptr : &m_nodes[iterator->second];

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DockNodeId DockModel::nodeForWindow(Id window) const noexcept
    {
        auto iterator = m_windowNodes.find(window);
        auto returnedValue = iterator == m_windowNodes.end() ? 0 : iterator->second;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DockNodeSpan DockModel::nodes() const noexcept
    {
        return m_nodes;
    }
    //////////////////////////////////////////////////////////////////////////
    DockNodeId DockModel::createNode(DockNodeType type)
    {
        DockNodeId id = m_nextId++;
        DockNode value;
        value.id = id;
        value.type = type;
        m_nodes.emplace_back(std::move(value));
        m_nodeIndices[id] = m_nodes.size() - 1;

        return id;
    }
    //////////////////////////////////////////////////////////////////////////
    void DockModel::removeNode(DockNodeId id)
    {
        m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                                     [id](const DockNode & candidate)
                                     {
                                         return candidate.id == id;
                                     }),
                      m_nodes.end());
        rebuildIndices();
    }
    //////////////////////////////////////////////////////////////////////////
    void DockModel::rebuildIndices()
    {
        m_nodeIndices.clear();
        m_windowNodes.clear();
        m_nodeIndices.reserve(m_nodes.size());
        for(size_t index = 0; index != m_nodes.size(); ++index)
        {
            const DockNode & current = m_nodes[index];
            m_nodeIndices[current.id] = index;

            if(current.type != DockNodeType::Tabs)
            {
                continue;
            }

            for(Id window : current.tabs)
            {
                m_windowNodes[window] = current.id;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::dock(Id window, DockNodeId target, DockPlacement placement, float ratio)
    {
        if(window == InvalidId)
        {
            return false;
        }

        DockNode * targetNode = node(target);

        if(targetNode == nullptr)
        {
            return false;
        }

        if(targetNode->type == DockNodeType::Split)
        {
            target = targetNode->children[0];
            targetNode = node(target);

            if(targetNode == nullptr)
            {
                return false;
            }
        }

        DockNodeId oldNode = nodeForWindow(window);

        if(oldNode != 0)
        {
            if(oldNode == target && placement == DockPlacement::Center)
            {
                auto returnedValue = activate(window);

                return returnedValue;
            }

            undock(window);
            targetNode = node(target);

            if(targetNode == nullptr)
            {
                target = m_root;
                targetNode = node(target);
                while(targetNode != nullptr && targetNode->type == DockNodeType::Split)
                {
                    target = targetNode->children[0];
                    targetNode = node(target);
                }
            }
        }

        if(targetNode == nullptr)
        {
            return false;
        }

        if(targetNode->type != DockNodeType::Tabs)
        {
            return false;
        }

        if(placement == DockPlacement::Center)
        {
            targetNode->tabs.push_back(window);
            targetNode->activeTab = window;
            m_windowNodes[window] = target;

            return true;
        }

        DockNodeId previousParent = targetNode->parent;
        DockNodeId newTabsId = createNode(DockNodeType::Tabs);
        DockNodeId splitId = createNode(DockNodeType::Split);
        DockNode * newTabs = node(newTabsId);
        DockNode * splitNode = node(splitId);
        targetNode = node(target);

        if(newTabs == nullptr)
        {
            return false;
        }

        if(splitNode == nullptr)
        {
            return false;
        }

        if(targetNode == nullptr)
        {
            return false;
        }

        newTabs->tabs.push_back(window);
        newTabs->activeTab = window;
        m_windowNodes[window] = newTabsId;
        newTabs->parent = splitId;
        targetNode->parent = splitId;
        splitNode->parent = previousParent;
        splitNode->orientation = placement == DockPlacement::Left || placement == DockPlacement::Right ? Orientation::Horizontal : Orientation::Vertical;
        splitNode->ratio = std::clamp(ratio, 0.05f, 0.95f);
        bool before = placement == DockPlacement::Left || placement == DockPlacement::Top;
        splitNode->children = before ? DockNodeIdPair{newTabsId, target} : DockNodeIdPair{target, newTabsId};

        if(previousParent == 0)
        {
            m_root = splitId;
        }
        else
        {
            DockNode * parentNode = node(previousParent);

            if(parentNode == nullptr)
            {
                return false;
            }

            if(parentNode->type != DockNodeType::Split)
            {
                return false;
            }

            if(parentNode->children[0] == target)
            {
                parentNode->children[0] = splitId;
            }
            else if(parentNode->children[1] == target)
            {
                parentNode->children[1] = splitId;
            }
            else
            {
                return false;
            }
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::undock(Id window)
    {
        DockNodeId ownerId = nodeForWindow(window);
        DockNode * owner = node(ownerId);

        if(owner == nullptr)
        {
            return false;
        }

        owner->tabs.erase(std::remove(owner->tabs.begin(), owner->tabs.end(), window), owner->tabs.end());
        m_windowNodes.erase(window);

        if(owner->activeTab == window)
        {
            owner->activeTab = owner->tabs.empty() == true ? InvalidId : owner->tabs.front();
        }

        if(owner->tabs.empty() == true)
        {
            collapseEmpty(ownerId);
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void DockModel::collapseEmpty(DockNodeId id)
    {
        DockNode * empty = node(id);

        if(empty == nullptr)
        {
            return;
        }

        if(empty->type != DockNodeType::Tabs)
        {
            return;
        }

        if(empty->tabs.empty() == false)
        {
            return;
        }

        if(empty->parent == 0)
        {
            return;
        }

        DockNodeId parentId = empty->parent;
        DockNode * parentNode = node(parentId);

        if(parentNode == nullptr)
        {
            return;
        }

        if(parentNode->type != DockNodeType::Split)
        {
            return;
        }

        DockNodeId siblingId = parentNode->children[0] == id ? parentNode->children[1] : parentNode->children[0];
        DockNode * sibling = node(siblingId);

        if(sibling == nullptr)
        {
            return;
        }

        if(m_central == id)
        {
            m_central = siblingId;
            sibling->central = true;
        }

        DockNodeId grandParentId = parentNode->parent;
        sibling->parent = grandParentId;

        if(grandParentId == 0)
        {
            m_root = siblingId;
        }
        else
        {
            DockNode * grandParent = node(grandParentId);

            if(grandParent != nullptr)
            {
                if(grandParent->children[0] == parentId)
                {
                    grandParent->children[0] = siblingId;
                }
                else if(grandParent->children[1] == parentId)
                {
                    grandParent->children[1] = siblingId;
                }
            }
        }

        removeNode(id);
        removeNode(parentId);
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::activate(Id window)
    {
        DockNode * owner = node(nodeForWindow(window));

        if(owner == nullptr)
        {
            return false;
        }

        owner->activeTab = window;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::reorder(Id window, size_t newIndex)
    {
        DockNode * owner = node(nodeForWindow(window));

        if(owner == nullptr)
        {
            return false;
        }

        if(newIndex >= owner->tabs.size())
        {
            return false;
        }

        auto iterator = std::find(owner->tabs.begin(), owner->tabs.end(), window);

        if(iterator == owner->tabs.end())
        {
            return false;
        }

        owner->tabs.erase(iterator);
        owner->tabs.insert(owner->tabs.begin() + static_cast<std::ptrdiff_t>(newIndex), window);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::setSplitRatio(DockNodeId split, float ratio)
    {
        DockNode * splitNode = node(split);

        if(splitNode == nullptr)
        {
            return false;
        }

        if(splitNode->type != DockNodeType::Split)
        {
            return false;
        }

        if(std::isfinite(ratio) == false)
        {
            return false;
        }

        splitNode->ratio = std::clamp(ratio, 0.05f, 0.95f);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::setCentralNode(DockNodeId value)
    {
        DockNode * selected = node(value);

        if(selected == nullptr)
        {
            return false;
        }

        if(selected->type != DockNodeType::Tabs)
        {
            return false;
        }

        for(DockNode & candidate : m_nodes)
        {
            candidate.central = false;
        }
        selected->central = true;
        m_central = value;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::restore(DockNodeSpan input, DockNodeId rootId)
    {
        if(input.empty() == true)
        {
            return false;
        }

        if(rootId == 0)
        {
            return false;
        }

        m_nodes.assign(input.begin(), input.end());
        m_root = rootId;
        m_central = 0;
        m_nextId = 1;
        for(const DockNode & value : m_nodes)
        {
            m_nextId = std::max(m_nextId, value.id + 1);
        }
        rebuildIndices();

        if(validate() == false)
        {
            clear();

            return false;
        }

        DockNodeId restoredCentral = 0;
        for(const DockNode & value : m_nodes)
        {
            if(restoredCentral == 0 && value.central == true && value.type == DockNodeType::Tabs)
            {
                restoredCentral = value.id;
            }
        }

        if(restoredCentral != 0)
        {
            (void)setCentralNode(restoredCentral);
        }
        else
        {
            DockNodeId candidate = m_root;
            const DockNode * candidateNode = node(candidate);
            while(candidateNode != nullptr && candidateNode->type == DockNodeType::Split)
            {
                candidate = candidateNode->children[0];
                candidateNode = node(candidate);
            }

            if(candidateNode != nullptr)
            {
                (void)setCentralNode(candidate);
            }
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::validate() const noexcept
    {
        if(node(m_root) == nullptr)
        {
            return false;
        }

        DockNodeIdSet visited;
        DockNodeIdVector pending = {m_root};
        IdSet windows;
        while(pending.empty() == false)
        {
            DockNodeId id = pending.back();
            pending.pop_back();

            if(visited.insert(id).second == false)
            {
                return false;
            }

            const DockNode * current = node(id);

            if(current == nullptr)
            {
                return false;
            }

            if(current->type == DockNodeType::Split)
            {
                if(current->children[0] == 0)
                {
                    return false;
                }

                if(current->children[1] == 0)
                {
                    return false;
                }

                if(current->ratio <= 0.f)
                {
                    return false;
                }

                if(current->ratio >= 1.f)
                {
                    return false;
                }

                for(DockNodeId child : current->children)
                {
                    const DockNode * childNode = node(child);

                    if(childNode == nullptr)
                    {
                        return false;
                    }

                    if(childNode->parent != id)
                    {
                        return false;
                    }

                    pending.push_back(child);
                }
            }
            else
            {
                if(current->tabs.empty() == true && id != m_root)
                {
                    return false;
                }

                for(Id window : current->tabs)
                {
                    if(windows.insert(window).second == false)
                    {
                        return false;
                    }
                }

                if(current->tabs.empty() == false && std::find(current->tabs.begin(), current->tabs.end(), current->activeTab) == current->tabs.end())
                {
                    return false;
                }
            }
        }
        auto returnedValue = visited.size() == m_nodes.size();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DockLayoutEntryVector DockModel::layout(const Rect & bounds) const
    {
        DockLayoutEntryVector output;
        layoutNode(m_root, bounds, output);

        return output;
    }
    //////////////////////////////////////////////////////////////////////////
    DockLayoutEntryVector DockModel::layout(const Rect & bounds, IdSpan visibleWindows, IdSpan collapsedWindows, float collapsedExtent, float splitterExtent) const
    {
        DockLayoutEntryVector output;
        layoutVisibleNode(m_root, bounds, visibleWindows, collapsedWindows, collapsedExtent, std::max(0.f, splitterExtent), &output, nullptr);

        return output;
    }
    //////////////////////////////////////////////////////////////////////////
    void DockModel::layout(const Rect & bounds, IdSpan visibleWindows, IdSpan collapsedWindows, float collapsedExtent, float splitterExtent, DockLayoutEntryVector & windows, DockSplitterLayoutEntryVector & splitters) const
    {
        layoutVisibleNode(m_root, bounds, visibleWindows, collapsedWindows, collapsedExtent, std::max(0.f, splitterExtent), &windows, &splitters);
    }
    //////////////////////////////////////////////////////////////////////////
    DockSplitterLayoutEntryVector DockModel::splitterLayout(const Rect & bounds, IdSpan visibleWindows, IdSpan collapsedWindows, float collapsedExtent, float splitterExtent) const
    {
        DockSplitterLayoutEntryVector output;
        layoutVisibleNode(m_root, bounds, visibleWindows, collapsedWindows, collapsedExtent, std::max(0.f, splitterExtent), nullptr, &output);

        return output;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DockModel::subtreeVisible(DockNodeId id, IdSpan visibleWindows) const noexcept
    {
        const DockNode * current = node(id);

        if(current == nullptr)
        {
            return false;
        }

        if(current->type == DockNodeType::Tabs)
        {
            if(visibleWindows.empty() == true)
            {
                auto returnedValue = current->tabs.empty() == false;

                return returnedValue;
            }

            bool returnedValue = std::any_of(current->tabs.begin(), current->tabs.end(),
                                                   [visibleWindows](Id window)
                                                   {
                                                       auto returnedValue = std::find(visibleWindows.begin(), visibleWindows.end(), window) != visibleWindows.end();

                                                       return returnedValue;
                                                   });

            return returnedValue;
        }
        auto returnedValue = subtreeVisible(current->children[0], visibleWindows) || subtreeVisible(current->children[1], visibleWindows);

        return returnedValue;
    }

    //////////////////////////////////////////////////////////////////////////
    bool DockModel::subtreeCollapsed(DockNodeId id, IdSpan visibleWindows, IdSpan collapsedWindows) const noexcept
    {
        const DockNode * current = node(id);

        if(current == nullptr)
        {
            return false;
        }

        if(subtreeVisible(id, visibleWindows) == false)
        {
            return false;
        }

        if(current->type == DockNodeType::Tabs)
        {
            Id active = current->activeTab;

            if(visibleWindows.empty() == false && std::find(visibleWindows.begin(), visibleWindows.end(), active) == visibleWindows.end())
            {
                auto visible = std::find_if(current->tabs.begin(), current->tabs.end(),
                                                  [visibleWindows](Id window)
                                                  {
                                                      auto returnedValue = std::find(visibleWindows.begin(), visibleWindows.end(), window) != visibleWindows.end();

                                                      return returnedValue;
                                                  });
                active = visible == current->tabs.end() ? InvalidId : *visible;
            }
            auto returnedValue = active != InvalidId && std::find(collapsedWindows.begin(), collapsedWindows.end(), active) != collapsedWindows.end();

            return returnedValue;
        }

        auto returnedValue = subtreeCollapsed(current->children[0], visibleWindows, collapsedWindows) && subtreeCollapsed(current->children[1], visibleWindows, collapsedWindows);

        return returnedValue;
    }

    //////////////////////////////////////////////////////////////////////////
    void DockModel::layoutVisibleNode(DockNodeId id, const Rect & bounds, IdSpan visibleWindows, IdSpan collapsedWindows, float collapsedExtent, float splitterExtent, DockLayoutEntryVector * windows, DockSplitterLayoutEntryVector * splitters) const
    {
        const DockNode * current = node(id);

        if(current == nullptr)
        {
            return;
        }

        if(subtreeVisible(id, visibleWindows) == false)
        {
            return;
        }

        if(current->type == DockNodeType::Tabs)
        {
            if(windows == nullptr)
            {
                return;
            }

            Id active = current->activeTab;

            if(visibleWindows.empty() == false && std::find(visibleWindows.begin(), visibleWindows.end(), active) == visibleWindows.end())
            {
                auto visible = std::find_if(current->tabs.begin(), current->tabs.end(),
                                                  [visibleWindows](Id window)
                                                  {
                                                      auto returnedValue = std::find(visibleWindows.begin(), visibleWindows.end(), window) != visibleWindows.end();

                                                      return returnedValue;
                                                  });
                active = visible == current->tabs.end() ? InvalidId : *visible;
            }
            for(Id window : current->tabs)
            {
                if(visibleWindows.empty() == false && std::find(visibleWindows.begin(), visibleWindows.end(), window) == visibleWindows.end())
                {
                    continue;
                }

                windows->push_back({window, id, bounds, window == active});
            }

            return;
        }

        bool firstVisible = subtreeVisible(current->children[0], visibleWindows);
        bool secondVisible = subtreeVisible(current->children[1], visibleWindows);

        if(firstVisible != secondVisible)
        {
            layoutVisibleNode(firstVisible ? current->children[0] : current->children[1], bounds, visibleWindows, collapsedWindows, collapsedExtent, splitterExtent, windows, splitters);

            return;
        }

        float separator = std::min(splitterExtent, current->orientation == Orientation::Horizontal ? bounds.width : bounds.height);
        Rect first = bounds;
        Rect second = bounds;

        if(current->orientation == Orientation::Horizontal)
        {
            float content = std::max(0.f, bounds.width - separator);
            first.width = content * current->ratio;
            second.x = first.right() + separator;
            second.width = std::max(0.f, bounds.right() - second.x);
        }
        else
        {
            float content = std::max(0.f, bounds.height - separator);
            bool firstCollapsed = subtreeCollapsed(current->children[0], visibleWindows, collapsedWindows);
            bool secondCollapsed = subtreeCollapsed(current->children[1], visibleWindows, collapsedWindows);

            if(collapsedExtent > 0.f && firstCollapsed != secondCollapsed)
            {
                first.height = firstCollapsed ? std::min(collapsedExtent, content) : std::max(0.f, content - collapsedExtent);
            }
            else if(collapsedExtent > 0.f && firstCollapsed == true && secondCollapsed == true)
            {
                first.height = std::min(collapsedExtent, content * 0.5f);
            }
            else
            {
                first.height = content * current->ratio;
            }

            second.y = first.bottom() + separator;
            second.height = std::max(0.f, bounds.bottom() - second.y);

            if(collapsedExtent > 0.f && firstCollapsed == true && secondCollapsed == true)
            {
                second.height = std::min(collapsedExtent, std::max(0.f, bounds.height - first.height - separator));
                second.y = bounds.bottom() - second.height;
            }
        }

        if(splitters != nullptr)
        {
            Rect splitter = current->orientation == Orientation::Horizontal ? Rect{first.right(), bounds.y, separator, bounds.height} : Rect{bounds.x, first.bottom(), bounds.width, separator};
            splitters->push_back({current->id, bounds, splitter, current->orientation});
        }

        layoutVisibleNode(current->children[0], first, visibleWindows, collapsedWindows, collapsedExtent, splitterExtent, windows, splitters);
        layoutVisibleNode(current->children[1], second, visibleWindows, collapsedWindows, collapsedExtent, splitterExtent, windows, splitters);
    }

    //////////////////////////////////////////////////////////////////////////
    void DockModel::layoutNode(DockNodeId id, const Rect & bounds, DockLayoutEntryVector & output) const
    {
        const DockNode * current = node(id);

        if(current == nullptr)
        {
            return;
        }

        if(current->type == DockNodeType::Tabs)
        {
            for(Id window : current->tabs)
            {
                output.push_back({window, id, bounds, window == current->activeTab});
            }

            return;
        }

        Rect first = bounds;
        Rect second = bounds;

        if(current->orientation == Orientation::Horizontal)
        {
            first.width = bounds.width * current->ratio;
            second.x = first.right();
            second.width = bounds.width - first.width;
        }
        else
        {
            first.height = bounds.height * current->ratio;
            second.y = first.bottom();
            second.height = bounds.height - first.height;
        }

        layoutNode(current->children[0], first, output);
        layoutNode(current->children[1], second, output);
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
