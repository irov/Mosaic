#include "Context.hpp"
#include "ContextDetail.hpp"

#include <algorithm>

namespace Mosaic
{
    //////////////////////////////////////////////////////////////////////////
    Canvas dockSpace(Context * ui, StringView label, const DockSpaceOptions & options, const LayoutOptions & layout, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::dockSpace(ui, {}, label, options, layout, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Canvas dockSpace(Context * ui, const Key & key, StringView label, const DockSpaceOptions & options, const LayoutOptions & layout, const SourceLocation & location)
    {
        DockSpaceOptions resolved = options;
        resolved.group = std::max(1U, resolved.group);
        ui->dockSpaceOptions[resolved.group] = resolved;

        Canvas result = Mosaic::canvas(ui, key, label, layout, location);
        Rect bounds;
        if(Mosaic::debugBounds(ui, result.id(), &bounds) == true && bounds.empty() == false)
        {
            Mosaic::setDockArea(ui, resolved.group, bounds);
        }

        if(resolved.background == true && resolved.passthroughCentral == false)
        {
            Color background = resolved.backgroundColor;

            if(background.a <= 0.f)
            {
                background = Mosaic::getTheme(ui).colors.dockingEmptyBackground;
            }

            Rect content;
            if(result.contentRect(&content) == true)
            {
                result.rect(content, background);
            }
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    void clearDockSpace(Context * ui, uint32_t group) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(group == 0)
        {
            return;
        }

        Mosaic::docking(ui, group).clear();
    }
    //////////////////////////////////////////////////////////////////////////
    DockNodeId dockSpaceRoot(const Context * ui, uint32_t group) noexcept
    {
        if(ui == nullptr)
        {
            return 0;
        }

        if(group == 0)
        {
            return 0;
        }

        auto returnedValue = Mosaic::docking(ui, group).root();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DockNodeId dockSpaceCentralNode(const Context * ui, uint32_t group) noexcept
    {
        if(ui == nullptr)
        {
            return 0;
        }

        if(group == 0)
        {
            return 0;
        }

        auto returnedValue = Mosaic::docking(ui, group).centralNode();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DockNodeId dockNodeForWindow(const Context * ui, uint32_t group, Id window) noexcept
    {
        if(ui == nullptr)
        {
            return 0;
        }

        if(group == 0)
        {
            return 0;
        }

        if(window == InvalidId)
        {
            return 0;
        }

        auto returnedValue = Mosaic::docking(ui, group).nodeForWindow(window);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool dockSpaceSetCentralNode(Context * ui, uint32_t group, DockNodeId node) noexcept
    {
        auto returnedValue = ui != nullptr && group != 0 && Mosaic::docking(ui, group).setCentralNode(node);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool dockWindow(Context * ui, uint32_t group, Id window, DockNodeId target, DockPlacement placement, float ratio)
    {
        auto returnedValue = ui != nullptr && group != 0 && Mosaic::docking(ui, group).dock(window, target, placement, ratio);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool undockWindow(Context * ui, uint32_t group, Id window) noexcept
    {
        auto returnedValue = ui != nullptr && group != 0 && window != InvalidId && Mosaic::docking(ui, group).undock(window);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool activateDockWindow(Context * ui, uint32_t group, Id window) noexcept
    {
        auto returnedValue = ui != nullptr && group != 0 && window != InvalidId && Mosaic::docking(ui, group).activate(window);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
