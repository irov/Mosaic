#pragma once

#include "Mosaic/Types.hpp"

#include <stdint.h>

namespace Mosaic
{
    using DockNodeId = uint64_t;
    using DockNodeIdPair = Array<DockNodeId, 2>;
    using DockNodeIdVector = Vector<DockNodeId>;
    using DockNodeIdSet = UnorderedSet<DockNodeId>;

    enum class DockNodeType : uint8_t
    {
        Tabs,
        Split
    };

    enum class DockPlacement : uint8_t
    {
        Center,
        Left,
        Right,
        Top,
        Bottom
    };

    struct DockNode
    {
        DockNodeId id = 0;
        DockNodeId parent = 0;
        DockNodeType type = DockNodeType::Tabs;
        Orientation orientation = Orientation::Horizontal;
        float ratio = 0.5f;
        DockNodeIdPair children = {};
        IdVector tabs;
        Id activeTab = InvalidId;
        bool central = false;
    };

    using DockNodeVector = Vector<DockNode>;
    using DockNodeSpan = Span<const DockNode>;
    using DockNodeIndexMap = UnorderedMap<DockNodeId, size_t>;
    using DockWindowNodeMap = UnorderedMap<Id, DockNodeId>;

    struct DockLayoutEntry
    {
        Id window = InvalidId;
        DockNodeId node = 0;
        Rect bounds;
        bool active = false;
        uint32_t group = 1;
    };

    using DockLayoutEntryVector = Vector<DockLayoutEntry>;

    struct DockSplitterLayoutEntry
    {
        DockNodeId node = 0;
        Rect parentBounds;
        Rect bounds;
        Orientation orientation = Orientation::Horizontal;
        uint32_t group = 1;
    };

    using DockSplitterLayoutEntryVector = Vector<DockSplitterLayoutEntry>;

    class DockModel
    {
    public:
        DockModel();

        void clear();
        [[nodiscard]] DockNodeId root() const noexcept;
        [[nodiscard]] DockNodeId centralNode() const noexcept;
        [[nodiscard]] const DockNode * node(DockNodeId id) const noexcept;
        [[nodiscard]] DockNode * node(DockNodeId id) noexcept;
        [[nodiscard]] DockNodeId nodeForWindow(Id window) const noexcept;
        [[nodiscard]] DockNodeSpan nodes() const noexcept;

        bool dock(Id window, DockNodeId target, DockPlacement placement = DockPlacement::Center, float ratio = 0.5f);
        bool undock(Id window);
        bool activate(Id window);
        bool reorder(Id window, size_t newIndex);
        bool setSplitRatio(DockNodeId split, float ratio);
        bool setCentralNode(DockNodeId node);
        bool restore(DockNodeSpan nodes, DockNodeId root);
        [[nodiscard]] bool validate() const noexcept;
        [[nodiscard]] DockLayoutEntryVector layout(const Rect & bounds) const;
        [[nodiscard]] DockLayoutEntryVector layout(const Rect & bounds, IdSpan visibleWindows, IdSpan collapsedWindows = {}, float collapsedExtent = 0.f, float splitterExtent = 4.f) const;
        [[nodiscard]] DockSplitterLayoutEntryVector splitterLayout(const Rect & bounds, IdSpan visibleWindows = {}, IdSpan collapsedWindows = {}, float collapsedExtent = 0.f, float splitterExtent = 4.f) const;
        void layout(const Rect & bounds, IdSpan visibleWindows, IdSpan collapsedWindows, float collapsedExtent, float splitterExtent, DockLayoutEntryVector & windows, DockSplitterLayoutEntryVector & splitters) const;

    private:
        [[nodiscard]] DockNodeId createNode(DockNodeType type);
        void removeNode(DockNodeId id);
        void rebuildIndices();
        void collapseEmpty(DockNodeId id);
        void layoutNode(DockNodeId id, const Rect & bounds, DockLayoutEntryVector & output) const;
        [[nodiscard]] bool subtreeVisible(DockNodeId id, IdSpan visibleWindows) const noexcept;
        [[nodiscard]] bool subtreeCollapsed(DockNodeId id, IdSpan visibleWindows, IdSpan collapsedWindows) const noexcept;
        void layoutVisibleNode(DockNodeId id, const Rect & bounds, IdSpan visibleWindows, IdSpan collapsedWindows, float collapsedExtent, float splitterExtent, DockLayoutEntryVector * windows, DockSplitterLayoutEntryVector * splitters) const;

    private:
        DockNodeId m_root = 0;
        DockNodeId m_central = 0;
        DockNodeId m_nextId = 1;
        DockNodeVector m_nodes;
        DockNodeIndexMap m_nodeIndices;
        DockWindowNodeMap m_windowNodes;
    };
} // namespace Mosaic
