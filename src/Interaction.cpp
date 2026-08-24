#include "Interaction.hpp"
#include "Popup.hpp"
#include "Utility.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Mosaic
{
    namespace Detail
    {
        namespace InteractionDetail
        {
            //////////////////////////////////////////////////////////////////////////
            void setFlag(Response & response, unsigned bit, bool value = true) noexcept
            {
                if(value == true)
                {
                    response.flags |= 1U << bit;
                }
            }
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] bool inputLayerBlocked(const Context * ui, const Context::Node & node) noexcept
            {
                Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;
                auto returnedValue = blockingLayer != InvalidId && node.inputLayer != blockingLayer && Detail::popupOwnerCanInteract(ui, node.id) == false;

                return returnedValue;
            }
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] const PointerState * pointer(Context * ui) noexcept
            {
                if(ui->capturedPointer != 0)
                {
                    for(const PointerState & candidate : ui->input.pointers)
                    {
                        if(candidate.id == ui->capturedPointer)
                        {
                            return &candidate;
                        }
                    }
                }

                auto returnedValue = ui->input.primaryPointer();

                return returnedValue;
            }
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] bool isEditing(const Context * ui, Id id) noexcept
            {
                const Context::Persistent * state = ui->findState(id);

                return state != nullptr && state->editing;
            }
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] Vec2 center(const Rect & rect) noexcept
            {
                return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
            }
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] Id directionalFocus(const Context * ui, KeyCode key) noexcept
            {
                if(ui->previousFocusOrder.empty() == true)
                {
                    return InvalidId;
                }

                const Context::Persistent * current = ui->findState(ui->navigationFocused);

                if(current == nullptr)
                {
                    auto returnedValue = ui->previousFocusOrder.front();

                    return returnedValue;
                }

                if(current->lastBounds.empty() == true)
                {
                    auto returnedValue = ui->previousFocusOrder.front();

                    return returnedValue;
                }

                Vec2 origin = InteractionDetail::center(current->lastBounds);
                Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;
                Id best = InvalidId;
                float bestScore = std::numeric_limits<float>::max();
                for(Id candidateId : ui->previousFocusOrder)
                {
                    if(candidateId == ui->navigationFocused)
                    {
                        continue;
                    }

                    const Context::Persistent * candidate = ui->findState(candidateId);

                    if(candidate == nullptr)
                    {
                        continue;
                    }

                    if(candidate->lastBounds.empty() == true)
                    {
                        continue;
                    }

                    if(blockingLayer != InvalidId && candidate->lastInputLayer != blockingLayer)
                    {
                        continue;
                    }

                    Vec2 target = InteractionDetail::center(candidate->lastBounds);
                    float x = target.x - origin.x;
                    float y = target.y - origin.y;
                    bool valid = (key == KeyCode::Left && x < 0.f) || (key == KeyCode::Right && x > 0.f) || (key == KeyCode::Up && y < 0.f) || (key == KeyCode::Down && y > 0.f);

                    if(valid == false)
                    {
                        continue;
                    }

                    float primary = key == KeyCode::Left || key == KeyCode::Right ? std::abs(x) : std::abs(y);
                    float secondary = key == KeyCode::Left || key == KeyCode::Right ? std::abs(y) : std::abs(x);
                    float score = primary + secondary * 2.5f;

                    if(score < bestScore)
                    {
                        bestScore = score;
                        best = candidateId;
                    }
                }

                return best;
            }
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] size_t frameNodeIndex(const Context * ui, Id id) noexcept
            {
                const Context::Node * node = ui->findFrameNode(id);
                auto returnedValue = node == nullptr ? std::numeric_limits<size_t>::max() : static_cast<size_t>(node - ui->nodes.data());

                return returnedValue;
            }
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] Id shortcutRouteOwner(const Context * ui, size_t node) noexcept
            {
                while(node < ui->nodes.size())
                {
                    const Context::Node & candidate = ui->nodes[node];

                    if(candidate.kind == NodeKind::Scroll)
                    {
                        return candidate.id;
                    }

                    if(candidate.kind == NodeKind::Window)
                    {
                        return candidate.id;
                    }

                    if(node == 0)
                    {
                        break;
                    }

                    if(candidate.parent == node)
                    {
                        break;
                    }

                    node = candidate.parent;
                }

                return RootId;
            }
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] bool shortcutRouteMatches(Context * ui, size_t node, const ShortcutOptions & options) noexcept
            {
                if(node >= ui->nodes.size())
                {
                    return false;
                }

                const Context::Node & candidate = ui->nodes[node];

                if(options.route != ShortcutRoute::Always && ui->blockingInputLayer != InvalidId && candidate.inputLayer != ui->blockingInputLayer)
                {
                    return false;
                }

                Id owner = InteractionDetail::shortcutRouteOwner(ui, node);
                size_t focusedNode = InteractionDetail::frameNodeIndex(ui, ui->focused);
                size_t activeNode = InteractionDetail::frameNodeIndex(ui, ui->active);
                Id focusedOwner = focusedNode == std::numeric_limits<size_t>::max() ? InvalidId : InteractionDetail::shortcutRouteOwner(ui, focusedNode);
                Id activeOwner = activeNode == std::numeric_limits<size_t>::max() ? InvalidId : InteractionDetail::shortcutRouteOwner(ui, activeNode);
                switch(options.route)
                {
                case ShortcutRoute::Active:
                {
                    auto returnedValue = ui->active != InvalidId && (ui->active == candidate.id || activeOwner == owner);

                    return returnedValue;
                }
                case ShortcutRoute::Focused:
                {
                    auto returnedValue = ui->focused != InvalidId && (ui->focused == candidate.id || focusedOwner == owner);

                    return returnedValue;
                }
                case ShortcutRoute::Global:
                {
                    if(ui->active != InvalidId && options.routeOverActive == false)
                    {
                        return false;
                    }

                    if(ui->focused != InvalidId && options.routeOverFocused == false)
                    {
                        return false;
                    }

                    if(options.routeUnlessBackgroundFocused == true && ui->focused == InvalidId)
                    {
                        return false;
                    }

                    return true;
                }
                case ShortcutRoute::Always:
                {
                    return true;
                }
                }

                return false;
            }
            //////////////////////////////////////////////////////////////////////////
        } // namespace InteractionDetail
        //////////////////////////////////////////////////////////////////////////
        bool shortcutTriggered(Context * ui, size_t node, const Shortcut & shortcut, const ShortcutOptions & options) noexcept
        {
            if(ui == nullptr)
            {
                return false;
            }

            if(ui->configuration.keyboardInput == false)
            {
                return false;
            }

            if(InteractionDetail::shortcutRouteMatches(ui, node, options) == false)
            {
                return false;
            }

            for(const KeyEvent & event : ui->input.keyboard)
            {
                if(event.pressed == false)
                {
                    continue;
                }

                if(event.key != shortcut.key)
                {
                    continue;
                }

                if((event.repeat == true && options.repeat == false))
                {
                    continue;
                }

                if(Detail::modifierMatches(event.modifiers, shortcut.modifiers) == false)
                {
                    continue;
                }

                return true;
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        Response itemBehavior(Context * ui, size_t index, const ItemBehaviorOptions & options, const Rect * interactionBounds)
        {
            Context::Node & node = ui->nodes[index];
            Context::Persistent & state = ui->state(node);
            state.lastInputLayer = node.inputLayer;
            Response response;
            response.id = node.id;
            InteractionDetail::setFlag(response, 13, node.disabled);
            const PointerState * pointer = InteractionDetail::pointer(ui);
            Rect hitBounds = interactionBounds == nullptr ? state.lastBounds : *interactionBounds;

            if(pointer != nullptr && pointer->type == PointerType::Touch)
            {
                Vec2 extra = node.style->metrics.touchExtraPadding;
                hitBounds = {hitBounds.x - extra.x, hitBounds.y - extra.y, hitBounds.width + extra.x * 2.f, hitBounds.height + extra.y * 2.f};
            }

            bool hit = pointer != nullptr && hitBounds.empty() == false && state.lastClip.empty() == false && hitBounds.contains(pointer->position) && state.lastClip.contains(pointer->position);
            bool canInteract = node.disabled == false && node.inputBlocked == false && InteractionDetail::inputLayerBlocked(ui, node) == false && (ui->pointerWindow == InvalidId || node.windowOwner == ui->pointerWindow || ui->captured == node.id);
            bool pointerCapturedElsewhere = ui->captured != InvalidId && ui->captured != node.id;
            bool pointerAvailable = pointerCapturedElsewhere == false || options.allowOverlap;
            bool canPoint = canInteract && pointerAvailable;
            bool hovered = canPoint && hit == true && node.style->behavior.hoverEnabled == true && pointer != nullptr && pointer->type != PointerType::Touch;
            bool tooltipHovered = node.inputBlocked == false && InteractionDetail::inputLayerBlocked(ui, node) == false && (ui->pointerWindow == InvalidId || node.windowOwner == ui->pointerWindow) && pointerAvailable == true && hit == true && node.style->behavior.hoverEnabled == true && pointer != nullptr && pointer->type != PointerType::Touch;
            InteractionDetail::setFlag(response, 0, hovered);

            if(tooltipHovered == true)
            {
                if(state.hoverLastFrame == 0 || state.hoverLastFrame + 1 != ui->frame.number)
                {
                    state.hoverStartedTimestamp = ui->input.timestamp;
                    state.stationaryHoverStartedTimestamp = ui->input.timestamp;
                }

                state.hoverDuration = std::max(0.0, ui->input.timestamp - state.hoverStartedTimestamp);

                if(std::abs(pointer->delta.x) > 0.25f || std::abs(pointer->delta.y) > 0.25f)
                {
                    state.stationaryHoverStartedTimestamp = ui->input.timestamp;
                }

                state.stationaryHoverDuration = std::max(0.0, ui->input.timestamp - state.stationaryHoverStartedTimestamp);
                state.hoverLastFrame = ui->frame.number;
            }
            else
            {
                state.hoverStartedTimestamp = ui->input.timestamp;
                state.hoverDuration = 0.0;
                state.stationaryHoverStartedTimestamp = ui->input.timestamp;
                state.stationaryHoverDuration = 0.0;
                state.hoverLastFrame = 0;
            }

            if(canPoint == true && pointer != nullptr && hit == true && pointer->isPressed(options.pointerButton) == true)
            {
                bool focusChanged = ui->focused != node.id;
                ui->active = node.id;
                ui->captured = node.id;
                ui->capturedPointer = pointer->id;
                ui->pointerFocused = node.id;
                ui->focused = node.id;
                ui->navigationFocused = node.id;
                state.pressBounds = hitBounds;
                state.pressClip = state.lastClip;
                state.pressStartedTimestamp = ui->input.timestamp;
                state.lastRepeatTimestamp = ui->input.timestamp;
                InteractionDetail::setFlag(response, 3);
                InteractionDetail::setFlag(response, 14);
                InteractionDetail::setFlag(response, 10, pointer->buttonClickCount(options.pointerButton) >= 2);
                bool doubleClicked = pointer->buttonClickCount(options.pointerButton) >= 2;
                bool pressedByPolicy = options.pressPolicy == ItemPressPolicy::Press;

                if(options.pressPolicy == ItemPressPolicy::DoubleClick)
                {
                    pressedByPolicy = doubleClicked;
                }

                if(options.allowDoubleClick == true)
                {
                    if(doubleClicked == true)
                    {
                        pressedByPolicy = true;
                    }
                }

                if(pressedByPolicy == true)
                {
                    InteractionDetail::setFlag(response, 5);
                }

                ui->frame.events.push_back({EventType::PointerDown, node.id, ui->nodePath(node), node.file, node.line, ui->input.timestamp});

                if(focusChanged == true)
                {
                    ui->frame.events.push_back({EventType::FocusChanged, node.id, ui->nodePath(node), node.file, node.line, ui->input.timestamp});
                }
            }

            InteractionDetail::setFlag(response, 2, canInteract && ui->focused == node.id);
            bool ownsPointer = ui->captured == node.id || ui->active == node.id;
            InteractionDetail::setFlag(response, 1, ownsPointer && pointer != nullptr && pointer->isDown(options.pointerButton));

            bool repeatPressed = options.repeat;

            if(ownsPointer == false)
            {
                repeatPressed = false;
            }

            if(pointer == nullptr)
            {
                repeatPressed = false;
            }

            if(repeatPressed == true)
            {
                repeatPressed = pointer->isDown(options.pointerButton);
            }

            if(ui->input.timestamp - state.pressStartedTimestamp < 0.35)
            {
                repeatPressed = false;
            }

            if(ui->input.timestamp - state.lastRepeatTimestamp < 0.075)
            {
                repeatPressed = false;
            }

            if(repeatPressed == true)
            {
                state.lastRepeatTimestamp = ui->input.timestamp;
                InteractionDetail::setFlag(response, 5);
                InteractionDetail::setFlag(response, 11);
            }

            if(pointer != nullptr && ownsPointer == true && pointer->isReleased(options.pointerButton) == true)
            {
                InteractionDetail::setFlag(response, 4);
                InteractionDetail::setFlag(response, 15);

                if(options.pressPolicy == ItemPressPolicy::Release)
                {
                    bool pressedRegionHit = state.pressBounds.empty() == false && state.pressClip.empty() == false && state.pressBounds.contains(pointer->position) && state.pressClip.contains(pointer->position);
                    bool releaseCanInteract = node.disabled == false && node.inputBlocked == false && InteractionDetail::inputLayerBlocked(ui, node) == false && (ui->pointerWindow == InvalidId || node.windowOwner == ui->pointerWindow || ui->captured == node.id);
                    InteractionDetail::setFlag(response, 5, releaseCanInteract && (hit == true || pressedRegionHit == true));
                }

                ui->frame.events.push_back({EventType::PointerUp, node.id, ui->nodePath(node), node.file, node.line, ui->input.timestamp});
                ui->active = InvalidId;
                ui->captured = InvalidId;
                ui->capturedPointer = 0;
                state.pressBounds = {};
                state.pressClip = {};
            }

            if(canInteract == true && options.keyboardActivation == true && ui->configuration.keyboardNavigation == true && ui->navigationFocused == node.id)
            {
                for(const KeyEvent & event : ui->input.keyboard)
                {
                    if(event.pressed == false)
                    {
                        continue;
                    }

                    if((event.key != KeyCode::Enter && event.key != KeyCode::Space))
                    {
                        continue;
                    }

                    InteractionDetail::setFlag(response, 5);
                    InteractionDetail::setFlag(response, 3);
                    InteractionDetail::setFlag(response, 4);
                    InteractionDetail::setFlag(response, 14);
                    InteractionDetail::setFlag(response, 15);
                    InteractionDetail::setFlag(response, 11, event.repeat);
                    break;
                }
            }

            bool showShortcutTooltip = false;
            Shortcut shortcutTooltip;

            if(ui->nextItemShortcutPending == true)
            {
                bool triggered = canInteract && Detail::shortcutTriggered(ui, index, ui->nextItemShortcut, ui->nextItemShortcutOptions);
                showShortcutTooltip = ui->nextItemShortcutOptions.tooltip;
                shortcutTooltip = ui->nextItemShortcut;
                ui->nextItemShortcutPending = false;

                if(triggered == true)
                {
                    InteractionDetail::setFlag(response, 3);
                    InteractionDetail::setFlag(response, 4);
                    InteractionDetail::setFlag(response, 5);
                    InteractionDetail::setFlag(response, 14);
                    InteractionDetail::setFlag(response, 15);
                }
            }

            node.response = response;

            if(node.focusable == true && node.navigationBlocked == false && state.lastClip.empty() == false)
            {
                ui->focusOrder.push_back(node.id);
            }

            if(showShortcutTooltip == true)
            {
                Mosaic::itemTooltip(ui, response, Mosaic::shortcutLabel(shortcutTooltip));
            }

            return response;
        }
        //////////////////////////////////////////////////////////////////////////
        void updateNavigation(Context * ui)
        {
            IdVector & focusCandidates = ui->navigationCandidates;
            focusCandidates.clear();
            Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;
            focusCandidates.reserve(ui->previousFocusOrder.size());
            for(Id candidate : ui->previousFocusOrder)
            {
                const Context::Persistent * candidateState = ui->findState(candidate);

                if(blockingLayer == InvalidId || (candidateState != nullptr && candidateState->lastInputLayer == blockingLayer))
                {
                    focusCandidates.push_back(candidate);
                }
            }

            if(focusCandidates.empty() == true)
            {
                return;
            }

            if(ui->navigationFocused == InvalidId)
            {
                ui->navigationFocused = ui->focused;
            }

            bool windowCycleModifier = ui->input.modifiers.control || ui->input.modifiers.primary;

            if(windowCycleModifier == true && ui->input.keyPressed(KeyCode::Tab) == true)
            {
                IdVector windows;
                for(Id candidate : ui->previousVisibleWindowIds)
                {
                    const Context::Persistent * state = ui->findState(candidate);

                    if(state != nullptr && state->windowVisible == true && state->windowAcceptsInput == true && state->windowPopup == false)
                    {
                        windows.push_back(candidate);
                    }
                }

                if(windows.empty() == false)
                {
                    Id focusedWindow = InvalidId;
                    const Context::Persistent * focusedState = ui->findState(ui->focused);

                    if(focusedState != nullptr)
                    {
                        focusedWindow = focusedState->windowOwner;
                    }

                    auto iterator = std::find(windows.begin(), windows.end(), focusedWindow);
                    std::ptrdiff_t index = iterator == windows.end() ? -1 : std::distance(windows.begin(), iterator);

                    if(ui->input.modifiers.shift == true)
                    {
                        index = index <= 0 ? static_cast<std::ptrdiff_t>(windows.size() - 1) : index - 1;
                    }
                    else
                    {
                        index = (index + 1) % static_cast<std::ptrdiff_t>(windows.size());
                    }

                    Id selectedWindow = windows[static_cast<size_t>(index)];
                    ui->focused = selectedWindow;
                    ui->navigationFocused = selectedWindow;
                    Context::Persistent & selectedState = ui->state(selectedWindow);

                    if(selectedState.windowBringToFront == true)
                    {
                        selectedState.windowZOrder = ui->nextWindowZOrder++;
                    }
                }

                return;
            }

            if(ui->input.keyPressed(KeyCode::Tab) == true)
            {
                const Context::Persistent * focusedState = ui->findState(ui->focused);

                if(focusedState != nullptr && focusedState->editing == true && focusedState->acceptsTabInput == true)
                {
                    return;
                }

                auto iterator = std::find(focusCandidates.begin(), focusCandidates.end(), ui->navigationFocused);
                std::ptrdiff_t index = iterator == focusCandidates.end() ? -1 : std::distance(focusCandidates.begin(), iterator);

                if(ui->input.modifiers.shift == true)
                {
                    index = index <= 0 ? static_cast<std::ptrdiff_t>(focusCandidates.size() - 1) : index - 1;
                }
                else
                {
                    index = (index + 1) % static_cast<std::ptrdiff_t>(focusCandidates.size());
                }

                ui->navigationFocused = focusCandidates[static_cast<size_t>(index)];
                ui->focused = ui->navigationFocused;

                return;
            }

            if(InteractionDetail::isEditing(ui, ui->focused) == true)
            {
                return;
            }

            const Context::Persistent * focusedState = ui->findState(ui->focused);
            bool focusedScrollNavigation = false;

            if(focusedState != nullptr)
            {
                bool scrollable = focusedState->scrollRange.x > 0.f;

                if(focusedState->scrollRange.y > 0.f)
                {
                    scrollable = true;
                }

                if(scrollable == true)
                {
                    focusedScrollNavigation = ui->input.keyPressed(KeyCode::Left);

                    if(ui->input.keyPressed(KeyCode::Right) == true)
                    {
                        focusedScrollNavigation = true;
                    }

                    if(ui->input.keyPressed(KeyCode::Up) == true)
                    {
                        focusedScrollNavigation = true;
                    }

                    if(ui->input.keyPressed(KeyCode::Down) == true)
                    {
                        focusedScrollNavigation = true;
                    }

                    if(ui->input.keyPressed(KeyCode::PageUp) == true)
                    {
                        focusedScrollNavigation = true;
                    }

                    if(ui->input.keyPressed(KeyCode::PageDown) == true)
                    {
                        focusedScrollNavigation = true;
                    }

                    if(ui->input.keyPressed(KeyCode::Home) == true)
                    {
                        focusedScrollNavigation = true;
                    }

                    if(ui->input.keyPressed(KeyCode::End) == true)
                    {
                        focusedScrollNavigation = true;
                    }
                }
            }

            if(focusedScrollNavigation == true)
            {
                return;
            }

            for(KeyCode key : {KeyCode::Left, KeyCode::Right, KeyCode::Up, KeyCode::Down})
            {
                if(ui->input.keyPressed(key) == false)
                {
                    continue;
                }

                if(key == KeyCode::Left || key == KeyCode::Right)
                {
                    auto current = std::find_if(ui->previousSelectionItems.begin(), ui->previousSelectionItems.end(),
                                                      [ui](const Context::SelectionItem & item)
                                                      {
                                                          return item.node == ui->navigationFocused && item.navigationWrapX;
                                                      });

                    if(current != ui->previousSelectionItems.end())
                    {
                        IdVector wrappedItems;
                        for(const Context::SelectionItem & item : ui->previousSelectionItems)
                        {
                            if(item.model == current->model && item.scope == current->scope && item.navigationWrapX == true)
                            {
                                wrappedItems.push_back(item.node);
                            }
                        }
                        auto wrapped = std::find(wrappedItems.begin(), wrappedItems.end(), current->node);

                        if(wrapped != wrappedItems.end() && wrappedItems.empty() == false)
                        {
                            size_t index = static_cast<size_t>(wrapped - wrappedItems.begin());
                            size_t nextIndex = key == KeyCode::Right ? (index + 1) % wrappedItems.size() : (index == 0 ? wrappedItems.size() - 1 : index - 1);
                            ui->navigationFocused = wrappedItems[nextIndex];
                            ui->focused = ui->navigationFocused;

                            return;
                        }
                    }
                }
                Id next = InteractionDetail::directionalFocus(ui, key);

                if(next != InvalidId)
                {
                    ui->navigationFocused = next;
                    ui->focused = next;
                }

                return;
            }

            bool home = ui->input.keyPressed(KeyCode::Home);
            bool end = ui->input.keyPressed(KeyCode::End);
            bool pageUp = ui->input.keyPressed(KeyCode::PageUp);
            bool pageDown = ui->input.keyPressed(KeyCode::PageDown);

            if(home == false && end == false && pageUp == false && pageDown == false)
            {
                return;
            }

            auto iterator = std::find(focusCandidates.begin(), focusCandidates.end(), ui->navigationFocused);
            std::ptrdiff_t index = iterator == focusCandidates.end() ? 0 : std::distance(focusCandidates.begin(), iterator);

            if(home == true)
            {
                index = 0;
            }
            else if(end == true)
            {
                index = static_cast<std::ptrdiff_t>(focusCandidates.size() - 1);
            }
            else
            {
                constexpr std::ptrdiff_t PageItemCount = 10;
                index += pageUp ? -PageItemCount : PageItemCount;
                index = std::clamp(index, std::ptrdiff_t{0}, static_cast<std::ptrdiff_t>(focusCandidates.size() - 1));
            }

            ui->navigationFocused = focusCandidates[static_cast<size_t>(index)];
            ui->focused = ui->navigationFocused;
        }
        //////////////////////////////////////////////////////////////////////////
        void updateInputCapture(Context * ui) noexcept
        {
            InputCapture capture;
            capture.pointer = ui->captured != InvalidId || ui->blockingInputLayer != InvalidId || ui->previousBlockingInputLayer != InvalidId;
            const PointerState * pointer = ui->input.primaryPointer();

            if(capture.pointer == false && pointer != nullptr)
            {
                for(auto iterator = ui->nodes.rbegin(); iterator != ui->nodes.rend(); ++iterator)
                {
                    const Context::Node & node = *iterator;

                    if(node.visible == false)
                    {
                        continue;
                    }

                    if(node.disabled == true)
                    {
                        continue;
                    }

                    if(node.inputBlocked == true)
                    {
                        continue;
                    }

                    if(node.kind == NodeKind::Root)
                    {
                        continue;
                    }

                    if(node.bounds.empty() == true)
                    {
                        continue;
                    }

                    if(node.clip.empty() == true)
                    {
                        continue;
                    }

                    if(node.bounds.contains(pointer->position) == false)
                    {
                        continue;
                    }

                    if(node.clip.contains(pointer->position) == false)
                    {
                        continue;
                    }

                    bool capturesPointer = node.focusable;

                    if(node.kind == NodeKind::Window)
                    {
                        capturesPointer = true;
                    }

                    if(node.kind == NodeKind::Scroll)
                    {
                        capturesPointer = true;
                    }

                    if(node.kind == NodeKind::Split)
                    {
                        capturesPointer = true;
                    }

                    if(node.kind == NodeKind::Backdrop)
                    {
                        capturesPointer = true;
                    }

                    if(capturesPointer == true)
                    {
                        capture.pointer = true;
                        break;
                    }
                }
            }

            capture.pointerUnlessPopupClose = capture.pointer && ui->popupClosedByOutsidePointer == false;
            capture.text = InteractionDetail::isEditing(ui, ui->focused);
            capture.keyboard = ui->blockingInputLayer != InvalidId || capture.text == true || (ui->configuration.navigationCapturesKeyboard && (ui->focused != InvalidId || ui->navigationFocused != InvalidId));

            if(ui->inputCaptureOverride.pointer >= 0)
            {
                capture.pointer = ui->inputCaptureOverride.pointer != 0;
                capture.pointerUnlessPopupClose = capture.pointer;
            }

            if(ui->inputCaptureOverride.keyboard >= 0)
            {
                capture.keyboard = ui->inputCaptureOverride.keyboard != 0;
            }

            if(ui->inputCaptureOverride.text >= 0)
            {
                capture.text = ui->inputCaptureOverride.text != 0;
            }

            ui->frame.inputCapture = capture;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
} // namespace Mosaic
