#include "Popup.hpp"

#include <algorithm>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        void closePopupRange(Context * ui, size_t first, bool outsidePointer, bool allowReplacement)
        {
            if(first >= ui->popupStack.size())
            {
                return;
            }

            Id restoreFocus = ui->popupStack[first].restoreFocus;
            const Context::PopupState & lastPopup = ui->popupStack.back();
            Id closedInputLayer = lastPopup.id;
            for(size_t index = ui->popupStack.size(); index != first; --index)
            {
                Context::PopupState & state = ui->popupStack[index - 1];
                state.open = false;
                ui->popupClosedThisFrame.insert(state.id);
                ui->frame.events.push_back({EventType::PopupClose, state.id, "Popup", {}, 0, ui->input.timestamp});
            }
            ui->popupStack.erase(ui->popupStack.begin() + static_cast<std::ptrdiff_t>(first), ui->popupStack.end());
            ui->focused = restoreFocus;
            ui->navigationFocused = restoreFocus;
            ui->popupReplacementAllowed = allowReplacement;
            ui->popupClosedByOutsidePointer = outsidePointer;

            if(ui->popupStack.empty() == true)
            {
                ui->previousBlockingInputLayer = closedInputLayer;
                ui->blockingInputLayer = InvalidId;
            }
            else
            {
                const Context::PopupState & remainingPopup = ui->popupStack.back();
                ui->blockingInputLayer = remainingPopup.id;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        Context::PopupState * findPopup(Context * ui, Id id) noexcept
        {
            auto iterator = std::find_if(ui->popupStack.begin(), ui->popupStack.end(),
                                               [id](const Context::PopupState & popup)
                                               {
                                                   return popup.id == id;
                                               });
            auto returnedValue = iterator == ui->popupStack.end() ? nullptr : &*iterator;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        const Context::PopupState * findPopup(const Context * ui, Id id) noexcept
        {
            auto iterator = std::find_if(ui->popupStack.begin(), ui->popupStack.end(),
                                               [id](const Context::PopupState & popup)
                                               {
                                                   return popup.id == id;
                                               });
            auto returnedValue = iterator == ui->popupStack.end() ? nullptr : &*iterator;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        void closePopupById(Context * ui, Id id) noexcept
        {
            auto iterator = std::find_if(ui->popupStack.begin(), ui->popupStack.end(),
                                               [id](const Context::PopupState & popup)
                                               {
                                                   return popup.id == id;
                                               });

            if(iterator == ui->popupStack.end())
            {
                return;
            }

            Detail::closePopupRange(ui, static_cast<size_t>(std::distance(ui->popupStack.begin(), iterator)), false, false);
        }
        //////////////////////////////////////////////////////////////////////////
        Id popupId(const Context * ui, const Key & key, Id owner) noexcept
        {
            Id scope = owner == InvalidId ? ui->nodes[ui->currentParent].identityScope : owner;
            auto returnedValue = combineId(scope, key.value());

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        bool popupOwnerCanInteract(const Context * ui, Id node) noexcept
        {
            for(auto iterator = ui->popupStack.rbegin(); iterator != ui->popupStack.rend(); ++iterator)
            {
                if(iterator->open == true)
                {
                    if(iterator->owner == node)
                    {
                        return true;
                    }

                    if(iterator->options.allowSiblingOwners == false)
                    {
                        return false;
                    }

                    const Context::Node * ownerNode = ui->findFrameNode(iterator->owner);
                    const Context::Node * candidateNode = ui->findFrameNode(node);

                    return ownerNode != nullptr && candidateNode != nullptr && ownerNode->parentId == candidateNode->parentId;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        Rect placePopup(const Context::Node & node, const Vec2 & size, const Rect & workArea) noexcept
        {
            Rect anchor = node.windowData().popupAnchor;

            if(anchor.empty() == true)
            {
                anchor = {workArea.x, workArea.y, 1.f, 1.f};
            }

            float availableBelow = std::max(0.f, workArea.bottom() - anchor.bottom());
            float availableAbove = std::max(0.f, anchor.y - workArea.y);
            float availableLeft = std::max(0.f, anchor.x - workArea.x);
            float availableRight = std::max(0.f, workArea.right() - anchor.right());
            PopupPlacement placement = node.windowData().popupPlacement;

            if(placement == PopupPlacement::Automatic)
            {
                placement = size.y <= availableBelow || availableBelow >= availableAbove ? PopupPlacement::Below : PopupPlacement::Above;
            }

            Rect result = {anchor.x, anchor.bottom(), std::min(size.x, workArea.width), std::min(size.y, workArea.height)};
            float horizontal = node.windowData().popupHorizontalAlignment == PopupHorizontalAlignment::End ? anchor.right() - result.width : anchor.x;
            switch(placement)
            {
            case PopupPlacement::Automatic:
            case PopupPlacement::Below:
                result.x = horizontal;
                result.y = anchor.bottom();
                result.height = std::min(result.height, availableBelow);
                break;
            case PopupPlacement::Above:
                result.x = horizontal;
                result.height = std::min(result.height, availableAbove);
                result.y = anchor.y - result.height;
                break;
            case PopupPlacement::Left:
                result.width = std::min(result.width, availableLeft);
                result.x = anchor.x - result.width;
                result.y = anchor.y;
                break;
            case PopupPlacement::Right:
                result.x = anchor.right();
                result.width = std::min(result.width, availableRight);
                result.y = anchor.y;
                break;
            case PopupPlacement::Cursor:
                result.x = anchor.x;
                result.y = anchor.y;
                break;
            case PopupPlacement::Center:
                result.x = workArea.x + (workArea.width - result.width) * 0.5f;
                result.y = workArea.y + (workArea.height - result.height) * 0.5f;
                break;
            }

            result.x = std::clamp(result.x, workArea.x, std::max(workArea.x, workArea.right() - result.width));
            result.y = std::clamp(result.y, workArea.y, std::max(workArea.y, workArea.bottom() - result.height));

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        void beginPopupFrame(Context * ui)
        {
            while(ui->popupStack.empty() == false)
            {
                const Context::PopupState & popup = ui->popupStack.back();

                if(popup.open == true)
                {
                    break;
                }

                ui->popupStack.pop_back();
            }

            if(ui->popupStack.empty() == true)
            {
                return;
            }

            const Context::PopupState & top = ui->popupStack.back();

            if(top.options.closeOnEscape == true && ui->input.keyPressed(KeyCode::Escape) == true)
            {
                Detail::closePopupRange(ui, ui->popupStack.size() - 1, false, false);

                return;
            }

            const PointerState * pointer = ui->input.primaryPointer();

            if(pointer == nullptr)
            {
                return;
            }

            if(top.options.closeOnClickOutside == false)
            {
                return;
            }

            if((pointer->isPressed(PointerButton::Primary) == false && pointer->isPressed(PointerButton::Secondary) == false))
            {
                return;
            }

            if(top.bounds.contains(pointer->position) == true)
            {
                return;
            }

            size_t containingPopup = ui->popupStack.size();
            for(size_t index = ui->popupStack.size(); index != 0; --index)
            {
                if(ui->popupStack[index - 1].bounds.contains(pointer->position) == true)
                {
                    containingPopup = index - 1;
                    break;
                }
            }
            size_t firstToClose = containingPopup == ui->popupStack.size() ? 0 : containingPopup + 1;
            Detail::closePopupRange(ui, firstToClose, true, pointer->isPressed(PointerButton::Secondary));
        }
        //////////////////////////////////////////////////////////////////////////
        void syncPopupBounds(Context * ui)
        {
            for(Context::PopupState & popup : ui->popupStack)
            {
                const Context::Persistent * persistent = ui->findState(popup.node);

                if(persistent != nullptr && persistent->lastBounds.empty() == false)
                {
                    popup.bounds = persistent->lastBounds;
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    void openPopup(Context * ui, const Key & key, const PopupOptions & options)
    {
        Id id = Detail::popupId(ui, key, options.owner);
        Id resolvedOwner = options.owner == InvalidId ? ui->focused : options.owner;
        Context::PopupState * lastPopup = ui->popupStack.empty() == true ? nullptr : &ui->popupStack.back();
        bool existingPopupBlocksOpen = options.openOverExisting == false;

        if(lastPopup == nullptr)
        {
            existingPopupBlocksOpen = false;
        }

        if(existingPopupBlocksOpen == true && lastPopup->open == false)
        {
            existingPopupBlocksOpen = false;
        }

        if(existingPopupBlocksOpen == true && lastPopup->id == id)
        {
            existingPopupBlocksOpen = false;
        }

        if(existingPopupBlocksOpen == true)
        {
            return;
        }

        if(options.openOverItems == false)
        {
            auto hoveredItem = std::find_if(ui->nodes.rbegin(), ui->nodes.rend(),
                                            [resolvedOwner](const Context::Node & node)
                                            {
                                                return node.id != resolvedOwner && node.response.hovered() == true;
                                            });

            if(hoveredItem != ui->nodes.rend())
            {
                return;
            }
        }

        Context::PopupState * existing = Detail::findPopup(ui, id);

        if(options.reopen == false && existing != nullptr && existing->open == true)
        {
            return;
        }

        if(options.allowSiblingOwners == true && lastPopup != nullptr && lastPopup->open == true && lastPopup->id != id && lastPopup->options.allowSiblingOwners == true)
        {
            EventTraceEntry event = {EventType::PopupClose, lastPopup->id, "Popup", {}, 0, ui->input.timestamp};
            ui->frame.events.push_back(event);
            ui->popupStack.pop_back();
        }

        Context::PopupState * state = Detail::findPopup(ui, id);

        if(state == nullptr)
        {
            Context::PopupState created;
            created.id = id;
            created.key = key.value();
            created.level = static_cast<uint32_t>(ui->popupStack.size());
            ui->popupStack.push_back(std::move(created));
            state = &ui->popupStack.back();
        }

        state->owner = options.owner == InvalidId ? ui->focused : options.owner;
        state->restoreFocus = ui->focused;
        state->options = options;

        if(state->options.owner == InvalidId)
        {
            state->options.owner = state->owner;
        }

        if(state->options.anchor.empty() == true)
        {
            Vec2 position = ui->input.primaryPointer() == nullptr ? Vec2{} : ui->input.primaryPointer()->position;
            state->options.anchor = {position.x, position.y, 1.f, 1.f};
        }

        state->open = true;
        state->lastFrame = ui->frame.number;
        ui->frame.events.push_back({EventType::PopupOpen, id, "Popup", {}, 0, ui->input.timestamp});
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope popup(Context * ui, const Key & key, const PopupOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::popup(ui, key, key.debug(), options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope popup(Context * ui, const Key & key, StringView label, const PopupOptions & options, const SourceLocation & location)
    {
        Id id = Detail::popupId(ui, key, options.owner);
        Context::PopupState * state = Detail::findPopup(ui, id);

        if(state == nullptr && options.owner == InvalidId)
        {
            auto iterator = std::find_if(ui->popupStack.rbegin(), ui->popupStack.rend(),
                                               [&key](const Context::PopupState & candidate)
                                               {
                                                   auto returnedValue = candidate.open && candidate.key == key.value();

                                                   return returnedValue;
                                               });

            if(iterator != ui->popupStack.rend())
            {
                state = &*iterator;
                id = state->id;
            }
        }

        if(state == nullptr)
        {
            return {};
        }

        if(state->open == false)
        {
            return {};
        }

        size_t submittedNode = ui->findFrameNodeIndex(state->node);
        bool submittedPopupWindow = submittedNode < ui->nodes.size();

        if(submittedPopupWindow == true)
        {
            if(ui->nodes[submittedNode].kind != Detail::NodeKind::Window)
            {
                submittedPopupWindow = false;
            }

            if(ui->nodes[submittedNode].windowData().popup == false)
            {
                submittedPopupWindow = false;
            }

            if(ui->nodes[submittedNode].inputLayer != id)
            {
                submittedPopupWindow = false;
            }
        }

        if(submittedPopupWindow == true)
        {
            Id previousInputLayer = ui->currentInputLayer;
            ui->blockingInputLayer = id;
            ui->currentInputLayer = id;
            uint64_t token = ui->pushScope(submittedNode, ui->currentStyle, ui->currentDisabled);

            if(ui->scopes.empty() == false)
            {
                Context::ScopeState & scope = ui->scopes.back();
                scope.previousInputLayer = previousInputLayer;
            }

            ui->currentInputLayer = id;

            return {ui, token, ui->nodes[submittedNode].id, true};
        }

        state->lastFrame = ui->frame.number;

        if(options.owner != InvalidId)
        {
            state->options.minimumSize = options.minimumSize;
            state->options.maximumSize = options.maximumSize;
            state->options.closeOnEscape = options.closeOnEscape;
            state->options.closeOnClickOutside = options.closeOnClickOutside;
            state->options.closeOnSelection = options.closeOnSelection;
            state->options.allowSiblingOwners = options.allowSiblingOwners;
            state->options.horizontalAlignment = options.horizontalAlignment;
            state->options.modal = options.modal;
            state->options.titleBar = options.titleBar;
            state->options.movable = options.movable;
            state->options.resizable = options.resizable;
            state->options.backdropColor = options.backdropColor;

            if(options.anchor.empty() == false)
            {
                state->options.anchor = options.anchor;
            }

            if(options.placement != PopupPlacement::Automatic)
            {
                state->options.placement = options.placement;
            }
        }

        Id previousInputLayer = ui->currentInputLayer;
        size_t previousParent = ui->currentParent;
        ui->blockingInputLayer = id;
        ui->currentInputLayer = id;
        ui->currentParent = 0;

        if(state->options.modal == true)
        {
            LayoutOptions backdropLayout;
            backdropLayout.width = SizeRule::Fill;
            backdropLayout.height = SizeRule::Fill;
            size_t backdropNode = ui->addNode(Detail::NodeKind::Backdrop, Key(combineId(id, hashBytes("Backdrop"))), "Modal backdrop", backdropLayout, location);
            Context::Node & backdrop = ui->nodes[backdropNode];
            backdrop.inputLayer = id;
            backdrop.tint = state->options.backdropColor;
            backdrop.response = ui->interact(backdropNode, false);
        }

        WindowOptions windowOptions;
        windowOptions.initialBounds = state->bounds.empty() == true ? Rect{state->options.anchor.x, state->options.anchor.bottom(), std::max(ui->currentStyle->metrics.minimumPopupWidth, state->options.minimumSize.x), std::max(ui->currentStyle->metrics.controlHeight, state->options.minimumSize.y)} : state->bounds;
        windowOptions.movable = state->options.movable;
        windowOptions.resizable = state->options.resizable;
        windowOptions.dockable = false;
        windowOptions.titleBar = state->options.titleBar;
        bool popupWindowOpen = true;
        windowOptions.open = state->options.titleBar ? &popupWindowOpen : nullptr;
        WindowScope result = Mosaic::window(ui, Key(id), label, windowOptions, location);
        Context::Node & node = ui->nodes.back();
        state->node = node.id;
        node.inputLayer = id;
        node.mutableWindowData().autoSize = true;
        node.mutableWindowData().popup = true;
        Context::Persistent & windowState = ui->state(node);
        windowState.windowData().popup = true;
        windowState.windowData().zOrder = ui->nextWindowZOrder++;
        node.mutableWindowData().zOrder = windowState.windowData().zOrder;
        node.mutableWindowData().minimumSize = state->options.minimumSize;
        node.mutableWindowData().maximumSize = state->options.maximumSize;
        node.mutableWindowData().popupAnchor = state->options.anchor;
        node.mutableWindowData().popupPlacement = state->options.placement;
        node.mutableWindowData().popupHorizontalAlignment = state->options.horizontalAlignment;
        node.layout.width = SizeRule::Content;
        node.layout.height = SizeRule::Content;
        node.layout.minimum = state->options.minimumSize;
        node.layout.maximum = state->options.maximumSize;
        Theme & style = ui->mutableStyle(node);
        style.colors.panel = style.colors.popup;
        style.colors.background = style.colors.popup;
        style.colors.border = style.colors.popupBorder;
        style.colors.borderStrong = style.colors.popupBorder;

        if(ui->scopes.empty() == false)
        {
            Context::ScopeState & scope = ui->scopes.back();
            scope.previousInputLayer = previousInputLayer;
            scope.previousParent = previousParent;
        }

        ui->currentInputLayer = id;

        if(popupWindowOpen == false)
        {
            Detail::closePopupById(ui, id);
        }

        return result;
    }

    //////////////////////////////////////////////////////////////////////////
    WindowScope contextPopup(Context * ui, const Key & key, StringView label, const Response & item, const PopupOptions & options, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return {};
        }

        PopupOptions resolvedOptions = options;
        resolvedOptions.owner = item.id;
        const PointerState * pointer = ui->input.primaryPointer();

        if(pointer != nullptr && item.hovered() == true && pointer->isPressed(PointerButton::Secondary) == true)
        {
            resolvedOptions.anchor = {pointer->position.x, pointer->position.y, 1.f, 1.f};
            resolvedOptions.placement = PopupPlacement::Cursor;
            Mosaic::openPopup(ui, key, resolvedOptions);
        }

        return Mosaic::popup(ui, key, label, resolvedOptions, location);
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope contextWindowPopup(Context * ui, const Key & key, StringView label, const PopupOptions & options, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return {};
        }

        if(ui->currentWindow == InvalidId)
        {
            return {};
        }

        PopupOptions resolvedOptions = options;
        resolvedOptions.owner = ui->currentWindow;
        const PointerState * pointer = ui->input.primaryPointer();

        if(pointer != nullptr && ui->pointerWindow == ui->currentWindow && pointer->isPressed(PointerButton::Secondary) == true)
        {
            resolvedOptions.anchor = {pointer->position.x, pointer->position.y, 1.f, 1.f};
            resolvedOptions.placement = PopupPlacement::Cursor;
            Mosaic::openPopup(ui, key, resolvedOptions);
        }

        return Mosaic::popup(ui, key, label, resolvedOptions, location);
    }
    //////////////////////////////////////////////////////////////////////////
    void closeCurrentPopup(Context * ui) noexcept
    {
        if(ui->popupStack.empty() == true)
        {
            return;
        }

        Detail::closePopupRange(ui, ui->popupStack.size() - 1, false, false);
    }
    //////////////////////////////////////////////////////////////////////////
    void closePopup(Context * ui, const Key & key, Id owner) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(owner != InvalidId)
        {
            Detail::closePopupById(ui, Detail::popupId(ui, key, owner));

            return;
        }

        auto iterator = std::find_if(ui->popupStack.rbegin(), ui->popupStack.rend(),
            [&key](const Context::PopupState & popup)
            {
                return popup.open == true && popup.key == key.value();
            });

        if(iterator == ui->popupStack.rend())
        {
            return;
        }

        Detail::closePopupById(ui, iterator->id);
    }
    //////////////////////////////////////////////////////////////////////////
    bool isPopupOpen(const Context * ui, const Key & key) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        auto iterator = std::find_if(ui->popupStack.rbegin(), ui->popupStack.rend(),
            [&key](const Context::PopupState & popup)
            {
                return popup.open == true && popup.key == key.value();
            });
        bool returnedValue = iterator != ui->popupStack.rend();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool isPopupOpen(const Context * ui, const Key & key, Id owner) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        const Context::PopupState * state = Detail::findPopup(ui, Detail::popupId(ui, key, owner));

        return state != nullptr && state->open;
    }
    //////////////////////////////////////////////////////////////////////////
    bool isAnyPopupOpen(const Context * ui) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        auto iterator = std::find_if(ui->popupStack.begin(), ui->popupStack.end(),
                                     [](const Context::PopupState & popup)
                                     {
                                         return popup.open == true;
                                     });
        auto returnedValue = iterator != ui->popupStack.end();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    uint32_t popupLevel(const Context * ui) noexcept
    {
        if(ui == nullptr)
        {
            return 0;
        }

        uint32_t level = 0;

        for(const Context::PopupState & popup : ui->popupStack)
        {
            if(popup.open == true)
            {
                ++level;
            }
        }

        return level;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
