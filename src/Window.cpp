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

            if(node.windowScrollable == false)
            {
                return result;
            }

            const PointerState * pointer = ui->input.primaryPointer();

            if(pointer == nullptr)
            {
                return result;
            }

            bool verticalHit = state.verticalScrollbarTrack.empty() == false && state.verticalScrollbarTrack.contains(pointer->position);
            bool horizontalHit = state.horizontalScrollbarTrack.empty() == false && state.horizontalScrollbarTrack.contains(pointer->position);
            result.hovered = canPoint && (verticalHit || horizontalHit == true);
            bool ownsScrollbar = ui->captured == node.id && (state.windowInteraction == 7 || state.windowInteraction == 8);

            if(canPoint == true && pointer->isPressed() == true && (verticalHit == true || horizontalHit == true))
            {
                bool vertical = verticalHit;
                const Rect & track = vertical ? state.verticalScrollbarTrack : state.horizontalScrollbarTrack;
                const Rect & thumb = vertical ? state.verticalScrollbarThumb : state.horizontalScrollbarThumb;
                float pointerPosition = vertical ? pointer->position.y : pointer->position.x;
                float thumbStart = vertical ? thumb.y : thumb.x;
                float thumbExtent = vertical ? thumb.height : thumb.width;
                float & position = vertical ? state.scrollPosition.y : state.scrollPosition.x;
                float extent = vertical ? state.scrollRange.y : state.scrollRange.x;

                ui->captured = node.id;
                ui->capturedPointer = pointer->id;
                ui->active = node.id;
                ui->focused = node.id;
                state.windowInteraction = vertical ? 7 : 8;
                state.draggingScrollbar = true;
                state.draggingScrollAxis = vertical ? 2 : 1;
                result.consumedPress = true;

                if(thumb.contains(pointer->position) == true)
                {
                    state.scrollbarDragOffset = pointerPosition - thumbStart;
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

                    state.scrollbarDragOffset = thumbExtent * 0.5f;
                    result.changed = position != previous;
                }

                state.scrollTarget = state.scrollPosition;
                state.scrollVelocity = {};
                state.scrollTargetInitialized = true;
            }

            if((ownsScrollbar == true || result.consumedPress == true) && pointer->isDown() == true)
            {
                bool vertical = state.windowInteraction == 7;
                const Rect & track = vertical ? state.verticalScrollbarTrack : state.horizontalScrollbarTrack;
                const Rect & thumb = vertical ? state.verticalScrollbarThumb : state.horizontalScrollbarThumb;
                float & position = vertical ? state.scrollPosition.y : state.scrollPosition.x;
                float previous = position;
                position = Detail::scrollbarScrollAtPointer(track, thumb, vertical ? state.scrollRange.y : state.scrollRange.x, vertical, state.scrollbarDragOffset, pointer->position);
                state.scrollTarget = state.scrollPosition;
                state.scrollVelocity = {};
                state.scrollTargetInitialized = true;
                result.changed = result.changed || position != previous;
            }

            result.active = ownsScrollbar || result.consumedPress;

            if(result.changed == true)
            {
                state.scroll = node.scrollOptions.axes != ScrollAxes::Horizontal ? state.scrollPosition.y : state.scrollPosition.x;
                ui->frame.events.push_back({EventType::Change, node.id, ui->nodePath(node), node.file, node.line, ui->input.timestamp});
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
            float border = std::max(0.f, window.windowPopup ? window.style->metrics.popupBorderSize : window.style->metrics.windowBorderSize);
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
            float leftControlsWidth = window.windowCollapseVisible && window.windowCollapsePlacement == WindowCollapsePlacement::Left ? buttonSide + 2.f : 0.f;
            float rightControlsWidth = (window.windowCloseVisible ? buttonSide + 2.f : 0.f) + (window.windowCollapseVisible && window.windowCollapsePlacement == WindowCollapsePlacement::Right ? buttonSide + 2.f : 0.f) + 4.f;
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
                float unsavedWidth = tabNode != nullptr && tabNode->windowUnsavedDocument ? window.style->metrics.fontSize * 0.75f : 0.f;
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
            case Condition::FirstUseEver:
                return firstUse;
            case Condition::Appearing:
                return appearing;
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        void markWindowCondition(Condition condition, bool & onceApplied) noexcept
        {
            if(condition == Condition::Once)
            {
                onceApplied = true;
            }
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
        LayoutOptions layout;
        layout.orientation = options.scroll.contentOrientation;
        layout.width = options.fitContentWidth ? Dimension(SizeRule::Content) : Dimension::fixed(options.initialBounds.width);
        bool collapsed = options.collapsed != nullptr && *options.collapsed;
        layout.height = collapsed ? Dimension::fixed(ui->currentStyle->metrics.windowTitleHeight) : (options.fitContentHeight ? Dimension(SizeRule::Content) : Dimension::fixed(options.initialBounds.height));
        size_t node = ui->addNode(Detail::NodeKind::Window, key, label, layout, location, SemanticRole::Window);
        Context::Node & windowNode = ui->nodes[node];
        Context::Persistent & persistentState = ui->state(windowNode);

        if(persistentState.windowSettingsPolicyInitialized == false)
        {
            if(options.saveSettings == false)
            {
                persistentState.windowInitialized = false;
                persistentState.windowBounds = {};
            }

            persistentState.windowSettingsPolicyInitialized = true;
        }

        persistentState.windowSaveSettings = options.saveSettings;
        bool firstUse = persistentState.windowInitialized == false;
        bool appearing = persistentState.windowVisibleLastFrame + 1 < ui->frame.number;

        if(persistentState.windowCollapsedInitialized == false)
        {
            persistentState.windowCollapsedValue = collapsed;
            persistentState.windowCollapsedInitialized = true;
        }
        else if(options.collapsed != nullptr)
        {
            persistentState.windowCollapsedValue = *options.collapsed;
        }

        if(nextWindow.collapsedPending == true)
        {
            bool conditionApplies = Detail::windowConditionApplies(nextWindow.collapsedCondition, persistentState.windowCollapsedConditionApplied, firstUse, appearing == true);

            if(conditionApplies == true)
            {
                persistentState.windowCollapsedValue = nextWindow.collapsed;

                if(options.collapsed != nullptr)
                {
                    *options.collapsed = nextWindow.collapsed;
                }

                Detail::markWindowCondition(nextWindow.collapsedCondition, persistentState.windowCollapsedConditionApplied);
            }
        }

        collapsed = persistentState.windowCollapsedValue;
        windowNode.layout.height = collapsed ? Dimension::fixed(ui->currentStyle->metrics.windowTitleHeight) : (options.fitContentHeight ? Dimension(SizeRule::Content) : Dimension::fixed(options.initialBounds.height));
        ui->windowLabels[windowNode.id] = String(label);
        windowNode.visible = options.open == nullptr || *options.open;
        windowNode.inputBlocked = windowNode.inputBlocked || options.input == false;
        windowNode.navigationBlocked = windowNode.navigationBlocked || options.navigation == false || options.input == false;
        windowNode.windowTitleVisible = options.titleBar;
        windowNode.windowBackgroundVisible = options.background;
        windowNode.windowBackgroundAlpha = std::clamp(options.backgroundAlpha, 0.f, 1.f);
        windowNode.windowUnsavedDocument = options.unsavedDocument;

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
        windowNode.windowDockGroup = dockGroup;
        DockModel * dockModel = Detail::dockModel(ui, dockGroup, dockGroup != 0);
        const DockSpaceOptions * dockSpace = Detail::dockSpaceOptions(ui, dockGroup);
        bool dockSpaceNoResize = dockSpace != nullptr && dockSpace->noResize;
        bool dockSpaceAutoHideTabBar = dockSpace != nullptr && dockSpace->autoHideTabBar;
        DockNodeId dockNodeId = dockModel == nullptr ? 0 : dockModel->nodeForWindow(windowNode.id);
        const DockNode * dockNode = dockModel == nullptr ? nullptr : dockModel->node(dockNodeId);
        bool dockActive = dockNode == nullptr || dockNode->activeTab == windowNode.id;
        windowNode.windowDockNode = dockNodeId;
        windowNode.windowDocked = dockNodeId != 0 || (dockGroup != 0 && options.movable == false && options.resizable == false);
        windowNode.windowDockAutoHideTabBar = options.dockAutoHideTabBar || dockSpaceAutoHideTabBar;
        windowNode.windowCollapsed = collapsed;
        windowNode.windowCloseVisible = options.titleBar && options.open != nullptr && (windowNode.windowDocked == false || windowNode.style->metrics.dockingNodeHasCloseButton == true);
        windowNode.windowCollapseVisible = options.titleBar && options.collapsed != nullptr;
        windowNode.windowCollapsePlacement = options.collapsePlacement;
        windowNode.windowMinimumSize = options.minimumSize;
        windowNode.windowMaximumSize = options.maximumSize;
        windowNode.windowFitContentWidth = options.fitContentWidth;
        windowNode.windowFitContentHeight = options.fitContentHeight;
        windowNode.windowScrollable = options.scrollable;
        windowNode.scrollOptions = options.scroll;
        windowNode.windowBringToFront = options.bringToFront;
        const PointerState * pointer = ui->input.primaryPointer();

        if(persistentState.windowZOrder == 0)
        {
            persistentState.windowZOrder = ui->nextWindowZOrder++;
        }

        persistentState.windowVisible = windowNode.visible && dockActive;
        persistentState.windowAcceptsInput = options.input;
        persistentState.windowBringToFront = options.bringToFront;
        persistentState.windowPopup = windowNode.windowPopup;
        persistentState.windowOwner = windowNode.id;
        windowNode.windowOwner = windowNode.id;
        ui->visibleWindowIds.push_back(windowNode.id);

        if(pointer != nullptr && pointer->isPressed() == true && ui->pointerWindow == windowNode.id && options.bringToFront == true)
        {
            persistentState.windowZOrder = ui->nextWindowZOrder++;
        }

        windowNode.windowZOrder = persistentState.windowZOrder;

        if(forcedUndock == true && persistentState.lastBounds.empty() == false)
        {
            persistentState.windowBounds = persistentState.lastBounds;
            persistentState.windowInitialized = true;
        }

        if(persistentState.windowInitialized == false || (options.movable == false && options.resizable == false))
        {
            persistentState.windowBounds = options.initialBounds;
            persistentState.windowInitialized = true;
        }

        if(nextWindow.positionPending == true)
        {
            bool conditionApplies = Detail::windowConditionApplies(nextWindow.positionCondition, persistentState.windowPositionConditionApplied, firstUse, appearing == true);

            if(conditionApplies == true)
            {
                persistentState.windowBounds.x = nextWindow.position.x;
                persistentState.windowBounds.y = nextWindow.position.y;
                Detail::markWindowCondition(nextWindow.positionCondition, persistentState.windowPositionConditionApplied);
            }
        }

        if(nextWindow.sizePending == true)
        {
            bool conditionApplies = Detail::windowConditionApplies(nextWindow.sizeCondition, persistentState.windowSizeConditionApplied, firstUse, appearing == true);

            if(conditionApplies == true)
            {
                persistentState.windowBounds.width = std::max(0.f, nextWindow.size.x);
                persistentState.windowBounds.height = std::max(0.f, nextWindow.size.y);
                Detail::markWindowCondition(nextWindow.sizeCondition, persistentState.windowSizeConditionApplied);
            }
        }

        if(nextWindow.contentSizePending == true)
        {
            bool conditionApplies = Detail::windowConditionApplies(nextWindow.contentSizeCondition, persistentState.windowContentSizeConditionApplied, firstUse, appearing == true);

            if(conditionApplies == true)
            {
                windowNode.windowContentSizeExplicit = true;
                windowNode.windowContentSize = {std::max(0.f, nextWindow.contentSize.x), std::max(0.f, nextWindow.contentSize.y)};
                Detail::markWindowCondition(nextWindow.contentSizeCondition, persistentState.windowContentSizeConditionApplied);
            }
        }

        if((nextWindow.focusPending == true || (options.focusOnAppearing == true && appearing == true)) && windowNode.visible == true && dockActive == true)
        {
            persistentState.windowZOrder = ui->nextWindowZOrder++;
            ui->focused = windowNode.id;
            ui->pointerFocused = windowNode.id;
            ui->navigationFocused = windowNode.id;
            windowNode.windowZOrder = persistentState.windowZOrder;
        }

        Rect available = ui->viewport.workArea.empty() == true ? ui->viewport.bounds : ui->viewport.workArea;
        persistentState.windowBounds = Detail::resizedWindowBounds(persistentState.windowBounds, static_cast<uint8_t>(Detail::WindowResizeRight | Detail::WindowResizeBottom), {}, options.minimumSize, options.maximumSize, available);
        persistentState.windowBounds = Detail::constrainResizedWindowBounds(persistentState.windowBounds, static_cast<uint8_t>(Detail::WindowResizeRight | Detail::WindowResizeBottom), persistentState.windowBounds, options, available);
        persistentState.windowBounds = Detail::constrainWindowBounds(persistentState.windowBounds, available);

        if(windowNode.visible == true && dockActive == true)
        {
            persistentState.windowVisibleLastFrame = ui->frame.number;
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

        if(windowNode.windowCloseVisible == true)
        {
            closeButton = {buttonRight - buttonSide, previousTitle.y + (previousTitle.height - buttonSide) * 0.5f, buttonSide, buttonSide};
            buttonRight = closeButton.x - 2.f;
        }

        if(windowNode.windowCollapseVisible == true)
        {
            collapseButton = options.collapsePlacement == WindowCollapsePlacement::Left ? Rect{previousTitle.x + 4.f, previousTitle.y + (previousTitle.height - buttonSide) * 0.5f, buttonSide, buttonSide} : Rect{buttonRight - buttonSide, previousTitle.y + (previousTitle.height - buttonSide) * 0.5f, buttonSide, buttonSide};
        }

        bool dockHostCornerOwner = dockNodeId != 0 && std::abs(persistentState.lastBounds.right() - dockHostBounds.right()) <= 1.f && std::abs(persistentState.lastBounds.bottom() - dockHostBounds.bottom()) <= 1.f;
        Rect resizeBounds = dockNodeId != 0 ? dockHostBounds : persistentState.lastBounds;
        float resizeHitThickness = std::max(1.f, windowNode.style->metrics.windowBorderSize + windowNode.style->metrics.windowBorderHoverPadding);
        bool canPoint = windowNode.visible && dockActive == true && windowNode.disabled == false && windowNode.inputBlocked == false && Detail::inputLayerBlocked(ui, windowNode) == false && pointer != nullptr && (ui->pointerWindow == InvalidId || ui->pointerWindow == windowNode.id || ui->captured == windowNode.id) && (ui->captured == InvalidId || ui->captured == windowNode.id);
        Detail::WindowScrollResult scrollInteraction = Detail::windowScrollBehavior(ui, windowNode, persistentState, canPoint);
        uint8_t resizeEdges = pointer == nullptr || scrollInteraction.hovered == true || scrollInteraction.active ? Detail::WindowResizeNone : Detail::windowResizeEdgesAt(resizeBounds, pointer->position, resizeHitThickness);

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
            resizeEdges = bottomRight ? static_cast<uint8_t>(Detail::WindowResizeRight | Detail::WindowResizeBottom) : Detail::WindowResizeNone;
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
                    ui->navigationFocused = tab;

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
                        windowNode.windowDockNode = 0;
                        windowNode.windowDocked = false;
                        ui->dockingDragWindow = windowNode.id;
                        ui->dockingTabDragWindow = InvalidId;
                        ui->dockingTabDragNode = 0;
                        persistentState.windowInteraction = 3;
                        persistentState.draggingWindow = true;
                        persistentState.windowBounds = sourceBounds;
                        persistentState.windowInitialized = true;
                        persistentState.dragOffset = pointer->position - Vec2{sourceBounds.x, sourceBounds.y};
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
                ui->capturedPointer = 0;
            }
        }

        windowNode.windowCloseHovered = canPoint && closeButton.contains(pointer->position);
        windowNode.windowCollapseHovered = canPoint && collapseButton.contains(pointer->position);
        windowNode.windowResizeHovered = canPoint && options.resizable == true && dockSpaceNoResize == false && resizeEdges != Detail::WindowResizeNone;
        bool activeResize = ui->captured == windowNode.id && persistentState.windowInteraction == 6;
        windowNode.windowResizeEdges = activeResize ? persistentState.windowResizeEdges : resizeEdges;
        windowNode.windowResizable = options.resizable && dockSpaceNoResize == false && (dockNodeId == 0 || dockHostCornerOwner == true || dockHostPointerOwner == true || persistentState.resizingDockHost == true);
        windowNode.windowResizeGripVisible = options.resizable && dockSpaceNoResize == false && (dockNodeId == 0 || dockHostCornerOwner == true);
        windowNode.windowResizeBounds = resizeBounds;

        bool autoFitResize = false;
        bool requestAutoFit = scrollInteraction.consumedPress == false;

        if(canPoint == false)
        {
            requestAutoFit = false;
        }

        if(windowNode.windowResizeHovered == false)
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

        if(persistentState.scrollContentSize.x <= 0.f)
        {
            if(persistentState.scrollContentSize.y <= 0.f)
            {
                requestAutoFit = false;
            }
        }

        if(requestAutoFit == true)
        {
            Rect fitted = persistentState.windowBounds;
            float verticalScrollbar = persistentState.scrollRange.y > 0.f ? windowNode.style->metrics.scrollbarWidth + windowNode.style->metrics.gap : 0.f;
            float horizontalScrollbar = persistentState.scrollRange.x > 0.f ? windowNode.style->metrics.scrollbarWidth + windowNode.style->metrics.gap : 0.f;
            float desiredWidth = persistentState.scrollContentSize.x + windowNode.layout.padding.left + windowNode.layout.padding.right + verticalScrollbar;
            float desiredHeight = persistentState.scrollContentSize.y + windowNode.layout.padding.top + windowNode.layout.padding.bottom + horizontalScrollbar + (windowNode.windowTitleVisible ? windowNode.style->metrics.windowTitleHeight : 0.f);

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

            persistentState.windowBounds = Detail::resizedWindowBounds(fitted, resizeEdges, {}, options.minimumSize, options.maximumSize, available);
            persistentState.windowBounds = Detail::constrainWindowBounds(persistentState.windowBounds, available);
            autoFitResize = true;
        }

        if(persistentState.windowBackgroundMovePending == true)
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
                persistentState.windowBackgroundMovePending = false;
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
                    persistentState.draggingWindow = true;
                    persistentState.windowInteraction = 3;
                    persistentState.dragOffset = pointer->pressPosition(PointerButton::Primary) - Vec2{persistentState.windowBounds.x, persistentState.windowBounds.y};
                    persistentState.windowBackgroundMovePending = false;

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

        if(windowNode.windowResizeHovered == false)
        {
            beginResize = false;
        }

        bool pressClose = actionPress;

        if(windowNode.windowCloseHovered == false)
        {
            pressClose = false;
        }

        bool pressCollapse = actionPress;

        if(windowNode.windowCollapseHovered == false)
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
            persistentState.windowInteraction = 6;
            persistentState.dragStartPosition = pointer->position;
            persistentState.windowResizeStartBounds = resizeBounds;
            persistentState.windowResizeEdges = resizeEdges;
            persistentState.resizingDockHost = dockNodeId != 0;
        }
        else if(pressClose == true)
        {
            ui->captured = windowNode.id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.windowInteraction = 2;
        }
        else if(pressCollapse == true)
        {
            ui->captured = windowNode.id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.windowInteraction = 1;
        }
        else if(doubleClickTitle == true)
        {
            *options.collapsed = !*options.collapsed;
            windowNode.windowCollapsed = *options.collapsed;
            windowNode.layout.height = Dimension::fixed(*options.collapsed ? windowNode.style->metrics.windowTitleHeight : options.initialBounds.height);
            persistentState.draggingWindow = false;
            persistentState.windowInteraction = 0;
            ui->focused = windowNode.id;
            ui->frame.events.push_back({EventType::Change, windowNode.id, ui->nodePath(windowNode), windowNode.file, windowNode.line, ui->input.timestamp});
        }
        else if(detachDockTab == true)
        {
            ui->captured = windowNode.id;
            ui->capturedPointer = pointer->id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.windowInteraction = 5;
            persistentState.draggingWindow = true;
            persistentState.dragOffset = pointer->position - Vec2{dockHostBounds.x, dockHostBounds.y};
        }
        else if(moveFromTitle == true)
        {
            ui->captured = windowNode.id;
            ui->active = windowNode.id;
            ui->focused = windowNode.id;
            persistentState.draggingWindow = true;
            persistentState.windowInteraction = 3;
            persistentState.dragOffset = pointer->position - Vec2{persistentState.windowBounds.x, persistentState.windowBounds.y};

            if(dockGroup != 0)
            {
                ui->dockingDragWindow = windowNode.id;
            }
        }
        else if(prepareBackgroundMove == true)
        {
            persistentState.windowBackgroundMovePending = true;
        }

        if(pointer != nullptr && ui->captured == windowNode.id && persistentState.windowInteraction == 5 && persistentState.draggingWindow == true && pointer->isDown() == true)
        {
            dockHostBounds.x = pointer->position.x - persistentState.dragOffset.x;
            dockHostBounds.y = pointer->position.y - persistentState.dragOffset.y;
            dockHostBounds = Detail::constrainWindowBounds(dockHostBounds, available);
            Detail::setDockArea(ui, dockGroup, dockHostBounds);
        }

        if(pointer != nullptr && ui->captured == windowNode.id && persistentState.windowInteraction == 3 && persistentState.draggingWindow == true && pointer->isDown() == true)
        {
            persistentState.windowBounds.x = pointer->position.x - persistentState.dragOffset.x;
            persistentState.windowBounds.y = pointer->position.y - persistentState.dragOffset.y;
            persistentState.windowBounds = Detail::constrainWindowBounds(persistentState.windowBounds, available);
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

                        if(candidate->windowDockGroup != dockGroup)
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
                    bool noMerge = ui->configuration.dockingNoMerge == true || (dockSpace != nullptr && dockSpace->noMerge == true);
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

        if(pointer != nullptr && ui->captured == windowNode.id && persistentState.windowInteraction == 6 && pointer->isDown() == true)
        {
            Vec2 delta = pointer->position - persistentState.dragStartPosition;
            Rect resizedBounds = Detail::resizedWindowBounds(persistentState.windowResizeStartBounds, persistentState.windowResizeEdges, delta, options.minimumSize, options.maximumSize, available);
            resizedBounds = Detail::constrainResizedWindowBounds(persistentState.windowResizeStartBounds, persistentState.windowResizeEdges, resizedBounds, options, available);

            if(persistentState.resizingDockHost == true)
            {
                Detail::setDockArea(ui, dockGroup, resizedBounds);
            }
            else
            {
                persistentState.windowBounds = resizedBounds;
            }
        }

        if(pointer != nullptr && ui->captured == windowNode.id && pointer->isReleased() == true)
        {
            bool completeDocking = persistentState.windowInteraction == 3;

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

            if(persistentState.windowInteraction == 1 && options.collapsed != nullptr && collapseButton.contains(pointer->position) == true)
            {
                *options.collapsed = !*options.collapsed;
                windowNode.windowCollapsed = *options.collapsed;
                windowNode.layout.height = Dimension::fixed(*options.collapsed ? windowNode.style->metrics.windowTitleHeight : options.initialBounds.height);
                ui->frame.events.push_back({EventType::Change, windowNode.id, ui->nodePath(windowNode), windowNode.file, windowNode.line, ui->input.timestamp});
            }
            else if(persistentState.windowInteraction == 2 && options.open != nullptr && closeButton.contains(pointer->position) == true)
            {
                *options.open = false;
                windowNode.visible = false;
                ui->frame.events.push_back({EventType::Change, windowNode.id, ui->nodePath(windowNode), windowNode.file, windowNode.line, ui->input.timestamp});
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

            persistentState.draggingWindow = false;
            persistentState.draggingScrollbar = false;
            persistentState.draggingScrollAxis = 0;
            persistentState.resizingDockHost = false;
            persistentState.windowResizeEdges = Detail::WindowResizeNone;
            persistentState.windowInteraction = 0;

            if(ui->dockingDragWindow == windowNode.id)
            {
                ui->dockingDragWindow = InvalidId;
                ui->dockingPreviewNode = 0;
                ui->dockingPreviewTargetWindow = InvalidId;
                ui->dockingTargetBounds = {};
                ui->dockingPreviewBounds = {};
            }

            ui->captured = InvalidId;
            ui->active = InvalidId;
        }

        Response windowResponse;
        windowResponse.id = windowNode.id;
        Detail::setFlag(windowResponse, 0, canPoint && (previousTitle.contains(pointer->position) || windowNode.windowResizeHovered == true || scrollInteraction.hovered == true));
        Detail::setFlag(windowResponse, 1, ui->captured == windowNode.id || scrollInteraction.active == true);
        Detail::setFlag(windowResponse, 6, autoFitResize || scrollInteraction.changed == true);
        Detail::setFlag(windowResponse, 2, ui->focused == windowNode.id);
        windowNode.response = windowResponse;
        bool previousNavigationBlocked = ui->currentNavigationBlocked;
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);
        ui->currentWindow = windowNode.id;
        ui->currentNavigationBlocked = previousNavigationBlocked || options.navigation == false;

        return {ui, token, windowNode.id, windowNode.visible && dockActive == true && windowNode.windowCollapsed == false};
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
