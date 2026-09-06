#include "Window.hpp"
#include "ContextDetail.hpp"
#include "Interaction.hpp"
#include "Layout.hpp"
#include "Utility.hpp"

#include <algorithm>
#include <cmath>

namespace Mosaic
{
    namespace Detail
    {
        struct WindowScrollResult
        {
            bool hovered = false;
            bool active = false;
            bool changed = false;
            bool consumedPress = false;
        };
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] WindowScrollResult windowScrollBehavior(Context * ui, Context::Node & node, Context::Persistent & state, bool canPoint)
        {
            WindowScrollResult result;

            if(node.windowData().scrollable == false)
            {
                return result;
            }

            const PointerState * pointer = ui->input.primaryPointer();

            if(pointer == nullptr)
            {
                return result;
            }

            bool verticalHit = state.scrollData().verticalTrack.empty() == false && state.scrollData().verticalTrack.contains(pointer->position);
            bool horizontalHit = state.scrollData().horizontalTrack.empty() == false && state.scrollData().horizontalTrack.contains(pointer->position);
            result.hovered = canPoint && (verticalHit || horizontalHit == true);
            bool ownsScrollbar = ui->captured == node.id && (state.windowData().interaction == 7 || state.windowData().interaction == 8);

            if(canPoint == true && pointer->isPressed() == true && (verticalHit == true || horizontalHit == true))
            {
                bool vertical = verticalHit;
                const Rect & track = vertical ? state.scrollData().verticalTrack : state.scrollData().horizontalTrack;
                const Rect & thumb = vertical ? state.scrollData().verticalThumb : state.scrollData().horizontalThumb;
                float pointerPosition = vertical ? pointer->position.y : pointer->position.x;
                float thumbStart = vertical ? thumb.y : thumb.x;
                float thumbExtent = vertical ? thumb.height : thumb.width;
                float & position = vertical ? state.scrollData().position.y : state.scrollData().position.x;
                float extent = vertical ? state.scrollData().range.y : state.scrollData().range.x;

                ui->captured = node.id;
                ui->capturedPointer = pointer->id;
                ui->active = node.id;
                ui->focused = node.id;
                state.windowData().interaction = vertical ? 7 : 8;
                state.scrollData().draggingScrollbar = true;
                state.scrollData().draggingAxis = vertical ? 2 : 1;
                result.consumedPress = true;

                if(thumb.contains(pointer->position) == true)
                {
                    state.scrollData().dragOffset = pointerPosition - thumbStart;
                }
                else
                {
                    float previous = position;

                    if(ui->configuration.scrollbarScrollByPage == true)
                    {
                        float page = vertical ? std::max(1.f, node.content.height) : std::max(1.f, node.content.width);
                        position = std::clamp(position + (pointerPosition < thumbStart ? -page : page), 0.f, extent);
                    }
                    else
                    {
                        float trackStart = vertical ? track.y : track.x;
                        float trackExtent = vertical ? track.height : track.width;
                        float travel = std::max(0.f, trackExtent - thumbExtent);
                        position = travel <= 0.f ? 0.f : std::clamp((pointerPosition - trackStart - thumbExtent * 0.5f) / travel, 0.f, 1.f) * extent;
                    }

                    state.scrollData().dragOffset = thumbExtent * 0.5f;
                    result.changed = position != previous;
                }

                state.scrollData().target = state.scrollData().position;
                state.scrollData().velocity = {};
                state.scrollData().targetInitialized = true;
            }

            if((ownsScrollbar == true || result.consumedPress == true) && pointer->isDown() == true)
            {
                bool vertical = state.windowData().interaction == 7;
                const Rect & track = vertical ? state.scrollData().verticalTrack : state.scrollData().horizontalTrack;
                const Rect & thumb = vertical ? state.scrollData().verticalThumb : state.scrollData().horizontalThumb;
                float & position = vertical ? state.scrollData().position.y : state.scrollData().position.x;
                float previous = position;
                position = Detail::scrollbarScrollAtPointer(track, thumb, vertical ? state.scrollData().range.y : state.scrollData().range.x, vertical, state.scrollData().dragOffset, pointer->position);
                state.scrollData().target = state.scrollData().position;
                state.scrollData().velocity = {};
                state.scrollData().targetInitialized = true;
                result.changed = result.changed || position != previous;
            }

            result.active = ownsScrollbar || result.consumedPress;

            if(result.changed == true)
            {
                state.scrollData().value = node.scrollOptions().axes != ScrollAxes::Horizontal ? state.scrollData().position.y : state.scrollData().position.x;
                ui->frame.events.push_back({EventType::Change, node.id, ui->nodePath(node), node.debugData().file, node.debugData().line, ui->input.timestamp});
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        DockTargetRectArray dockTargetRects(const Rect & bounds) noexcept
        {
            float side = std::clamp(std::min(bounds.width, bounds.height) * 0.09f, 30.f, 38.f);
            float distance = side + 8.f;
            float centerX = bounds.x + bounds.width * 0.5f;
            float centerY = bounds.y + bounds.height * 0.5f;
            auto target = [side](float x, float y) -> Rect
            {
                Rect result = {x - side * 0.5f, y - side * 0.5f, side, side};

                return result;
            };
            Rect center = target(centerX, centerY);
            Rect left = target(centerX - distance, centerY);
            Rect right = target(centerX + distance, centerY);
            Rect top = target(centerX, centerY - distance);
            Rect bottom = target(centerX, centerY + distance);
            DockTargetRectArray result = {center, left, right, top, bottom};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        bool dockPlacementAt(const DockTargetRectArray & targets, const Vec2 & position, DockPlacement * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            for(size_t index = 0; index != targets.size(); ++index)
            {
                if(targets[index].contains(position) == false)
                {
                    continue;
                }

                *_out = static_cast<DockPlacement>(index);

                return true;
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        Rect dockPreviewBounds(const Rect & bounds, DockPlacement placement, float ratio) noexcept
        {
            Rect preview = bounds;
            float resolvedRatio = std::clamp(ratio, 0.05f, 0.95f);

            if(placement == DockPlacement::Left)
            {
                preview.width *= resolvedRatio;
            }
            else if(placement == DockPlacement::Right)
            {
                preview.x += preview.width * (1.f - resolvedRatio);
                preview.width *= resolvedRatio;
            }
            else if(placement == DockPlacement::Top)
            {
                preview.height *= resolvedRatio;
            }
            else if(placement == DockPlacement::Bottom)
            {
                preview.y += preview.height * (1.f - resolvedRatio);
                preview.height *= resolvedRatio;
            }

            return preview;
        }
        //////////////////////////////////////////////////////////////////////////
        Rect windowTitleBarBounds(const Context::Node & window, const Rect & bounds) noexcept
        {
            float border = std::max(0.f, window.windowData().popup ? window.style->metrics.popupBorderSize : window.style->metrics.windowBorderSize);
            Rect result = {bounds.x + border, bounds.y + border, std::max(0.f, bounds.width - border * 2.f), std::max(0.f, window.style->metrics.windowTitleHeight - border)};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        bool dockTabBounds(const Context * ui, const Context::Node & window, const DockNode & dockNode, size_t tabIndex, const Rect & titleBar, Rect * const _out) noexcept
        {
            if(ui == nullptr)
            {
                return false;
            }

            if(_out == nullptr)
            {
                return false;
            }

            if(tabIndex >= dockNode.tabs.size())
            {
                return false;
            }

            float buttonSide = std::max(12.f, window.style->metrics.windowTitleHeight - 8.f);
            float leftControlsWidth = window.windowData().collapseVisible && window.windowData().collapsePlacement == WindowCollapsePlacement::Left ? buttonSide + 2.f : 0.f;
            float rightControlsWidth = (window.windowData().closeVisible ? buttonSide + 2.f : 0.f) + (window.windowData().collapseVisible && window.windowData().collapsePlacement == WindowCollapsePlacement::Right ? buttonSide + 2.f : 0.f) + 4.f;
            float maximumX = titleBar.right() - rightControlsWidth;
            float tabX = titleBar.x + 4.f + leftControlsWidth;
            float tabTop = titleBar.y + 2.f;
            float tabHeight = std::max(0.f, titleBar.height - 3.f);
            for(size_t index = 0; index <= tabIndex; ++index)
            {
                Id tab = dockNode.tabs[index];
                auto label = ui->windowLabels.find(tab);
                StringView visibleLabel = label == ui->windowLabels.end() ? StringView("Panel") : StringView(label->second);
                const Context::Node * tabNode = ui->findFrameNode(tab);
                float unsavedWidth = tabNode != nullptr && tabNode->windowData().unsavedDocument ? window.style->metrics.fontSize * 0.75f : 0.f;
                float tabWidth = std::min(std::max(ui->estimateText(visibleLabel, *window.style).x + window.style->metrics.framePadding.left + window.style->metrics.framePadding.right + unsavedWidth, window.style->metrics.tabMinimumWidthBase), std::max(0.f, maximumX - tabX));

                if(index == tabIndex)
                {
                    if(tabWidth <= 0.f)
                    {
                        return false;
                    }

                    *_out = {tabX, tabTop, tabWidth, tabHeight};

                    return true;
                }

                tabX += tabWidth + 2.f;

                if(tabX >= maximumX)
                {
                    return false;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        uint8_t windowResizeEdgesAt(const Rect & bounds, const Vec2 & position, float thickness) noexcept
        {
            if(bounds.empty() == true)
            {
                return WindowResizeNone;
            }

            float hit = std::max(1.f, thickness);

            if(position.x < bounds.x - hit)
            {
                return WindowResizeNone;
            }

            if(position.x > bounds.right() + hit)
            {
                return WindowResizeNone;
            }

            if(position.y < bounds.y - hit)
            {
                return WindowResizeNone;
            }

            if(position.y > bounds.bottom() + hit)
            {
                return WindowResizeNone;
            }

            uint8_t edges = WindowResizeNone;

            if(std::abs(position.x - bounds.x) <= hit)
            {
                edges = static_cast<uint8_t>(edges | WindowResizeLeft);
            }
            else if(std::abs(position.x - bounds.right()) <= hit)
            {
                edges = static_cast<uint8_t>(edges | WindowResizeRight);
            }

            if(std::abs(position.y - bounds.y) <= hit)
            {
                edges = static_cast<uint8_t>(edges | WindowResizeTop);
            }
            else if(std::abs(position.y - bounds.bottom()) <= hit)
            {
                edges = static_cast<uint8_t>(edges | WindowResizeBottom);
            }

            return edges;
        }
        //////////////////////////////////////////////////////////////////////////
        Rect resizedWindowBounds(const Rect & start, uint8_t edges, const Vec2 & delta, const Vec2 & minimum, const Vec2 & maximum, const Rect & available) noexcept
        {
            float minimumWidth = std::min(std::max(0.f, minimum.x), available.width);
            float minimumHeight = std::min(std::max(0.f, minimum.y), available.height);
            float maximumWidth = std::max(minimumWidth, std::min(std::max(0.f, maximum.x), available.width));
            float maximumHeight = std::max(minimumHeight, std::min(std::max(0.f, maximum.y), available.height));
            Rect result = start;

            if((edges & WindowResizeLeft) != 0)
            {
                float fixedRight = start.right();
                float minimumX = std::max(available.x, fixedRight - maximumWidth);
                float maximumX = std::max(minimumX, fixedRight - minimumWidth);
                result.x = std::clamp(start.x + delta.x, minimumX, maximumX);
                result.width = fixedRight - result.x;
            }
            else if((edges & WindowResizeRight) != 0)
            {
                float minimumRight = start.x + minimumWidth;
                float maximumRight = std::max(minimumRight, std::min(available.right(), start.x + maximumWidth));
                float right = std::clamp(start.right() + delta.x, minimumRight, maximumRight);
                result.width = right - start.x;
            }

            if((edges & WindowResizeTop) != 0)
            {
                float fixedBottom = start.bottom();
                float minimumY = std::max(available.y, fixedBottom - maximumHeight);
                float maximumY = std::max(minimumY, fixedBottom - minimumHeight);
                result.y = std::clamp(start.y + delta.y, minimumY, maximumY);
                result.height = fixedBottom - result.y;
            }
            else if((edges & WindowResizeBottom) != 0)
            {
                float minimumBottom = start.y + minimumHeight;
                float maximumBottom = std::max(minimumBottom, std::min(available.bottom(), start.y + maximumHeight));
                float bottom = std::clamp(start.bottom() + delta.y, minimumBottom, maximumBottom);
                result.height = bottom - start.y;
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        Rect constrainResizedWindowBounds(const Rect & start, uint8_t edges, Rect result, const WindowOptions & options, const Rect & available) noexcept
        {
            if(options.sizeConstraint == nullptr)
            {
                return result;
            }

            Vec2 desired = {result.width, result.height};
            Vec2 currentSize = {start.width, start.height};
            options.sizeConstraint(&desired, currentSize, options.sizeConstraintUserData);
            float minimumWidth = std::min(std::max(0.f, options.minimumSize.x), available.width);
            float minimumHeight = std::min(std::max(0.f, options.minimumSize.y), available.height);
            float maximumWidth = std::max(minimumWidth, std::min(options.maximumSize.x, available.width));
            float maximumHeight = std::max(minimumHeight, std::min(options.maximumSize.y, available.height));
            desired.x = std::clamp(desired.x, minimumWidth, maximumWidth);
            desired.y = std::clamp(desired.y, minimumHeight, maximumHeight);

            if((edges & WindowResizeLeft) != 0)
            {
                result.x = result.right() - desired.x;
            }

            result.width = desired.x;

            if((edges & WindowResizeTop) != 0)
            {
                result.y = result.bottom() - desired.y;
            }

            result.height = desired.y;
            auto returnedValue = constrainWindowBounds(result, available);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool windowConditionApplies(Condition condition, bool onceApplied, bool firstUse, bool appearing) noexcept
        {
            switch(condition)
            {
            case Condition::Always:
                return true;
            case Condition::Once:
                return onceApplied == false;
            case Condition::FirstUse:
                return firstUse;
            case Condition::Appearing:
                return appearing;
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool markWindowCondition(Condition condition, bool onceApplied) noexcept
        {
            if(condition == Condition::Once)
            {
                onceApplied = true;
            }

            return onceApplied;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    void setNextWindowPosition(Context * ui, const Vec2 & position, Condition condition) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->nextWindow.position = position;
        ui->nextWindow.positionCondition = condition;
        ui->nextWindow.positionPending = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setNextWindowSize(Context * ui, const Vec2 & size, Condition condition) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->nextWindow.size = size;
        ui->nextWindow.sizeCondition = condition;
        ui->nextWindow.sizePending = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setNextWindowContentSize(Context * ui, const Vec2 & size, Condition condition) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->nextWindow.contentSize = size;
        ui->nextWindow.contentSizeCondition = condition;
        ui->nextWindow.contentSizePending = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setNextWindowCollapsed(Context * ui, bool collapsed, Condition condition) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->nextWindow.collapsed = collapsed;
        ui->nextWindow.collapsedCondition = condition;
        ui->nextWindow.collapsedPending = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setNextWindowFocus(Context * ui) noexcept
    {
        if(ui != nullptr)
        {
            ui->nextWindow.focusPending = true;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope window(Context * ui, StringView label, const WindowOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::window(ui, {}, label, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    WindowScope window(Context * ui, const Key & key, StringView label, const WindowOptions & options, const SourceLocation & location)
    {
        Context::NextWindowData nextWindow = ui->nextWindow;
        ui->nextWindow = {};
        Id requestedId = combineId(RootId, ui->localId(Detail::NodeKind::Window, key, location, false));
        size_t existingIndex = ui->findFrameNodeIndex(requestedId);

        if(existingIndex < ui->nodes.size() && ui->nodes[existingIndex].kind == Detail::NodeKind::Window)
        {
            Context::Node & existingWindow = ui->nodes[existingIndex];
            uint64_t windowSubmission = ui->nextWindowSubmission++;
            (void)ui->addWindowFrameInstance(existingIndex, windowSubmission);
            bool previousNavigationBlocked = ui->currentNavigationBlocked;
            uint64_t token = ui->pushScope(existingIndex, ui->currentStyle, ui->currentDisabled);
            ui->currentWindow = existingWindow.id;
            ui->currentWindowSubmission = windowSubmission;
            ui->currentNavigationBlocked = previousNavigationBlocked || existingWindow.navigationBlocked;
            bool visible = existingWindow.visible == true && existingWindow.windowData().collapsed == false;

            return {ui, token, existingWindow.id, visible};
        }

        bool fitContentWidth = options.alwaysAutoResize == true || options.fitContentWidth == true;
        bool fitContentHeight = options.alwaysAutoResize == true || options.fitContentHeight == true;
        bool acceptsPointerInput = options.input == true && options.pointerInput == true;
        bool acceptsNavigationInputs = options.input == true && options.navigation == true && options.navigationInputs == true;
        bool acceptsNavigationFocus = options.input == true && options.navigation == true && options.navigationFocus == true;
        LayoutOptions layout;
        layout.orientation = options.scroll.contentOrientation;
        layout.width = fitContentWidth == true ? Dimension(SizeRule::Content) : Dimension::fixed(options.initialBounds.width);
        bool collapsed = options.collapsed != nullptr && *options.collapsed;
        layout.height = collapsed ? Dimension::fixed(ui->currentStyle->metrics.windowTitleHeight) : (fitContentHeight == true ? Dimension(SizeRule::Content) : Dimension::fixed(options.initialBounds.height));
        size_t node = ui->addNode(Detail::NodeKind::Window, key, label, layout, location, SemanticRole::Window);
        Context::Node & windowNode = ui->nodes[node];
        uint64_t windowSubmission = ui->nextWindowSubmission++;
        windowNode.windowSubmission = windowSubmission;
        windowNode.item = ui->addWindowFrameInstance(node, windowSubmission);
        Context::Persistent & persistentState = ui->state(windowNode);

        if(persistentState.windowData().settingsPolicyInitialized == false)
        {
            if(options.saveSettings == false)
            {
                persistentState.windowData().initialized = false;
                persistentState.windowData().bounds = {};
            }

            persistentState.windowData().settingsPolicyInitialized = true;
        }

        persistentState.windowData().saveSettings = options.saveSettings;
        bool firstUse = persistentState.windowData().initialized == false;
        bool appearing = persistentState.windowData().visibleLastFrame + 1 < ui->frame.number;

        if(persistentState.windowData().collapsedInitialized == false)
        {
            persistentState.windowData().collapsedValue = collapsed;
            persistentState.windowData().collapsedInitialized = true;
        }
        else if(options.collapsed != nullptr)
        {
            persistentState.windowData().collapsedValue = *options.collapsed;
        }

        if(nextWindow.collapsedPending == true)
        {
            bool conditionApplies = Detail::windowConditionApplies(nextWindow.collapsedCondition, persistentState.windowData().collapsedConditionApplied, firstUse, appearing == true);

            if(conditionApplies == true)
            {
                persistentState.windowData().collapsedValue = nextWindow.collapsed;

                if(options.collapsed != nullptr)
                {
                    *options.collapsed = nextWindow.collapsed;
                }

                persistentState.windowData().collapsedConditionApplied = Detail::markWindowCondition(nextWindow.collapsedCondition, persistentState.windowData().collapsedConditionApplied);
            }
        }

        collapsed = persistentState.windowData().collapsedValue;
        windowNode.layout.height = collapsed ? Dimension::fixed(ui->currentStyle->metrics.windowTitleHeight) : (fitContentHeight == true ? Dimension(SizeRule::Content) : Dimension::fixed(options.initialBounds.height));
        ui->windowLabels[windowNode.id] = String(label);
        windowNode.visible = options.open == nullptr || *options.open;
        windowNode.inputBlocked = windowNode.inputBlocked || options.input == false;
        windowNode.navigationBlocked = windowNode.navigationBlocked || acceptsNavigationInputs == false;
        windowNode.mutableWindowData().titleVisible = options.titleBar;
        windowNode.mutableWindowData().menuBar = options.menuBar;
        windowNode.mutableWindowData().backgroundVisible = options.background;
        windowNode.mutableWindowData().backgroundAlpha = std::clamp(options.backgroundAlpha, 0.f, 1.f);
        windowNode.mutableWindowData().unsavedDocument = options.unsavedDocument;

        if(options.unsavedDocument == true)
        {
            ui->prepareValueText(windowNode, "*");
        }

        if(options.padding == false)
        {
            windowNode.layout.padding = {};
        }

        uint32_t dockGroup = options.dockable && ui->configuration.dockingEnabled ? options.dockGroup : 0;
        bool forcedUndock = false;

        if(dockGroup != 1 && ui->docking.nodeForWindow(windowNode.id) != 0)
        {
            forcedUndock = ui->docking.undock(windowNode.id) || forcedUndock;
        }

        for(auto & [group, model] : ui->dockModels)
        {
            if(group != dockGroup && model.nodeForWindow(windowNode.id) != 0)
            {
                forcedUndock = model.undock(windowNode.id) || forcedUndock;
            }
        }
        auto previousDockGroup = ui->windowDockGroups.find(windowNode.id);

        if(previousDockGroup != ui->windowDockGroups.end() && previousDockGroup->second != dockGroup)
        {
            DockModel * previousModel = Detail::dockModel(ui, previousDockGroup->second, false);

            if(previousModel != nullptr)
            {
                forcedUndock = previousModel->undock(windowNode.id) || forcedUndock;
            }
        }

        ui->windowDockGroups[windowNode.id] = dockGroup;
        windowNode.mutableWindowData().dockGroup = dockGroup;
        DockModel * dockModel = Detail::dockModel(ui, dockGroup, dockGroup != 0);
        const DockSpaceOptions * dockSpace = Detail::dockSpaceOptions(ui, dockGroup);
        bool dockSpaceNoResize = dockSpace != nullptr && dockSpace->noResize;
        bool dockSpaceAutoHideTabBar = dockSpace != nullptr && dockSpace->autoHideTabBar;
        DockNodeId dockNodeId = dockModel == nullptr ? 0 : dockModel->nodeForWindow(windowNode.id);
        const DockNode * dockNode = dockModel == nullptr ? nullptr : dockModel->node(dockNodeId);
        bool dockActive = dockNode == nullptr || dockNode->activeTab == windowNode.id;
        windowNode.mutableWindowData().dockNode = dockNodeId;
        windowNode.mutableWindowData().docked = dockNodeId != 0 || (dockGroup != 0 && options.movable == false && options.resizable == false);
        windowNode.mutableWindowData().dockAutoHideTabBar = options.dockAutoHideTabBar || dockSpaceAutoHideTabBar;
        windowNode.mutableWindowData().collapsed = collapsed;
        windowNode.mutableWindowData().closeVisible = options.titleBar && options.open != nullptr && (windowNode.mutableWindowData().docked == false || windowNode.style->metrics.dockingNodeHasCloseButton == true);
        WindowCollapsePlacement collapsePlacement = options.collapsePlacement;
        bool styleCollapseVisible = true;

        if(collapsePlacement == WindowCollapsePlacement::Style)
        {
            WindowMenuButtonPosition position = windowNode.style->metrics.windowMenuButtonPosition;
            styleCollapseVisible = position != WindowMenuButtonPosition::None;
            collapsePlacement = position == WindowMenuButtonPosition::Left ? WindowCollapsePlacement::Left : WindowCollapsePlacement::Right;
        }

        windowNode.mutableWindowData().collapseVisible = options.titleBar && options.collapsed != nullptr && styleCollapseVisible == true;
        windowNode.mutableWindowData().collapsePlacement = collapsePlacement;
        windowNode.mutableWindowData().minimumSize = options.minimumSize;
        windowNode.mutableWindowData().maximumSize = options.maximumSize;
        windowNode.mutableWindowData().alwaysAutoResize = options.alwaysAutoResize;
        windowNode.mutableWindowData().fitContentWidth = fitContentWidth;
        windowNode.mutableWindowData().fitContentHeight = fitContentHeight;
        windowNode.mutableWindowData().scrollable = options.scrollable;
        windowNode.mutableScrollOptions() = options.scroll;
        windowNode.mutableWindowData().bringToFront = options.bringToFront;
        const PointerState * pointer = ui->input.primaryPointer();

        if(persistentState.windowData().zOrder == 0)
        {
            persistentState.windowData().zOrder = ui->nextWindowZOrder++;
        }

        persistentState.windowData().visible = windowNode.visible && dockActive;
        persistentState.windowData().acceptsInput = options.input;
        persistentState.windowData().acceptsPointerInput = acceptsPointerInput;
        persistentState.windowData().acceptsNavigationFocus = acceptsNavigationFocus;
        persistentState.windowData().bringToFront = options.bringToFront;
        persistentState.windowData().popup = windowNode.mutableWindowData().popup;
        persistentState.windowOwner = windowNode.id;
        windowNode.windowOwner = windowNode.id;
        ui->visibleWindowIds.push_back(windowNode.id);

        if(fitContentWidth == false)
        {
            persistentState.windowData().contentWidthFitted = false;
        }

        if(fitContentHeight == false)
        {
            persistentState.windowData().contentHeightFitted = false;
        }

        if(pointer != nullptr && pointer->isPressed() == true && ui->pointerWindow == windowNode.id && options.bringToFront == true)
        {
            persistentState.windowData().zOrder = ui->nextWindowZOrder++;
        }

        windowNode.mutableWindowData().zOrder = persistentState.windowData().zOrder;

        if(forcedUndock == true && persistentState.lastBounds.empty() == false)
        {
            persistentState.windowData().bounds = persistentState.lastBounds;
            persistentState.windowData().initialized = true;
        }

        if(persistentState.windowData().initialized == false || (options.movable == false && options.resizable == false))
        {
            persistentState.windowData().bounds = options.initialBounds;
            persistentState.windowData().initialized = true;
        }

        if(nextWindow.positionPending == true)
        {
            bool conditionApplies = Detail::windowConditionApplies(nextWindow.positionCondition, persistentState.windowData().positionConditionApplied, firstUse, appearing == true);

            if(conditionApplies == true)
            {
                persistentState.windowData().bounds.x = nextWindow.position.x;
                persistentState.windowData().bounds.y = nextWindow.position.y;
                persistentState.windowData().positionConditionApplied = Detail::markWindowCondition(nextWindow.positionCondition, persistentState.windowData().positionConditionApplied);
            }
        }

        if(nextWindow.sizePending == true)
        {
            bool conditionApplies = Detail::windowConditionApplies(nextWindow.sizeCondition, persistentState.windowData().sizeConditionApplied, firstUse, appearing == true);

            if(conditionApplies == true)
            {
                if(nextWindow.size.x > 0.f)
                {
                    persistentState.windowData().bounds.width = nextWindow.size.x;
                }

                if(nextWindow.size.y > 0.f)
                {
                    persistentState.windowData().bounds.height = nextWindow.size.y;
                }

                persistentState.windowData().sizeConditionApplied = Detail::markWindowCondition(nextWindow.sizeCondition, persistentState.windowData().sizeConditionApplied);
            }
        }

        if(nextWindow.contentSizePending == true)
        {
            bool conditionApplies = Detail::windowConditionApplies(nextWindow.contentSizeCondition, persistentState.windowData().contentSizeConditionApplied, firstUse, appearing == true);

            if(conditionApplies == true)
            {
                windowNode.mutableWindowData().contentSizeExplicit = true;
                windowNode.mutableWindowData().contentSize = {std::max(0.f, nextWindow.contentSize.x), std::max(0.f, nextWindow.contentSize.y)};
                persistentState.windowData().contentSizeConditionApplied = Detail::markWindowCondition(nextWindow.contentSizeCondition, persistentState.windowData().contentSizeConditionApplied);
            }
        }

        if((nextWindow.focusPending == true || (options.focusOnAppearing == true && appearing == true)) && windowNode.visible == true && dockActive == true)
        {
            persistentState.windowData().zOrder = ui->nextWindowZOrder++;
            ui->focused = windowNode.id;
            ui->pointerFocused = windowNode.id;

            if(acceptsNavigationFocus == true)
            {
                ui->navigationFocused = windowNode.id;
            }
            windowNode.mutableWindowData().zOrder = persistentState.windowData().zOrder;
        }

        Rect available = ui->viewport.workArea.empty() == true ? ui->viewport.bounds : ui->viewport.workArea;
        Rect windowAvailable = available;

        if(dockNodeId == 0)
        {
            float horizontalPadding = std::clamp(windowNode.style->metrics.displayWindowPadding.x, 0.f, available.width * 0.5f);
            float verticalPadding = std::clamp(windowNode.style->metrics.displayWindowPadding.y, 0.f, available.height * 0.5f);
            windowAvailable.x += horizontalPadding;
            windowAvailable.y += verticalPadding;
            windowAvailable.width = std::max(0.f, windowAvailable.width - horizontalPadding * 2.f);
            windowAvailable.height = std::max(0.f, windowAvailable.height - verticalPadding * 2.f);
        }

        persistentState.windowData().bounds = Detail::resizedWindowBounds(persistentState.windowData().bounds, static_cast<uint8_t>(Detail::WindowResizeRight | Detail::WindowResizeBottom), {}, options.minimumSize, options.maximumSize, available);
        persistentState.windowData().bounds = Detail::constrainResizedWindowBounds(persistentState.windowData().bounds, static_cast<uint8_t>(Detail::WindowResizeRight | Detail::WindowResizeBottom), persistentState.windowData().bounds, options, available);
        persistentState.windowData().bounds = Detail::constrainWindowBounds(persistentState.windowData().bounds, available);

        if(windowNode.visible == true && dockActive == true)
        {
            persistentState.windowData().visibleLastFrame = ui->frame.number;
        }

        Rect dockHostBounds;
        bool hasDockHostBounds = dockNodeId != 0 && Detail::dockArea(ui, dockGroup, &dockHostBounds);

        if(dockNodeId != 0 && (hasDockHostBounds == false || dockHostBounds.empty() == true))
        {
            dockHostBounds = available;
        }

        Rect previousTitle = Detail::windowTitleBarBounds(windowNode, persistentState.lastBounds);
        float buttonSide = std::max(12.f, windowNode.style->metrics.windowTitleHeight - 8.f);
        float buttonRight = previousTitle.right() - 4.f;
        Rect closeButton;
        Rect collapseButton;

        if(windowNode.mutableWindowData().closeVisible == true)
        {
            closeButton = {buttonRight - buttonSide, previousTitle.y + (previousTitle.height - buttonSide) * 0.5f, buttonSide, buttonSide};
            buttonRight = closeButton.x - 2.f;
        }

        if(windowNode.mutableWindowData().collapseVisible == true)
        {
            collapseButton = windowNode.mutableWindowData().collapsePlacement == WindowCollapsePlacement::Left ? Rect{previousTitle.x + 4.f, previousTitle.y + (previousTitle.height - buttonSide) * 0.5f, buttonSide, buttonSide} : Rect{buttonRight - buttonSide, previousTitle.y + (previousTitle.height - buttonSide) * 0.5f, buttonSide, buttonSide};
        }

        bool dockHostCornerOwner = dockNodeId != 0 && std::abs(persistentState.lastBounds.right() - dockHostBounds.right()) <= 1.f && std::abs(persistentState.lastBounds.bottom() - dockHostBounds.bottom()) <= 1.f;
        Rect resizeBounds = dockNodeId != 0 ? dockHostBounds : persistentState.lastBounds;
        float resizeHitThickness = std::max(1.f, windowNode.style->metrics.windowBorderSize + windowNode.style->metrics.windowBorderHoverPadding);
        bool canPoint = windowNode.visible && dockActive == true && acceptsPointerInput == true && windowNode.disabled == false && windowNode.inputBlocked == false && Detail::inputLayerBlocked(ui, windowNode) == false && pointer != nullptr && (ui->pointerWindow == InvalidId || ui->pointerWindow == windowNode.id || ui->captured == windowNode.id) && (ui->captured == InvalidId || ui->captured == windowNode.id);
        Detail::WindowScrollResult scrollInteraction = Detail::windowScrollBehavior(ui, windowNode, persistentState, canPoint);
        uint8_t resizeEdges = pointer == nullptr || scrollInteraction.hovered == true || scrollInteraction.active ? static_cast<uint8_t>(Detail::WindowResizeNone) : Detail::windowResizeEdgesAt(resizeBounds, pointer->position, resizeHitThickness);

        if(options.resizeHorizontal == false)
        {
            resizeEdges = static_cast<uint8_t>(resizeEdges & ~(Detail::WindowResizeLeft | Detail::WindowResizeRight));
        }

        if(options.resizeVertical == false)
        {
            resizeEdges = static_cast<uint8_t>(resizeEdges & ~(Detail::WindowResizeTop | Detail::WindowResizeBottom));
        }

        if(ui->configuration.windowResizeFromEdges == false && resizeEdges != Detail::WindowResizeNone)
        {
            bool bottomRight = (resizeEdges & Detail::WindowResizeRight) != 0 && (resizeEdges & Detail::WindowResizeBottom) != 0;
            resizeEdges = bottomRight ? static_cast<uint8_t>(Detail::WindowResizeRight | Detail::WindowResizeBottom) : static_cast<uint8_t>(Detail::WindowResizeNone);
        }

        bool dockHostPointerOwner = dockNodeId == 0;

        if(dockNodeId != 0 && pointer != nullptr && resizeEdges != 0 && persistentState.lastBounds.empty() == false)
        {
            Vec2 ownerPoint = {std::clamp(pointer->position.x, dockHostBounds.x + 0.5f, dockHostBounds.right() - 0.5f), std::clamp(pointer->position.y, dockHostBounds.y + 0.5f, dockHostBounds.bottom() - 0.5f)};
            dockHostPointerOwner = persistentState.lastBounds.contains(ownerPoint);
        }

        if(dockNodeId != 0 && dockHostPointerOwner == false)
        {
            resizeEdges = Detail::WindowResizeNone;
        }

        bool dockTabPressed = false;
        bool autoHideDockTabBar = options.dockAutoHideTabBar == true || dockSpaceAutoHideTabBar == true;
        bool dockTabBarVisible = dockNode != nullptr && dockNode->tabs.empty() == false && (autoHideDockTabBar == false || dockNode->tabs.size() > 1);

        if(canPoint == true && resizeEdges == Detail::WindowResizeNone && dockTabBarVisible == true && pointer->isPressed() == true)
        {
            for(size_t tabIndex = 0; tabIndex != dockNode->tabs.size(); ++tabIndex)
            {
                Id tab = dockNode->tabs[tabIndex];
                Rect tabBounds;
                if(Detail::dockTabBounds(ui, windowNode, *dockNode, tabIndex, previousTitle, &tabBounds) == false)
                {
                    break;
                }

                if(tabBounds.contains(pointer->position) == true)
                {
                    dockTabPressed = true;
                    (void)dockModel->activate(tab);
                    ui->focused = tab;

                    if(acceptsNavigationInputs == true)
                    {
                        ui->navigationFocused = tab;
                    }

                    if(options.movable == true)
                    {
                        ui->captured = tab;
                        ui->capturedPointer = pointer->id;
                        ui->dockingTabDragWindow = tab;
                        ui->dockingTabDragNode = dockNode->id;
                        ui->dockingTabDragIndex = tabIndex;
                        ui->dockingTabDragStart = pointer->position;
                    }

                    break;
                }

            }
        }

        if(pointer != nullptr && ui->dockingTabDragWindow == windowNode.id && ui->captured == windowNode.id)
        {
            if(pointer->isDown() == true)
            {
                Vec2 dragDistance = pointer->position - ui->dockingTabDragStart;
                bool leaveTabBar = std::abs(dragDistance.y) > windowNode.style->metrics.windowTitleHeight * 0.65f;

                if(leaveTabBar == true && dockGroup != 0 && ui->configuration.dockingNoUndocking == false && (dockSpace == nullptr || dockSpace->noUndocking == false))
                {
                    Rect sourceBounds = persistentState.lastBounds;

                    if(dockModel != nullptr && dockModel->undock(windowNode.id) == true)
                    {
                        windowNode.mutableWindowData().dockNode = 0;
                        windowNode.mutableWindowData().docked = false;
                        ui->dockingDragWindow = windowNode.id;
                        ui->dockingTabDragWindow = InvalidId;
                        ui->dockingTabDragNode = 0;
                        persistentState.windowData().interaction = 3;
                        persistentState.windowData().dragging = true;
                        persistentState.windowData().bounds = sourceBounds;
                        persistentState.windowData().initialized = true;
                        persistentState.windowData().dragOffset = pointer->position - Vec2{sourceBounds.x, sourceBounds.y};
                    }
                }
                else if(std::abs(dragDistance.x) >= 5.f)
                {
                    const DockNode * currentTabs = dockModel == nullptr ? nullptr : dockModel->node(ui->dockingTabDragNode);

                    if(currentTabs != nullptr && currentTabs->type == DockNodeType::Tabs)
                    {
                        size_t targetIndex = ui->dockingTabDragIndex;
                        for(size_t index = 0; index != currentTabs->tabs.size(); ++index)
                        {
                            Rect tabBounds;
                            if(Detail::dockTabBounds(ui, windowNode, *currentTabs, index, previousTitle, &tabBounds) == false)
                            {
                                break;
                            }

                            if(pointer->position.x < tabBounds.x + tabBounds.width * 0.5f)
                            {
                                targetIndex = index;
                                break;
                            }

                            targetIndex = index;
                        }

                        if(targetIndex != ui->dockingTabDragIndex && dockModel->reorder(windowNode.id, targetIndex) == true)
                        {
                            ui->dockingTabDragIndex = targetIndex;
                            ui->dockingTabDragStart = pointer->position;
                        }
                    }
                }
            }

            if(pointer->isReleased() == true && ui->dockingTabDragWindow == windowNode.id)
            {
                ui->dockingTabDragWindow = InvalidId;
                ui->dockingTabDragNode = 0;
                ui->captured = InvalidId;
                ui->capturedItem = {};
                ui->capturedPointer = 0;
            }
        }

        windowNode.mutableWindowData().closeHovered = canPoint && closeButton.contains(pointer->position);
        windowNode.mutableWindowData().collapseHovered = canPoint && collapseButton.contains(pointer->position);
        windowNode.mutableWindowData().resizeHovered = canPoint && options.resizable == true && dockSpaceNoResize == false && resizeEdges != Detail::WindowResizeNone;
        bool activeResize = ui->captured == windowNode.id && persistentState.windowData().interaction == 6;
        windowNode.mutableWindowData().resizeEdges = activeResize ? persistentState.windowData().resizeEdges : resizeEdges;
        windowNode.mutableWindowData().resizable = options.resizable && dockSpaceNoResize == false && (dockNodeId == 0 || dockHostCornerOwner == true || dockHostPointerOwner == true || persistentState.windowData().resizingDockHost == true);
        windowNode.mutableWindowData().resizeGripVisible = options.resizable && dockSpaceNoResize == false && (dockNodeId == 0 || dockHostCornerOwner == true);
        windowNode.mutableWindowData().resizeBounds = resizeBounds;

        bool autoFitResize = false;
        bool requestAutoFit = scrollInteraction.consumedPress == false;

        if(canPoint == false)
        {
            requestAutoFit = false;
        }

        if(windowNode.mutableWindowData().resizeHovered == false)
        {
            requestAutoFit = false;
        }

        if(pointer == nullptr)
        {
            requestAutoFit = false;
        }

        if(requestAutoFit == true)
        {
            requestAutoFit = pointer->isPressed();
        }

        if(requestAutoFit == true)
        {
            if(pointer->buttonClickCount() < 2)
            {
                requestAutoFit = false;
            }
        }

        if(persistentState.scrollData().contentSize.x <= 0.f)
        {
            if(persistentState.scrollData().contentSize.y <= 0.f)
            {
                requestAutoFit = false;
            }
        }

        if(requestAutoFit == true)
        {
            Rect fitted = persistentState.windowData().bounds;
            float verticalScrollbar = persistentState.scrollData().range.y > 0.f ? windowNode.style->metrics.scrollbarWidth + windowNode.style->metrics.gap : 0.f;
            float horizontalScrollbar = persistentState.scrollData().range.x > 0.f ? windowNode.style->metrics.scrollbarWidth + windowNode.style->metrics.gap : 0.f;
            float desiredWidth = persistentState.scrollData().contentSize.x + windowNode.layout.padding.left + windowNode.layout.padding.right + verticalScrollbar;
            float desiredHeight = persistentState.scrollData().contentSize.y + windowNode.layout.padding.top + windowNode.layout.padding.bottom + horizontalScrollbar + (windowNode.mutableWindowData().titleVisible ? windowNode.style->metrics.windowTitleHeight : 0.f);

            if((resizeEdges & (Detail::WindowResizeLeft | Detail::WindowResizeRight)) != 0)
            {
                float previousRight = fitted.right();
                fitted.width = desiredWidth;

                if((resizeEdges & Detail::WindowResizeLeft) != 0)
                {
                    fitted.x = previousRight - fitted.width;
                }
            }

            if((resizeEdges & (Detail::WindowResizeTop | Detail::WindowResizeBottom)) != 0)
            {
                float previousBottom = fitted.bottom();
                fitted.height = desiredHeight;

                if((resizeEdges & Detail::WindowResizeTop) != 0)
                {
                    fitted.y = previousBottom - fitted.height;
                }
            }

            persistentState.windowData().bounds = Detail::resizedWindowBounds(fitted, resizeEdges, {}, options.minimumSize, options.maximumSize, available);
            persistentState.windowData().bounds = Detail::constrainWindowBounds(persistentState.windowData().bounds, available);
            autoFitResize = true;
        }

        if(persistentState.windowData().backgroundMovePending == true)
        {
            bool cancelBackgroundMove = ui->configuration.windowMoveFromTitleBarOnly;

            if(options.movable == false)
            {
                cancelBackgroundMove = true;
            }

            if(dockNodeId != 0)
            {
                cancelBackgroundMove = true;
            }

            if(pointer == nullptr)
            {
                cancelBackgroundMove = true;
            }

            if(pointer != nullptr)
            {
                if(pointer->isDown() == false)
                {
                    cancelBackgroundMove = true;
                }
            }

            if(ui->captured != InvalidId)
            {
                if(ui->captured != windowNode.id)
                {
                    cancelBackgroundMove = true;
                }
            }

            if(cancelBackgroundMove == true)
            {
                persistentState.windowData().backgroundMovePending = false;
            }
            else
            {
                Vec2 movement = pointer->position - pointer->pressPosition(PointerButton::Primary);
                float threshold = windowNode.style->behavior.dragThreshold;

                if(movement.x * movement.x + movement.y * movement.y >= threshold * threshold)
                {
                    ui->captured = windowNode.id;
                    ui->capturedPointer = pointer->id;
                    ui->active = windowNode.id;
                    ui->focused = windowNode.id;
                    persistentState.windowData().dragging = true;
                    persistentState.windowData().interaction = 3;
                    persistentState.windowData().dragOffset = pointer->pressPosition(PointerButton::Primary) - Vec2{persistentState.windowData().bounds.x, persistentState.windowData().bounds.y};
                    persistentState.windowData().backgroundMovePending = false;

                    if(dockGroup != 0)
                    {
                        ui->dockingDragWindow = windowNode.id;
                    }
                }
            }
        }

        bool actionPress = scrollInteraction.consumedPress == false;

        if(canPoint == false)
        {
            actionPress = false;
        }

        if(pointer == nullptr)
        {
            actionPress = false;
        }

        if(actionPress == true)
        {
            actionPress = pointer->isPressed();
        }

        bool beginResize = actionPress;

        if(autoFitResize == true)
        {
            beginResize = false;
        }

        if(windowNode.mutableWindowData().resizeHovered == false)
        {
            beginResize = false;
        }

        bool pressClose = actionPress;

        if(windowNode.mutableWindowData().closeHovered == false)
        {
            pressClose = false;
        }

        bool pressCollapse = actionPress;

        if(windowNode.mutableWindowData().collapseHovered == false)
        {
            pressCollapse = false;
        }

        bool doubleClickTitle = canPoint;

        if(pointer == nullptr)
        {
            doubleClickTitle = false;
        }

        if(doubleClickTitle == true)
        {
            doubleClickTitle = pointer->isPressed();
        }

        if(doubleClickTitle == true)
        {
            if(pointer->buttonClickCount() < 2)
            {
                doubleClickTitle = false;
            }
        }

        if(options.titleBar == false)
        {
            doubleClickTitle = false;
        }

        if(options.collapsed == nullptr)
        {
            doubleClickTitle = false;
        }

        if(doubleClickTitle == true)
        {
            doubleClickTitle = previousTitle.contains(pointer->position);
        }

        if(dockTabPressed == true)
        {
            doubleClickTitle = false;
        }

        bool detachDockTab = actionPress;

        if(options.movable == false)
        {
            detachDockTab = false;
        }

        if(dockGroup == 0)
        {
            detachDockTab = false;
        }

        if(dockNodeId == 0)
        {
            detachDockTab = false;
        }

        if(detachDockTab == true)
        {
            detachDockTab = previousTitle.contains(pointer->position);
        }

        if(dockTabPressed == true)
        {
            detachDockTab = false;
        }

        bool movableTitle = options.movable;

        if(dockGroup != 0)
        {
            if(dockNodeId == 0)
            {
                movableTitle = true;
            }
        }

        bool moveFromTitle = actionPress;

        if(movableTitle == false)
        {
            moveFromTitle = false;
        }

        if(options.titleBar == false)
        {
            moveFromTitle = false;
        }

        if(moveFromTitle == true)
        {
            moveFromTitle = previousTitle.contains(pointer->position);
        }

        bool prepareBackgroundMove = actionPress;

        if(ui->configuration.windowMoveFromTitleBarOnly == true)
        {
            prepareBackgroundMove = false;
        }

        if(movableTitle == false)
        {
            prepareBackgroundMove = false;
        }

        if(dockNodeId != 0)
        {
            prepareBackgroundMove = false;
        }

        if(prepareBackgroundMove == true)
        {
            prepareBackgroundMove = persistentState.lastBounds.contains(pointer->position);
        }

        if(beginResize == true)
        {
            ui->captured = windowNode.id;
            ui->capturedPointer = pointer->id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.windowData().interaction = 6;
            persistentState.dragStartPosition = pointer->position;
            persistentState.windowData().resizeStartBounds = resizeBounds;
            persistentState.windowData().resizeEdges = resizeEdges;
            persistentState.windowData().resizingDockHost = dockNodeId != 0;
        }
        else if(pressClose == true)
        {
            ui->captured = windowNode.id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.windowData().interaction = 2;
        }
        else if(pressCollapse == true)
        {
            ui->captured = windowNode.id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.windowData().interaction = 1;
        }
        else if(doubleClickTitle == true)
        {
            *options.collapsed = !*options.collapsed;
            windowNode.mutableWindowData().collapsed = *options.collapsed;
            windowNode.layout.height = Dimension::fixed(*options.collapsed ? windowNode.style->metrics.windowTitleHeight : options.initialBounds.height);
            persistentState.windowData().dragging = false;
            persistentState.windowData().interaction = 0;
            ui->focused = windowNode.id;
            ui->frame.events.push_back({EventType::Change, windowNode.id, ui->nodePath(windowNode), windowNode.debugData().file, windowNode.debugData().line, ui->input.timestamp});
        }
        else if(detachDockTab == true)
        {
            ui->captured = windowNode.id;
            ui->capturedPointer = pointer->id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.windowData().interaction = 5;
            persistentState.windowData().dragging = true;
            persistentState.windowData().dragOffset = pointer->position - Vec2{dockHostBounds.x, dockHostBounds.y};
        }
        else if(moveFromTitle == true)
        {
            ui->captured = windowNode.id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.windowData().dragging = true;
            persistentState.windowData().interaction = 3;
            persistentState.windowData().dragOffset = pointer->position - Vec2{persistentState.windowData().bounds.x, persistentState.windowData().bounds.y};

            if(dockGroup != 0)
            {
                ui->dockingDragWindow = windowNode.id;
            }
        }
        else if(prepareBackgroundMove == true)
        {
            persistentState.windowData().backgroundMovePending = true;
        }

        if(pointer != nullptr && ui->captured == windowNode.id && persistentState.windowData().interaction == 5 && persistentState.windowData().dragging == true && pointer->isDown() == true)
        {
            dockHostBounds.x = pointer->position.x - persistentState.windowData().dragOffset.x;
            dockHostBounds.y = pointer->position.y - persistentState.windowData().dragOffset.y;
            dockHostBounds = Detail::constrainWindowBounds(dockHostBounds, available);
            Detail::setDockArea(ui, dockGroup, dockHostBounds);
        }

        if(pointer != nullptr && ui->captured == windowNode.id && persistentState.windowData().interaction == 3 && persistentState.windowData().dragging == true && pointer->isDown() == true)
        {
            persistentState.windowData().bounds.x = pointer->position.x - persistentState.windowData().dragOffset.x;
            persistentState.windowData().bounds.y = pointer->position.y - persistentState.windowData().dragOffset.y;
            persistentState.windowData().bounds = Detail::constrainWindowBounds(persistentState.windowData().bounds, windowAvailable);
            ui->dockingPreviewNode = 0;
            ui->dockingPreviewTargetWindow = InvalidId;
            ui->dockingTargetBounds = {};
            ui->dockingPreviewBounds = {};

            if(dockGroup != 0 && dockModel != nullptr && ui->dockingDragWindow == windowNode.id)
            {
                DockNodeId targetNode = 0;
                Id targetWindow = InvalidId;
                Rect targetBounds;
                for(const DockLayoutEntry & entry : ui->dockLayout)
                {
                    if(entry.group != dockGroup)
                    {
                        continue;
                    }

                    if(entry.window == windowNode.id)
                    {
                        continue;
                    }

                    if(entry.active == false)
                    {
                        continue;
                    }

                    if(entry.bounds.contains(pointer->position) == false)
                    {
                        continue;
                    }

                    if(dockModel->node(entry.node) == nullptr)
                    {
                        continue;
                    }

                    targetNode = entry.node;
                    targetWindow = entry.window;
                    targetBounds = entry.bounds;
                    break;
                }

                const DockNode * rootNode = dockModel->node(dockModel->root());
                bool emptyModel = rootNode != nullptr && rootNode->type == DockNodeType::Tabs && rootNode->tabs.empty() == true;

                if(targetBounds.empty() == true && emptyModel == true)
                {
                    for(auto candidate = ui->nodes.rbegin(); candidate != ui->nodes.rend(); ++candidate)
                    {
                        if(candidate->kind != Detail::NodeKind::Window)
                        {
                            continue;
                        }

                        if(candidate->id == windowNode.id)
                        {
                            continue;
                        }

                        if(candidate->visible == false)
                        {
                            continue;
                        }

                        if(candidate->windowData().dockGroup != dockGroup)
                        {
                            continue;
                        }

                        if(dockModel->nodeForWindow(candidate->id) != 0)
                        {
                            continue;
                        }

                        const Context::Persistent * targetState = ui->findState(candidate->id);

                        if(targetState == nullptr)
                        {
                            continue;
                        }

                        if(targetState->lastBounds.contains(pointer->position) == false)
                        {
                            continue;
                        }

                        targetWindow = candidate->id;
                        targetBounds = targetState->lastBounds;
                        break;
                    }
                }

                if(targetBounds.empty() == true && emptyModel == true)
                {
                    for(const auto & [candidateId, candidateGroup] : ui->windowDockGroups)
                    {
                        if(candidateId == windowNode.id)
                        {
                            continue;
                        }

                        if(candidateGroup != dockGroup)
                        {
                            continue;
                        }

                        if(dockModel->nodeForWindow(candidateId) != 0)
                        {
                            continue;
                        }

                        const Context::Persistent * targetState = ui->findState(candidateId);

                        if(targetState == nullptr)
                        {
                            continue;
                        }

                        if(targetState->lastFrame + 1 < ui->frame.number)
                        {
                            continue;
                        }

                        if(targetState->lastBounds.contains(pointer->position) == false)
                        {
                            continue;
                        }

                        targetWindow = candidateId;
                        targetBounds = targetState->lastBounds;
                        break;
                    }
                }

                if(targetBounds.empty() == false)
                {
                    ui->dockingTargetBounds = targetBounds;
                    Detail::DockTargetRectArray targets = Detail::dockTargetRects(targetBounds);
                    DockPlacement placement = DockPlacement::Center;
                    bool dockingModifier = ui->configuration.dockingWithShift == false || ui->input.modifiers.shift == true;
                    bool noSplit = ui->configuration.dockingNoSplit == true || (dockSpace != nullptr && dockSpace->noSplit == true);
                    bool noMerge = ui->configuration.dockingNoDockingOver == true;
                    noMerge = noMerge == true || ui->configuration.dockingNoMerge == true;
                    noMerge = noMerge == true || (dockSpace != nullptr && dockSpace->noMerge == true);
                    bool centralBlocked = dockSpace != nullptr && dockSpace->noDockingOverCentralNode == true && dockModel != nullptr && targetNode == dockModel->centralNode();
                    bool placementFound = Detail::dockPlacementAt(targets, pointer->position, &placement);
                    bool splitBlocked = noSplit == true && placement != DockPlacement::Center;
                    bool mergeBlocked = noMerge == true && placement == DockPlacement::Center;
                    bool centralMergeBlocked = centralBlocked == true && placement == DockPlacement::Center;

                    if(dockingModifier == true && placementFound == true && splitBlocked == false && mergeBlocked == false && centralMergeBlocked == false)
                    {
                        ui->dockingPreviewNode = targetNode;
                        ui->dockingPreviewTargetWindow = targetWindow;
                        ui->dockingPreviewPlacement = placement;
                        ui->dockingPreviewBounds = Detail::dockPreviewBounds(targetBounds, placement);
                    }
                }
            }
        }

        if(pointer != nullptr && ui->captured == windowNode.id && persistentState.windowData().interaction == 6 && pointer->isDown() == true)
        {
            Vec2 delta = pointer->position - persistentState.dragStartPosition;
            Rect resizeAvailable = persistentState.windowData().resizingDockHost == true ? available : windowAvailable;
            Rect resizedBounds = Detail::resizedWindowBounds(persistentState.windowData().resizeStartBounds, persistentState.windowData().resizeEdges, delta, options.minimumSize, options.maximumSize, resizeAvailable);
            resizedBounds = Detail::constrainResizedWindowBounds(persistentState.windowData().resizeStartBounds, persistentState.windowData().resizeEdges, resizedBounds, options, resizeAvailable);

            if(persistentState.windowData().resizingDockHost == true)
            {
                Detail::setDockArea(ui, dockGroup, resizedBounds);
            }
            else
            {
                persistentState.windowData().bounds = resizedBounds;
            }
        }

        if(pointer != nullptr && ui->captured == windowNode.id && pointer->isReleased() == true)
        {
            bool completeDocking = persistentState.windowData().interaction == 3;

            if(ui->dockingDragWindow != windowNode.id)
            {
                completeDocking = false;
            }

            if(dockModel == nullptr)
            {
                completeDocking = false;
            }

            if(ui->dockingPreviewNode == 0)
            {
                if(ui->dockingPreviewTargetWindow == InvalidId)
                {
                    completeDocking = false;
                }
            }

            if(persistentState.windowData().interaction == 1 && options.collapsed != nullptr && collapseButton.contains(pointer->position) == true)
            {
                *options.collapsed = !*options.collapsed;
                windowNode.mutableWindowData().collapsed = *options.collapsed;
                windowNode.layout.height = Dimension::fixed(*options.collapsed ? windowNode.style->metrics.windowTitleHeight : options.initialBounds.height);
                ui->frame.events.push_back({EventType::Change, windowNode.id, ui->nodePath(windowNode), windowNode.debugData().file, windowNode.debugData().line, ui->input.timestamp});
            }
            else if(persistentState.windowData().interaction == 2 && options.open != nullptr && closeButton.contains(pointer->position) == true)
            {
                *options.open = false;
                windowNode.visible = false;
                ui->frame.events.push_back({EventType::Change, windowNode.id, ui->nodePath(windowNode), windowNode.debugData().file, windowNode.debugData().line, ui->input.timestamp});
            }
            else if(completeDocking == true)
            {
                DockNodeId targetNode = ui->dockingPreviewNode;

                if(targetNode == 0)
                {
                    const DockNode * rootNode = dockModel->node(dockModel->root());

                    if(rootNode != nullptr && rootNode->type == DockNodeType::Tabs && rootNode->tabs.empty() == true)
                    {
                        Detail::setDockArea(ui, dockGroup, ui->dockingTargetBounds);
                        (void)dockModel->dock(ui->dockingPreviewTargetWindow, dockModel->root(), DockPlacement::Center);
                        targetNode = dockModel->nodeForWindow(ui->dockingPreviewTargetWindow);
                    }
                }

                if(targetNode != 0)
                {
                    (void)dockModel->dock(windowNode.id, targetNode, ui->dockingPreviewPlacement, 0.5f);
                }
            }

            persistentState.windowData().dragging = false;
            persistentState.scrollData().draggingScrollbar = false;
            persistentState.scrollData().draggingAxis = 0;
            persistentState.windowData().resizingDockHost = false;
            persistentState.windowData().resizeEdges = Detail::WindowResizeNone;
            persistentState.windowData().interaction = 0;

            if(ui->dockingDragWindow == windowNode.id)
            {
                ui->dockingDragWindow = InvalidId;
                ui->dockingPreviewNode = 0;
                ui->dockingPreviewTargetWindow = InvalidId;
                ui->dockingTargetBounds = {};
                ui->dockingPreviewBounds = {};
            }

            ui->captured = InvalidId;
            ui->capturedItem = {};
            ui->active = InvalidId;
            ui->activeItem = {};
        }

        Response windowResponse;
        windowResponse.id = windowNode.id;
        Detail::setFlag(windowResponse, 0, canPoint && (previousTitle.contains(pointer->position) || windowNode.mutableWindowData().resizeHovered == true || scrollInteraction.hovered == true));
        Detail::setFlag(windowResponse, 1, ui->captured == windowNode.id || scrollInteraction.active == true);
        Detail::setFlag(windowResponse, 6, autoFitResize || scrollInteraction.changed == true);
        Detail::setFlag(windowResponse, 2, ui->focused == windowNode.id);
        windowNode.response = windowResponse;
        bool previousNavigationBlocked = ui->currentNavigationBlocked;
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);
        ui->currentWindow = windowNode.id;
        ui->currentWindowSubmission = windowSubmission;
        ui->currentNavigationBlocked = previousNavigationBlocked || acceptsNavigationInputs == false;

        bool debugReturnFalse = false;
        ++ui->debugBeginCall;

        if(ui->configuration.debugBeginReturnValueOnce == true && ui->debugBeginOnceConsumed == false)
        {
            ui->debugBeginOnceConsumed = true;
            debugReturnFalse = true;
        }

        if(ui->configuration.debugBeginReturnValueLoop == true)
        {
            uint64_t phase = ui->frame.number / 24U + ui->debugBeginCall;
            debugReturnFalse = phase % 5U == 0U;
        }

        return {ui, token, windowNode.id, windowNode.visible && dockActive == true && windowNode.mutableWindowData().collapsed == false && debugReturnFalse == false};
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
