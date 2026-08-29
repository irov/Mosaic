#include "Popup.hpp"
#include "Interaction.hpp"
#include "Utility.hpp"
#include "Window.hpp"

#include <algorithm>
#include <utility>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Vec2 tooltipMaximumSize(const Context * ui, const Vec2 & requested) noexcept
        {
            Rect available = ui->viewport.workArea.empty() == true ? ui->viewport.bounds : ui->viewport.workArea;
            float width = requested.x > 0.f ? std::min(requested.x, available.width) : available.width;
            float height = requested.y > 0.f ? std::min(requested.y, available.height) : available.height;
            Vec2 result = {width, height};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool itemTooltipReady(Context * ui, const Response & item, const ItemTooltipOptions & options) noexcept
        {
            if(ui == nullptr)
            {
                return false;
            }

            if(item.id == InvalidId)
            {
                return false;
            }

            const Context::Persistent & state = ui->state(item.id);
            bool navigationFocused = options.navigationFocus;
            navigationFocused = navigationFocused == true && ui->currentStyle->behavior.tooltipNavigationFocus == true;
            navigationFocused = navigationFocused == true && ui->navigationFocused == item.id;
            bool hovered = item.hovered();
            hovered = hovered == true || navigationFocused == true;

            if(options.allowWhenDisabled == true && item.disabled() == true && state.hoverLastFrame == ui->frame.number)
            {
                hovered = true;
            }

            if(hovered == false)
            {
                return false;
            }

            bool stationary = options.stationary;
            float policyDelay = ui->currentStyle->behavior.tooltipShortDelay;
            TooltipDelay delayPolicy = options.delayPolicy;

            if(delayPolicy == TooltipDelay::Default)
            {
                delayPolicy = ui->currentStyle->behavior.tooltipMouseDelay;
                stationary = stationary == true || ui->currentStyle->behavior.tooltipMouseStationary == true;
            }

            switch(delayPolicy)
            {
            case TooltipDelay::Default:
                policyDelay = ui->currentStyle->behavior.tooltipShortDelay;
                break;
            case TooltipDelay::Short:
                policyDelay = ui->currentStyle->behavior.tooltipShortDelay;
                break;
            case TooltipDelay::None:
                policyDelay = 0.f;
                break;
            case TooltipDelay::Normal:
                policyDelay = ui->currentStyle->behavior.tooltipNormalDelay;
                break;
            }

            float delay = options.delay < 0.f ? policyDelay : options.delay;
            double hoverDuration = state.hoverDuration;

            if(navigationFocused == true)
            {
                hoverDuration = 0.0;

                if(state.focusVisual >= 0.999f)
                {
                    hoverDuration = static_cast<double>(std::max(0.f, delay));
                }
            }

            bool sharedReady = options.sharedDelay;
            sharedReady = sharedReady == true && ui->currentStyle->behavior.tooltipMouseSharedDelay == true;
            sharedReady = sharedReady == true && ui->sharedTooltipSource != InvalidId;
            sharedReady = sharedReady == true && ui->sharedTooltipSource != item.id;
            sharedReady = sharedReady == true && ui->input.timestamp <= ui->sharedTooltipVisibleUntil;
            bool stationaryReady = stationary == false || navigationFocused == true;

            if(stationaryReady == false)
            {
                stationaryReady = state.stationaryHoverDuration >= static_cast<double>(std::max(0.f, ui->currentStyle->behavior.tooltipStationaryDelay));
            }

            bool ready = stationaryReady == true && (sharedReady == true || hoverDuration >= static_cast<double>(std::max(0.f, delay)));

            if(ready == true)
            {
                ui->sharedTooltipSource = item.id;
                ui->sharedTooltipVisibleUntil = ui->input.timestamp + 0.20;
            }

            return ready;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float triangleSign(const Vec2 & point, const Vec2 & first, const Vec2 & second) noexcept
        {
            auto returnedValue = (point.x - second.x) * (first.y - second.y) - (first.x - second.x) * (point.y - second.y);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool pointInTriangle(const Vec2 & point, const Vec2 & first, const Vec2 & second, const Vec2 & third) noexcept
        {
            bool firstSide = Detail::triangleSign(point, first, second) < 0.f;
            bool secondSide = Detail::triangleSign(point, second, third) < 0.f;
            bool thirdSide = Detail::triangleSign(point, third, first) < 0.f;

            return firstSide == secondSide && secondSide == thirdSide;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool menuAimHoldsCurrentPopup(const Context * ui) noexcept
        {
            if(ui->popupStack.size() < 2)
            {
                return false;
            }

            const PointerState * pointer = ui->input.primaryPointer();
            const Context::PopupState & popup = ui->popupStack.back();

            if(pointer == nullptr)
            {
                return false;
            }

            if(popup.bounds.empty() == true)
            {
                return false;
            }

            if((pointer->delta.x == 0.f && pointer->delta.y == 0.f))
            {
                return false;
            }

            Vec2 previous = pointer->position - pointer->delta;
            bool opensRight = popup.bounds.x >= previous.x;
            float edge = opensRight ? popup.bounds.x : popup.bounds.right();
            Vec2 top = {edge, popup.bounds.y - 6.f};
            Vec2 bottom = {edge, popup.bounds.bottom() + 6.f};
            auto returnedValue = Detail::pointInTriangle(pointer->position, previous, top, bottom);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Scope menuBar(Context * ui, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::menuBar(ui, MenuBarOptions{}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope menuBar(Context * ui, const MenuBarOptions & menuBarOptions, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return {};
        }

        if(ui->currentWindow != InvalidId)
        {
            Context::Node * windowNode = ui->findFrameNode(ui->currentWindow);

            if(windowNode != nullptr)
            {
                windowNode->mutableWindowData().menuBar = true;
            }
        }

        LayoutOptions layout;
        layout.width = menuBarOptions.width;
        layout.orientation = Orientation::Horizontal;
        layout.gap = 0.f;
        size_t node = ui->addNode(Detail::NodeKind::Row, {}, {}, layout, location, SemanticRole::Group, false, true);
        ui->nodes[node].menuBar = true;
        ui->nodes[node].fillBackground = menuBarOptions.fillBackground;
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    MainMenuBarScope mainMenuBar(Context * ui, const MenuBarOptions & options, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return {};
        }

        if(ui->mainMenuBarSubmitted == true)
        {
            return {};
        }

        Viewport viewport;

        if(Mosaic::currentViewport(ui, &viewport) == false)
        {
            return {};
        }

        Rect workArea = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
        float menuHeight = ui->currentStyle->metrics.controlHeight + ui->currentStyle->metrics.framePadding.top + ui->currentStyle->metrics.framePadding.bottom;
        WindowOptions windowOptions;
        windowOptions.initialBounds = {workArea.x, workArea.y, workArea.width, menuHeight};
        windowOptions.minimumSize = {1.f, menuHeight};
        windowOptions.maximumSize = {workArea.width, menuHeight};
        windowOptions.titleBar = false;
        windowOptions.movable = false;
        windowOptions.resizable = false;
        windowOptions.dockable = false;
        windowOptions.bringToFront = false;
        windowOptions.saveSettings = false;
        WindowScope window = Mosaic::window(ui, Key("Main menu bar"), {}, windowOptions, location);
        Mosaic::setWindowBounds(ui, window.id(), windowOptions.initialBounds);
        Context::Node * windowNode = ui->findFrameNode(window.id());

        if(windowNode != nullptr)
        {
            Context::Persistent & persistentState = ui->state(*windowNode);
            persistentState.windowData().zOrder = ui->nextWindowZOrder++;
            windowNode->mutableWindowData().zOrder = persistentState.windowData().zOrder;
        }

        ui->mainMenuBarSubmitted = true;
        ui->viewport.workArea = workArea;
        ui->viewport.workArea.y += menuHeight;
        ui->viewport.workArea.height = std::max(0.f, ui->viewport.workArea.height - menuHeight);

        if(window.visible() == false)
        {
            return {std::move(window), {}};
        }

        Scope menuBar = Mosaic::menuBar(ui, options, location);

        return {std::move(window), std::move(menuBar)};
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope menu(Context * ui, StringView label, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::menu(ui, label, MenuOptions{}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope menu(Context * ui, StringView label, const MenuOptions & menuOptions, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::menu(ui, {}, label, menuOptions, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope menu(Context * ui, const Key & key, StringView label, const MenuOptions & menuOptions, const SourceLocation & location)
    {
        LayoutOptions layout;

        if(menuOptions.width > 0.f)
        {
            layout.width = Dimension::fixed(menuOptions.width);
        }

        Id identityParent = ui->nodes[ui->currentParent].identityScope;
        Id ownerId = combineId(identityParent, ui->localId(Detail::NodeKind::Button, key, location, false));
        size_t existingOwner = ui->findFrameNodeIndex(ownerId);
        bool append = existingOwner < ui->nodes.size() && ui->nodes[existingOwner].parent == ui->currentParent && ui->nodes[existingOwner].kind == Detail::NodeKind::Button && ui->nodes[existingOwner].semanticRole == SemanticRole::MenuItem;
        size_t menuNode = existingOwner;
        Response owner;

        if(append == true)
        {
            owner = ui->nodes[menuNode].response;
        }
        else
        {
            menuNode = ui->addNode(Detail::NodeKind::Button, key, label, layout, location, SemanticRole::MenuItem, true);
            ui->nodes[menuNode].fillBackground = menuOptions.fillBackground;
            ui->nodes[menuNode].menuPopupItem = ui->currentInputLayer != InvalidId;
            ui->nodes[menuNode].menuSubmenu = ui->currentInputLayer != InvalidId;
            owner = ui->interact(menuNode);
            ui->nodes[menuNode].response = owner;
        }

        Key popupKey("Menu popup");
        PopupOptions options;
        options.owner = owner.id;

        if(Mosaic::debugBounds(ui, owner.id, &options.anchor) == false)
        {
            return {};
        }

        options.placement = ui->currentInputLayer == InvalidId ? PopupPlacement::Below : PopupPlacement::Right;
        options.minimumSize = {ui->currentStyle->metrics.minimumPopupWidth, 0.f};
        options.maximumSize = {420.f, 520.f};
        options.allowSiblingOwners = true;
        options.closeOnSelection = false;
        bool nested = ui->currentInputLayer != InvalidId;

        if(nested == false)
        {
            ui->menuBarItems.push_back(owner.id);
        }

        bool keyboardOpen = owner.focused() == true && ((nested == false && ui->input.keyPressed(KeyCode::Down) == true) || (nested == true && ui->input.keyPressed(KeyCode::Right) == true));
        bool keyboardSiblingOpen = nested == false && owner.focused() == true && ui->popupStack.empty() == false && (ui->input.keyPressed(KeyCode::Left) == true || ui->input.keyPressed(KeyCode::Right) == true);
        const Context::PopupState * state = Detail::findPopup(ui, Detail::popupId(ui, popupKey, owner.id));
        bool popupActionHandled = false;

        if(owner.clicked() == true || keyboardOpen == true)
        {
            popupActionHandled = true;

            if(state != nullptr && state->open == true)
            {
                Mosaic::closeCurrentPopup(ui);
            }
            else
            {
                Mosaic::openPopup(ui, popupKey, options);
            }
        }
        else if(keyboardSiblingOpen == true && (state == nullptr || state->open == false))
        {
            popupActionHandled = true;
            Mosaic::openPopup(ui, popupKey, options);
        }

        const Context::PopupState * lastPopup = ui->popupStack.empty() == true ? nullptr : &ui->popupStack.back();
        bool openHoveredSibling = owner.hovered() == true && lastPopup != nullptr && lastPopup->open == true && lastPopup->options.allowSiblingOwners == true && lastPopup->owner != owner.id;
        bool nestedAimAllowsOpen = nested == false || (ui->state(owner.id).hoverDuration >= 0.08 && Detail::menuAimHoldsCurrentPopup(ui) == false);

        if(popupActionHandled == false && openHoveredSibling == true && nestedAimAllowsOpen == true)
        {
            Mosaic::openPopup(ui, popupKey, options);
        }

        const Context::PopupState * activeState = Detail::findPopup(ui, Detail::popupId(ui, popupKey, owner.id));
        lastPopup = ui->popupStack.empty() == true ? nullptr : &ui->popupStack.back();
        bool closeNestedPopup = nested;

        if(activeState == nullptr)
        {
            closeNestedPopup = false;
        }

        if(closeNestedPopup == true)
        {
            if(activeState->open == false)
            {
                closeNestedPopup = false;
            }
        }

        if(ui->input.keyPressed(KeyCode::Left) == false)
        {
            closeNestedPopup = false;
        }

        if(lastPopup == nullptr)
        {
            closeNestedPopup = false;
        }

        if(closeNestedPopup == true)
        {
            if(lastPopup->id != activeState->id)
            {
                closeNestedPopup = false;
            }
        }

        if(closeNestedPopup == true)
        {
            Mosaic::closeCurrentPopup(ui);
            activeState = nullptr;
        }

        if(Context::Node * ownerNode = ui->findFrameNode(owner.id); ownerNode != nullptr)
        {
            ownerNode->selected = activeState != nullptr && activeState->open;
        }

        WindowScope popupScope = Mosaic::popup(ui, popupKey, options, location);
        auto returnedValue = TreeScope(std::move(popupScope));

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response menuItem(Context * ui, StringView label, bool enabled, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::menuItem(ui, {}, label, enabled, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response menuItem(Context * ui, const Key & key, StringView label, bool enabled, const SourceLocation & location)
    {
        MenuItemOptions options;
        options.enabled = enabled;
        auto returnedValue = Mosaic::menuItem(ui, key, label, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response menuItem(Context * ui, StringView label, const MenuItemOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::menuItem(ui, {}, label, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response menuItem(Context * ui, const Key & key, StringView label, const MenuItemOptions & options, const SourceLocation & location)
    {
        Response response;

        if(options.enabled == true)
        {
            response = Mosaic::selectable(ui, key, label, options.selected || (options.checked != nullptr && *options.checked), location);
        }
        else
        {
            auto disabled = Mosaic::disabledScope(ui, true, location);
            response = Mosaic::selectable(ui, key, label, options.selected, location);
        }

        if(Context::Node * node = ui->findFrameNode(response.id); node != nullptr)
        {
            node->semanticRole = SemanticRole::MenuItem;
            node->menuPopupItem = ui->currentInputLayer != InvalidId;
            node->menuCheckVisible = options.checked != nullptr || options.selected;
            node->checked = (options.checked != nullptr && *options.checked) || options.selected;
            node->valueText.assign(options.shortcut);
            node->valueData().valueTextSize = ui->estimateText(node->valueText, *node->style);
            node->valueTextPrepared = node->valueText.empty() == true;
        }

        if(response.clicked() == true && options.checked != nullptr)
        {
            *options.checked = !*options.checked;
            Detail::setFlag(response, 6);
        }

        if(response.clicked() == true && options.closeOnActivate == true)
        {
            Mosaic::closeCurrentPopup(ui);
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope popup(Context * ui, StringView label, bool * open, const Rect & bounds, const SourceLocation & location)
    {
        Rect popupBounds = bounds;

        if(popupBounds.empty() == true)
        {
            Vec2 pointer;
            if(Mosaic::pointerPosition(ui, &pointer) == false)
            {
                return {};
            }

            popupBounds = {pointer.x, pointer.y, 280.f, 180.f};
        }

        Id popupLayer = combineId(RootId, ui->localId(Detail::NodeKind::Window, {}, location, false));
        Id previousInputLayer = ui->currentInputLayer;
        ui->blockingInputLayer = popupLayer;
        ui->currentInputLayer = popupLayer;

        if(open == nullptr || *open == true)
        {
            Detail::popupBackdrop(ui, label, open, true, Color{0.f, 0.f, 0.f, 0.f}, popupBounds, location);
        }

        ui->currentInputLayer = previousInputLayer;

        if(open != nullptr && *open == false)
        {
            return {};
        }

        WindowOptions options;
        options.open = open;
        options.initialBounds = popupBounds;
        options.movable = false;
        options.resizable = false;
        options.input = false;
        options.dockable = false;
        options.titleBar = false;
        WindowScope result = Mosaic::window(ui, {}, label, options, location);
        Context::Node & popupNode = ui->nodes.back();
        popupNode.inputLayer = popupLayer;

        if(ui->scopes.empty() == false)
        {
            Context::ScopeState & scope = ui->scopes.back();
            scope.previousInputLayer = previousInputLayer;
        }

        ui->currentInputLayer = popupLayer;
        Detail::makePopupOpaque(ui, popupNode);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope modal(Context * ui, StringView label, bool * open, const Vec2 & size, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::modal(ui, {}, label, open, size, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope modal(Context * ui, const Key & key, StringView label, bool * open, const Vec2 & size, const SourceLocation & location)
    {
        Key resolvedKey = key.isExplicit() ? key : Key(ui->localId(Detail::NodeKind::Window, {}, location, false));
        PopupOptions options;
        options.owner = ui->nodes[ui->currentParent].identityScope;
        options.anchor = ui->viewport.workArea.empty() == true ? ui->viewport.bounds : ui->viewport.workArea;
        options.placement = PopupPlacement::Center;
        Rect available = ui->viewport.workArea.empty() == true ? ui->viewport.bounds : ui->viewport.workArea;
        options.minimumSize = {size.x > 0.f ? size.x : 0.f, size.y > 0.f ? size.y : 0.f};
        options.maximumSize = {size.x > 0.f ? size.x : available.width, size.y > 0.f ? size.y : available.height};
        options.closeOnClickOutside = false;
        options.closeOnSelection = false;
        options.modal = true;
        options.titleBar = true;
        options.backdropColor = ui->currentStyle->colors.modalDimBackground;
        Id popupId = Detail::popupId(ui, resolvedKey, options.owner);

        if(ui->popupClosedThisFrame.contains(popupId) == true)
        {
            if(open != nullptr)
            {
                *open = false;
            }

            return {};
        }

        if(open != nullptr && *open == false)
        {
            Mosaic::closePopup(ui, resolvedKey, options.owner);

            return {};
        }

        if(Mosaic::isPopupOpen(ui, resolvedKey, options.owner) == false)
        {
            Mosaic::openPopup(ui, resolvedKey, options);
        }

        WindowScope result = Mosaic::popup(ui, resolvedKey, label, options, location);

        if(result.id() != InvalidId && (size.x <= 0.f || size.y <= 0.f))
        {
            Context::Node & modalNode = ui->nodes[ui->currentParent];
            modalNode.mutableWindowData().autoSize = true;
            modalNode.mutableWindowData().fitContentWidth = size.x <= 0.f;
            modalNode.mutableWindowData().fitContentHeight = size.y <= 0.f;

            if(size.x <= 0.f)
            {
                modalNode.layout.width = SizeRule::Content;
            }

            if(size.y <= 0.f)
            {
                modalNode.layout.height = SizeRule::Content;
            }
        }

        if(open != nullptr && Mosaic::isPopupOpen(ui, resolvedKey, options.owner) == false)
        {
            *open = false;
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope tooltip(Context * ui, StringView label, const Vec2 & maximumSize, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::tooltip(ui, {}, label, {}, maximumSize, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope tooltip(Context * ui, const Key & key, StringView label, const Vec2 & maximumSize, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::tooltip(ui, key, label, {}, maximumSize, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope tooltip(Context * ui, StringView label, const Vec2 & minimumSize, const Vec2 & maximumSize, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::tooltip(ui, {}, label, minimumSize, maximumSize, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope tooltip(Context * ui, const Key & key, StringView label, const Vec2 & minimumSize, const Vec2 & maximumSize, const SourceLocation & location)
    {
        Vec2 resolvedMaximumSize = Detail::tooltipMaximumSize(ui, maximumSize);
        Vec2 resolvedMinimumSize;
        resolvedMinimumSize.x = std::clamp(minimumSize.x, 0.f, resolvedMaximumSize.x);
        resolvedMinimumSize.y = std::clamp(minimumSize.y, 0.f, resolvedMaximumSize.y);
        size_t previousParent = ui->currentParent;
        ui->currentParent = 0;
        WindowOptions windowOptions;
        windowOptions.dockable = false;
        windowOptions.movable = false;
        windowOptions.resizable = false;
        windowOptions.titleBar = false;
        windowOptions.minimumSize = resolvedMinimumSize;
        windowOptions.maximumSize = resolvedMaximumSize;
        Vec2 pointer;
        if(Mosaic::pointerPosition(ui, &pointer) == false)
        {
            ui->currentParent = previousParent;

            return {};
        }

        float initialWidth = std::max(ui->currentStyle->metrics.minimumPopupWidth, resolvedMinimumSize.x);
        initialWidth = std::min(initialWidth, resolvedMaximumSize.x);
        float initialHeight = std::max(ui->currentStyle->metrics.controlHeight, resolvedMinimumSize.y);
        initialHeight = std::min(initialHeight, resolvedMaximumSize.y);
        windowOptions.initialBounds = {pointer.x + 12.f, pointer.y + 18.f, initialWidth, initialHeight};
        WindowScope result = Mosaic::window(ui, key, label, windowOptions, location);

        if(result.id() != InvalidId)
        {
            Context::Node & tooltipNode = ui->nodes[ui->currentParent];
            tooltipNode.mutableWindowData().autoSize = true;
            tooltipNode.mutableWindowData().popup = true;
            Context::Persistent & tooltipState = ui->state(tooltipNode);
            tooltipState.windowData().popup = true;
            tooltipState.windowData().zOrder = ui->nextWindowZOrder++;
            tooltipNode.mutableWindowData().zOrder = tooltipState.windowData().zOrder;
            tooltipNode.mutableWindowData().minimumSize = resolvedMinimumSize;
            tooltipNode.mutableWindowData().maximumSize = resolvedMaximumSize;
            ui->currentInputBlocked = true;
            ui->currentNavigationBlocked = true;
            tooltipNode.layout.width = SizeRule::Content;
            tooltipNode.layout.height = SizeRule::Content;
            tooltipNode.layout.minimum = resolvedMinimumSize;
            tooltipNode.layout.maximum = resolvedMaximumSize;
            tooltipNode.mutableWindowData().popupAnchor = {pointer.x + 12.f, pointer.y + 18.f, 1.f, 1.f};
            tooltipNode.mutableWindowData().popupPlacement = PopupPlacement::Cursor;
            Theme & style = ui->mutableStyle(tooltipNode);
            style.colors.panel = style.colors.popup;
            style.colors.background = style.colors.popup;
            style.colors.border = style.colors.popupBorder;
            style.colors.borderStrong = style.colors.popupBorder;

            if(ui->scopes.empty() == false)
            {
                Context::ScopeState & scope = ui->scopes.back();
                scope.previousParent = previousParent;
            }
        }
        else
        {
            ui->currentParent = previousParent;
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    void itemTooltip(Context * ui, const Response & item, StringView description, const Vec2 & size, const SourceLocation & location)
    {
        Mosaic::itemTooltip(ui, item, description, size, -1.f, location);
    }
    //////////////////////////////////////////////////////////////////////////
    void itemTooltip(Context * ui, const Response & item, StringView description, const Vec2 & size, float delay, const SourceLocation & location)
    {
        ItemTooltipOptions options;
        options.maximumSize = size;
        options.delay = delay;
        Mosaic::itemTooltip(ui, item, description, options, location);
    }
    //////////////////////////////////////////////////////////////////////////
    void itemTooltip(Context * ui, const Response & item, StringView description, const ItemTooltipOptions & options, const SourceLocation & location)
    {
        if(Detail::itemTooltipReady(ui, item, options) == false)
        {
            return;
        }

        String tooltipName = "Item tooltip ";
        String itemId;
        if(Detail::toString(item.id, &itemId) == false)
        {
            return;
        }

        tooltipName += itemId;
        size_t previousParent = ui->currentParent;
        ui->currentParent = 0;
        {
            auto blocked = Mosaic::interactionScope(ui, false, location);
            auto itemTooltip = Mosaic::tooltip(ui, Key(item.id), tooltipName, options.minimumSize, options.maximumSize, location);

            if(itemTooltip.visible() == true)
            {
                TextOptions textOptions;
                textOptions.wordWrap = true;
                Mosaic::text(ui, description, textOptions, location);
            }
        }
        ui->currentParent = previousParent;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope itemTooltip(Context * ui, const Response & item, const Key & key, const ItemTooltipOptions & options, const SourceLocation & location)
    {
        if(Detail::itemTooltipReady(ui, item, options) == false)
        {
            return {};
        }

        String tooltipName = "Item tooltip ";
        String itemId;
        if(Detail::toString(item.id, &itemId) == false)
        {
            return {};
        }

        String keyId;
        if(Detail::toString(key.value(), &keyId) == false)
        {
            return {};
        }

        tooltipName += itemId;
        tooltipName += " ";
        tooltipName += keyId;
        auto returnedValue = Mosaic::tooltip(ui, Key(combineId(item.id, key.value())), tooltipName, options.minimumSize, options.maximumSize, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response helpMarker(Context * ui, StringView description, const HelpMarkerOptions & options, const SourceLocation & location)
    {
        StringView marker = options.marker.empty() == true ? StringView("(?)") : options.marker;
        size_t node = ui->addNode(Detail::NodeKind::Button, {}, marker, {}, location, SemanticRole::None, false);
        Context::Node & markerNode = ui->nodes[node];
        markerNode.fillBackground = false;
        markerNode.fillHoverBackground = false;
        Theme & style = ui->mutableStyle(markerNode);
        style.colors.text = style.colors.textDisabled;
        Response response = ui->interact(node, false);
        markerNode.response = response;
        ItemTooltipOptions tooltipOptions;
        tooltipOptions.minimumSize = options.tooltipMinimumSize;
        tooltipOptions.maximumSize = options.tooltipSize;
        tooltipOptions.delay = options.tooltipDelay;
        Mosaic::itemTooltip(ui, response, description, tooltipOptions, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
