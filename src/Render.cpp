#include "Render.hpp"
#include "ContextDetail.hpp"
#include "Layout.hpp"
#include "Window.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool emitsVisuals(NodeKind kind) noexcept
        {
            switch(kind)
            {
            case NodeKind::Root:
            case NodeKind::Scope:
            case NodeKind::Column:
            case NodeKind::Grid:
            case NodeKind::Overlay:
            case NodeKind::Absolute:
            case NodeKind::Clip:
            case NodeKind::Disabled:
            case NodeKind::Interaction:
            case NodeKind::Style:
            case NodeKind::Spacer:
            case NodeKind::TableCell:
            case NodeKind::NativeSurface:
                return false;
            default:
                return true;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] SemanticActionFlags semanticActions(const Context::Node & node) noexcept
        {
            if(node.disabled == true)
            {
                return 0;
            }

            if(node.inputBlocked == true)
            {
                return 0;
            }

            SemanticActionFlags actions = node.focusable ? Mosaic::semanticAction(SemanticAction::Focus) : 0;
            switch(node.semanticRole)
            {
            case SemanticRole::Button:
            case SemanticRole::MenuItem:
            case SemanticRole::Tab:
                actions |= Mosaic::semanticAction(SemanticAction::Press);
                break;
            case SemanticRole::Checkbox:
            case SemanticRole::Radio:
                actions |= Mosaic::semanticAction(SemanticAction::Press) | Mosaic::semanticAction(SemanticAction::Toggle);
                break;
            case SemanticRole::TreeItem:
                actions |= Mosaic::semanticAction(SemanticAction::Press) | Mosaic::semanticAction(node.expanded ? SemanticAction::Collapse : SemanticAction::Expand) | Mosaic::semanticAction(SemanticAction::Select);
                break;
            case SemanticRole::Row:
                actions |= Mosaic::semanticAction(SemanticAction::Press) | Mosaic::semanticAction(SemanticAction::Select);
                break;
            case SemanticRole::TextField:
                if(node.readOnly == false)
                {
                    actions |= Mosaic::semanticAction(SemanticAction::SetValue);
                }

                break;
            case SemanticRole::Slider:
                if(node.readOnly == false)
                {
                    actions |= Mosaic::semanticAction(SemanticAction::SetValue) | Mosaic::semanticAction(SemanticAction::Increment) | Mosaic::semanticAction(SemanticAction::Decrement);
                }

                break;
            default:
                break;
            }

            return actions;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    uint64_t Context::internRenderState(const RenderState & value)
    {
        if(lastRenderStateKey != 0 && lastRenderState == value)
        {
            return lastRenderStateKey;
        }

        constexpr size_t minimumCapacity = 64;
        RenderStateHash hasher;

        if(renderStateIndices.empty() == true || (frame.renderStates.size() + 1) * 2 > renderStateIndices.size())
        {
            size_t capacity = renderStateIndices.empty() == true ? minimumCapacity : renderStateIndices.size() * 2;
            renderStateIndices.assign(capacity, {});
            size_t mask = capacity - 1;
            for(size_t index = 0; index != frame.renderStates.size(); ++index)
            {
                size_t stateHash = hasher(frame.renderStates[index]);
                size_t slot = stateHash & mask;
                while(renderStateIndices[slot].frame == frame.number)
                {
                    slot = (slot + 1) & mask;
                }
                renderStateIndices[slot] = {stateHash, index + 1, frame.number};
            }
        }

        size_t valueHash = hasher(value);
        size_t mask = renderStateIndices.size() - 1;
        size_t slot = valueHash & mask;
        while(renderStateIndices[slot].frame == frame.number)
        {
            const RenderStateIndexEntry & entry = renderStateIndices[slot];

            if(entry.hash == valueHash && entry.key != 0 && frame.renderStates[entry.key - 1] == value)
            {
                lastRenderState = value;
                lastRenderStateKey = entry.key;

                return lastRenderStateKey;
            }

            slot = (slot + 1) & mask;
        }

        frame.renderStates.emplace_back(value);
        lastRenderState = value;
        lastRenderStateKey = frame.renderStates.size();
        renderStateIndices[slot] = {valueHash, lastRenderStateKey, frame.number};

        return lastRenderStateKey;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::emitText(DrawList & drawList, const Node & node, const Vec2 & position, const Color & color, const RenderState & baseState, uint64_t baseKey)
    {
        if(node.textRun == nullptr)
        {
            return;
        }

        emitPreparedText(drawList, *node.textRun, position, color, baseState, baseKey);
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::emitValueText(DrawList & drawList, const Node & node, const Vec2 & position, const Color & color, const RenderState & baseState, uint64_t baseKey)
    {
        if(node.valueTextRun != nullptr)
        {
            emitPreparedText(drawList, *node.valueTextRun, position, color, baseState, baseKey);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::emitPreparedText(DrawList & drawList, const CachedText & text, const Vec2 & position, const Color & color, const RenderState & baseState, uint64_t baseKey, const Vec2 & axisX, const Vec2 & axisY)
    {
        Vec2 translation = {Detail::snapToPixel(position.x, viewport.dpiScale), Detail::snapToPixel(position.y, viewport.dpiScale)};
        uint64_t batchIndex = 0;
        for(const PreparedTextRunBatch & batch : text.batches)
        {
            RenderState glyphState = baseState;
            glyphState.texture = batch.texture;
            uint64_t glyphKey = batch.texture == baseState.texture ? baseKey : internRenderState(glyphState);
            uint64_t geometryKey = combineId(text.key, batchIndex);
            drawList.textGeometry(geometryKey, batch.rectangles, translation, color, glyphKey, axisX, axisY);
            ++batchIndex;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::emitNode(size_t index, DrawList & drawList, CanvasLayer canvasPass)
    {
        Node & node = nodes[index];

        if(node.kind == Detail::NodeKind::NativeSurface)
        {
            if(emittingNativeSurface != node.id)
            {
                return;
            }
        }

        if(node.kind == Detail::NodeKind::Canvas && hasCanvasCommands(node, canvasPass) == false)
        {
            if(canvasPass == CanvasLayer::Local)
            {
                for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
                {
                    emitNode(child, drawList, CanvasLayer::Local);
                }
            }

            return;
        }

        if(node.visible == false)
        {
            return;
        }

        if((node.response.flags & Detail::CulledNodeFlag) != 0)
        {
            return;
        }

        if(node.clip.empty() == true)
        {
            if(Detail::clipsDescendants(node.kind) == false)
            {
                for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
                {
                    emitNode(child, drawList, canvasPass);
                }
            }

            return;
        }

        auto emitMetadata = [this, &node, index]()
        {
            if(frameCaptureOptions.semantics == true && node.semanticRole != SemanticRole::None)
            {
                StringView semanticName = nodeSemanticName(node);
                frame.semantics.push_back({node.id, node.parentId, node.semanticRole, semanticName.empty() == true ? node.label : String(semanticName), String(nodeSemanticDescription(node)), String(nodeSemanticValue(node)), node.bounds, node.checked, node.selected, node.expanded, node.disabled, focused == node.id, node.readOnly, Detail::semanticActions(node)});
            }

            if(frameCaptureOptions.debug == true)
            {
                const Persistent * persistentState = findState(node.id);
                frame.debug.push_back({node.id, node.parentId, Detail::nodeKindName(node.kind), node.label, nodePath(index), node.debugData().file, node.debugData().function, node.debugData().line, persistentState == nullptr ? frame.number : persistentState->firstFrame, persistentState == nullptr ? frame.number : persistentState->lastFrame, node.windowData().zOrder, node.bounds, node.clip, node.response.hovered(), active == node.id, focused == node.id, node.disabled});
            }
        };
        auto emitChildren = [this, &node, &drawList, canvasPass]()
        {
            if(node.kind == Detail::NodeKind::Root)
            {
                for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
                {
                    if(nodes[child].kind != Detail::NodeKind::Window && nodes[child].kind != Detail::NodeKind::Backdrop)
                    {
                        emitNode(child, drawList, canvasPass);
                    }
                }
                for(size_t child : windowRenderOrder)
                {
                    if(nodes[child].windowData().popup == false)
                    {
                        emitNode(child, drawList, canvasPass);
                    }
                }
                for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
                {
                    if(nodes[child].kind == Detail::NodeKind::Backdrop)
                    {
                        emitNode(child, drawList, canvasPass);
                    }
                }
                for(size_t child : windowRenderOrder)
                {
                    if(nodes[child].windowData().popup)
                    {
                        emitNode(child, drawList, canvasPass);
                    }
                }

                return;
            }

            for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
            {
                emitNode(child, drawList, canvasPass);
            }
        };

        if(Detail::emitsVisuals(node.kind) == false)
        {
            emitMetadata();
            emitChildren();

            return;
        }

        struct AlphaRestore final
        {
            DrawList & drawList;
            float alpha;
            //////////////////////////////////////////////////////////////////////////
            ~AlphaRestore()
            {
                drawList.setAlphaMultiplier(alpha);
            }
        };

        AlphaRestore alphaRestore = {drawList, drawList.alphaMultiplier()};
        float nodeAlpha = std::clamp(node.style->behavior.alpha, 0.f, 1.f) * (node.disabled ? std::clamp(node.style->behavior.disabledAlpha, 0.f, 1.f) : 1.f);

        bool transparentDockingPayload = configuration.dockingTransparentPayload == true;
        transparentDockingPayload = transparentDockingPayload == true && node.kind == Detail::NodeKind::Window;
        transparentDockingPayload = transparentDockingPayload == true && dockingDragWindow == node.id;

        if(transparentDockingPayload == true)
        {
            nodeAlpha *= 0.35f;
        }

        drawList.setAlphaMultiplier(nodeAlpha);
        Color textColor = node.disabled ? node.style->colors.textDisabled : node.style->colors.text;
        RenderState baseState;
        baseState.clip = node.kind == Detail::NodeKind::Canvas && canvasPass != CanvasLayer::Local ? viewport.bounds : node.visualClip;
        baseState.blend = BlendMode::PremultipliedAlpha;
        baseState.renderTarget = viewport.renderTarget;
        float physicalPixel = 1.f / std::max(1.f, viewport.dpiScale);
        baseState.linePenumbra = node.style->behavior.antiAliasedLines == true ? physicalPixel * 0.5f : 0.f;
        baseState.fillFeather = node.style->behavior.antiAliasedFill == true ? physicalPixel * 0.75f : 0.f;
        baseState.curveTessellationMaximumError = std::max(0.01f, node.style->behavior.curveTessellationMaximumError);
        baseState.circleTessellationMaximumError = std::max(0.01f, node.style->behavior.circleTessellationMaximumError);
        baseState.curveQuality = std::max<uint8_t>(1, node.style->behavior.curveTessellationQuality);
        baseState.ellipseQuality = std::clamp<uint8_t>(node.style->behavior.ellipseTessellationQuality, 4, 252);
        baseState.rectangleQuality = std::max<uint8_t>(1, node.style->behavior.rectangleTessellationQuality);
        uint64_t baseKey = internRenderState(baseState);
        Persistent * visualState = node.persistentState;

        if(visualState != nullptr)
        {
            Detail::updateVisualState(*visualState, node, input.deltaTime);
        }

        float hoverVisual = visualState == nullptr ? (node.response.hovered() ? 1.f : 0.f) : visualState->hoverVisual;
        float activeVisual = visualState == nullptr ? (node.response.active() ? 1.f : 0.f) : visualState->activeVisual;
        float selectionVisual = visualState == nullptr ? (node.checked || node.selected == true || node.expanded ? 1.f : 0.f) : visualState->selectionVisual;
        float focusVisual = visualState == nullptr ? (node.response.focused() ? 1.f : 0.f) : visualState->focusVisual;
        float navigationFocusVisual = navigationCursorVisible && navigationFocused == node.id ? focusVisual : 0.f;
        float scalarVisual = visualState == nullptr ? node.valueData().scalar : visualState->scalarVisual;
        double hoverDuration = visualState == nullptr ? 0.0 : visualState->hoverDuration;
        Color controlColor = Detail::mixColor(node.style->colors.panel, node.style->colors.panelHovered, hoverVisual);
        controlColor = Detail::mixColor(controlColor, node.style->colors.selection, selectionVisual * 0.82f);
        controlColor = Detail::mixColor(controlColor, node.style->colors.panelActive, activeVisual);
        Color controlBorder = Detail::mixColor(node.style->colors.border, node.style->colors.accent, std::max(hoverVisual * 0.22f, activeVisual));
        controlBorder = Detail::mixColor(controlBorder, node.style->colors.navigationCursor, navigationFocusVisual);

        if(node.disabled == true)
        {
            controlColor = Detail::mixColor(controlColor, node.style->colors.background, 0.55f);
            controlBorder = Detail::mixColor(controlBorder, node.style->colors.background, 0.45f);
        }

        Color accentColor = node.disabled ? Detail::mixColor(node.style->colors.accent, node.style->colors.background, 0.58f) : node.style->colors.accent;
        switch(node.kind)
        {
        case Detail::NodeKind::Backdrop:
            drawList.rect(node.bounds, node.tint, baseKey);
            break;
        case Detail::NodeKind::Row:
            if(node.menuBar == true && node.fillBackground == true)
            {
                drawList.rect(node.bounds, node.style->colors.menuBarBackground, baseKey);
            }

            break;
        case Detail::NodeKind::Split:
        {
            Rect splitterBounds = visualState == nullptr ? Rect{} : visualState->splitterBounds;

            if(splitterBounds.empty() == false)
            {
                float interaction = std::max(hoverVisual, activeVisual);
                drawList.rect(splitterBounds, node.style->colors.background, baseKey);
                Color splitterColor = Detail::mixColor(node.style->colors.separator, node.style->colors.separatorHovered, hoverVisual);
                splitterColor = Detail::mixColor(splitterColor, node.style->colors.separatorActive, activeVisual);

                if(node.layout.orientation == Orientation::Horizontal)
                {
                    float center = splitterBounds.x + splitterBounds.width * 0.5f;
                    drawList.line({center, splitterBounds.y}, {center, splitterBounds.bottom()}, 1.f + interaction, splitterColor, baseKey);
                }
                else
                {
                    float center = splitterBounds.y + splitterBounds.height * 0.5f;
                    drawList.line({splitterBounds.x, center}, {splitterBounds.right(), center}, 1.f + interaction, splitterColor, baseKey);
                }
            }

            break;
        }
        case Detail::NodeKind::Window:
        {
            float windowBorder = node.windowData().popup ? node.style->metrics.popupBorderSize : node.style->metrics.windowBorderSize;
            float windowRadius = node.windowData().docked || node.windowData().attachedPopup ? 0.f : (node.windowData().popup ? node.style->metrics.popupCornerRadius : node.style->metrics.cornerRadius);

            if(node.windowData().backgroundVisible == true)
            {
                Color background = node.style->colors.background;
                Color border = node.style->colors.borderStrong;
                background.a *= node.windowData().backgroundAlpha;
                border.a *= node.windowData().backgroundAlpha;
                Detail::drawFrame(drawList, node.bounds, windowRadius, windowBorder, background, border, baseKey);
            }

            if(node.windowData().titleVisible == true)
            {
                Rect titleBar = Detail::windowTitleBarBounds(node, node.bounds);
                Color titleColor = node.windowData().collapsed ? node.style->colors.titleBackgroundCollapsed : Detail::mixColor(node.style->colors.titleBackground, node.style->colors.titleBackgroundActive, focusVisual);

                bool floatingDockingTab = configuration.dockingAlwaysTabBar == true;
                floatingDockingTab = floatingDockingTab == true && node.windowData().dockGroup != 0;
                floatingDockingTab = floatingDockingTab == true && node.windowData().docked == false;

                if(floatingDockingTab == true)
                {
                    titleColor = Detail::mixColor(node.style->colors.tabSelected, node.style->colors.tabDimmedSelected, focusVisual < 0.5f ? 0.45f : 0.f);
                }

                if(node.windowData().docked == true)
                {
                    drawList.rect(titleBar, titleColor, baseKey);
                }
                else
                {
                    drawList.roundedRect(titleBar, std::max(0.f, node.style->metrics.cornerRadius - windowBorder), titleColor, baseKey);
                }

                drawList.line({titleBar.x, titleBar.bottom()}, {titleBar.right(), titleBar.bottom()}, windowBorder, node.style->colors.borderStrong, baseKey);

                if(floatingDockingTab == true && node.style->metrics.tabOverlineSize > 0.f)
                {
                    drawList.rect({titleBar.x, titleBar.y, titleBar.width, node.style->metrics.tabOverlineSize}, node.style->colors.tabSelectedOverline, baseKey);
                }

                float buttonSide = std::max(12.f, node.style->metrics.windowTitleHeight - 8.f);
                float buttonRight = titleBar.right() - 4.f;
                const DockModel * dockModel = Detail::dockModel(this, node.windowData().dockGroup);
                const DockNode * dockNode = dockModel == nullptr ? nullptr : dockModel->node(node.windowData().dockNode);

                if(dockNode != nullptr && dockNode->tabs.empty() == false && (node.windowData().dockAutoHideTabBar == false || dockNode->tabs.size() > 1))
                {
                    const PointerState * pointer = input.primaryPointer();
                    for(size_t tabIndex = 0; tabIndex != dockNode->tabs.size(); ++tabIndex)
                    {
                        Id tab = dockNode->tabs[tabIndex];
                        Rect tabBounds;
                        if(Detail::dockTabBounds(this, node, *dockNode, tabIndex, titleBar, &tabBounds) == false)
                        {
                            break;
                        }

                        bool activeTab = dockNode->activeTab == tab;
                        bool hoveredTab = pointer != nullptr && node.style->behavior.hoverEnabled == true && tabBounds.contains(pointer->position);
                        bool dimmedTab = Mosaic::windowFocused(this, node.id) == false;
                        Color tabColor = activeTab ? (dimmedTab ? node.style->colors.tabDimmedSelected : node.style->colors.tabActive) : (hoveredTab ? node.style->colors.tabHovered : (dimmedTab ? node.style->colors.tabDimmed : node.style->colors.tab));
                        drawList.rect(tabBounds, tabColor, baseKey);
                        Color tabBorder = node.style->colors.border;
                        drawList.line({tabBounds.x, tabBounds.y}, {tabBounds.right(), tabBounds.y}, node.style->metrics.tabBorderSize, tabBorder, baseKey);
                        drawList.line({tabBounds.x, tabBounds.y}, {tabBounds.x, tabBounds.bottom()}, node.style->metrics.tabBorderSize, tabBorder, baseKey);
                        drawList.line({tabBounds.right(), tabBounds.y}, {tabBounds.right(), tabBounds.bottom()}, node.style->metrics.tabBorderSize, tabBorder, baseKey);

                        if(activeTab == false)
                        {
                            drawList.line({tabBounds.x, tabBounds.bottom()}, {tabBounds.right(), tabBounds.bottom()}, node.style->metrics.tabBarBorderSize, node.style->colors.borderStrong, baseKey);
                        }

                        if(activeTab)
                        {
                            drawList.line({tabBounds.x + 5.f, tabBounds.bottom() - 1.f}, {tabBounds.right() - 5.f, tabBounds.bottom() - 1.f}, 1.f, dimmedTab ? node.style->colors.tabDimmedSelectedOverline : node.style->colors.accent, baseKey);
                        }

                        const Node * tabNode = findFrameNode(tab);

                        if(tabNode != nullptr && tabNode->kind == Detail::NodeKind::Window)
                        {
                            Rect textClip = tabBounds.inset(node.style->metrics.tabBorderSize);
                            textClip.x += node.style->metrics.framePadding.left;
                            textClip.width = std::max(0.f, textClip.width - node.style->metrics.framePadding.left - node.style->metrics.framePadding.right);
                            drawList.pushClip(Rect::intersection(node.clip, textClip), baseKey);
                            emitText(drawList, *tabNode, {tabBounds.x + node.style->metrics.framePadding.left, tabBounds.y + (tabBounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);

                            if(tabNode->windowData().unsavedDocument == true)
                            {
                                emitValueText(drawList, *tabNode, {tabBounds.right() - node.style->metrics.framePadding.right - node.style->metrics.fontSize * 0.5f, tabBounds.y + (tabBounds.height - node.style->metrics.lineHeight) * 0.5f}, node.style->colors.unsavedMarker, baseState, baseKey);
                            }

                            drawList.popClip(baseKey);
                        }
                    }
                }
                else
                {
                    float leftControlsWidth = node.windowData().collapseVisible && node.windowData().collapsePlacement == WindowCollapsePlacement::Left ? buttonSide + 2.f : 0.f;
                    float titleContentRight = titleBar.right() - 4.f;

                    if(node.windowData().closeVisible == true)
                    {
                        titleContentRight -= buttonSide + 2.f;
                    }

                    if(node.windowData().collapseVisible == true && node.windowData().collapsePlacement == WindowCollapsePlacement::Right)
                    {
                        titleContentRight -= buttonSide + 2.f;
                    }

                    float unsavedWidth = node.windowData().unsavedDocument ? node.style->metrics.fontSize * 0.75f : 0.f;
                    float textLeft = node.bounds.x + node.style->metrics.padding + 3.f + leftControlsWidth;
                    Rect titleTextClip = {textLeft, titleBar.y, std::max(0.f, titleContentRight - unsavedWidth - textLeft), titleBar.height};
                    drawList.pushClip(Rect::intersection(node.clip, titleTextClip), baseKey);
                    Vec2 titleAlignment = {std::clamp(node.style->metrics.windowTitleAlignment.x, 0.f, 1.f), std::clamp(node.style->metrics.windowTitleAlignment.y, 0.f, 1.f)};
                    emitText(drawList, node, {textLeft + std::max(0.f, titleTextClip.width - node.textSize.x) * titleAlignment.x, titleBar.y + std::max(0.f, titleBar.height - node.style->metrics.lineHeight) * titleAlignment.y}, textColor, baseState, baseKey);
                    drawList.popClip(baseKey);

                    if(node.windowData().unsavedDocument == true)
                    {
                        emitValueText(drawList, node, {titleContentRight - unsavedWidth * 0.6f, titleBar.y + (titleBar.height - node.style->metrics.lineHeight) * 0.5f}, node.style->colors.unsavedMarker, baseState, baseKey);
                    }
                }

                if(node.windowData().closeVisible == true)
                {
                    Rect button = {buttonRight - buttonSide, titleBar.y + (titleBar.height - buttonSide) * 0.5f, buttonSide, buttonSide};

                    if(node.windowData().closeHovered == true)
                    {
                        drawList.roundedRect(button, node.style->metrics.cornerRadius, Detail::colorWithAlpha(node.style->colors.error, 0.72f), baseKey);
                    }

                    Color icon = node.windowData().closeHovered ? Color{1.f, 1.f, 1.f, 1.f} : textColor;
                    drawList.line({button.x + 5.f, button.y + 5.f}, {button.right() - 5.f, button.bottom() - 5.f}, 1.5f, icon, baseKey);
                    drawList.line({button.right() - 5.f, button.y + 5.f}, {button.x + 5.f, button.bottom() - 5.f}, 1.5f, icon, baseKey);
                    buttonRight = button.x - 2.f;
                }

                if(node.windowData().collapseVisible == true)
                {
                    Rect button = node.windowData().collapsePlacement == WindowCollapsePlacement::Left ? Rect{titleBar.x + 4.f, titleBar.y + (titleBar.height - buttonSide) * 0.5f, buttonSide, buttonSide} : Rect{buttonRight - buttonSide, titleBar.y + (titleBar.height - buttonSide) * 0.5f, buttonSide, buttonSide};

                    if(node.windowData().collapseHovered == true)
                    {
                        drawList.roundedRect(button, node.style->metrics.cornerRadius, Detail::colorWithAlpha(node.style->colors.panelHovered, 0.92f), baseKey);
                    }

                    float centerX = button.x + button.width * 0.5f;
                    float centerY = button.y + button.height * 0.5f;

                    if(node.windowData().collapsed == true)
                    {
                        drawList.line({centerX - 4.f, centerY + 2.f}, {centerX, centerY - 2.f}, 1.5f, textColor, baseKey);
                        drawList.line({centerX, centerY - 2.f}, {centerX + 4.f, centerY + 2.f}, 1.5f, textColor, baseKey);
                    }
                    else
                    {
                        drawList.line({centerX - 4.f, centerY - 2.f}, {centerX, centerY + 2.f}, 1.5f, textColor, baseKey);
                        drawList.line({centerX, centerY + 2.f}, {centerX + 4.f, centerY - 2.f}, 1.5f, textColor, baseKey);
                    }
                }
            }

            if(node.windowData().resizable == true)
            {
                float hover = node.windowData().resizeHovered ? 1.f : 0.f;
                float active = visualState != nullptr && visualState->windowData().interaction == 6 ? 1.f : 0.f;
                Color gripColor = Detail::mixColor(node.style->colors.resizeGrip, node.style->colors.resizeGripHovered, hover);
                gripColor = Detail::mixColor(gripColor, node.style->colors.resizeGripActive, active);
                Rect resizeBounds = node.windowData().resizeBounds.empty() == true ? node.bounds : node.windowData().resizeBounds;
                float edgeThickness = active > 0.f ? 2.f : 1.f;

                if((node.windowData().resizeEdges & Detail::WindowResizeLeft) != 0)
                {
                    drawList.line({resizeBounds.x, node.bounds.y}, {resizeBounds.x, node.bounds.bottom()}, edgeThickness, gripColor, baseKey);
                }

                if((node.windowData().resizeEdges & Detail::WindowResizeRight) != 0)
                {
                    drawList.line({resizeBounds.right(), node.bounds.y}, {resizeBounds.right(), node.bounds.bottom()}, edgeThickness, gripColor, baseKey);
                }

                if((node.windowData().resizeEdges & Detail::WindowResizeTop) != 0)
                {
                    drawList.line({node.bounds.x, resizeBounds.y}, {node.bounds.right(), resizeBounds.y}, edgeThickness, gripColor, baseKey);
                }

                if((node.windowData().resizeEdges & Detail::WindowResizeBottom) != 0)
                {
                    drawList.line({node.bounds.x, resizeBounds.bottom()}, {node.bounds.right(), resizeBounds.bottom()}, edgeThickness, gripColor, baseKey);
                }

                if(node.windowData().resizeGripVisible == true)
                {
                    float side = std::max(12.f, node.style->metrics.controlHeight * 0.7f);
                    Rect grip = {resizeBounds.right() - side, resizeBounds.bottom() - side, side, side};
                    for(float inset = 4.f; inset <= 10.f; inset += 3.f)
                    {
                        drawList.line({grip.right() - inset, grip.bottom() - 2.f}, {grip.right() - 2.f, grip.bottom() - inset}, 1.f, gripColor, baseKey);
                    }
                }
            }

            break;
        }
        case Detail::NodeKind::Text:
        {
            bool constrained = node.layout.width.rule != SizeRule::Content;
            if(constrained) drawList.pushClip(Rect::intersection(node.clip, node.bounds), baseKey);
            emitText(drawList, node, {node.bounds.x, node.bounds.y + (node.alignTextToFramePadding == true ? node.style->metrics.framePadding.top : 0.f)}, textColor, baseState, baseKey);
            if(constrained) drawList.popClip(baseKey);
            break;
        }
        case Detail::NodeKind::Bullet:
        {
            float side = 4.f;
            drawList.roundedRect({node.bounds.x + std::max(0.f, node.style->metrics.indent * 0.5f - side * 0.5f), node.bounds.y + node.style->metrics.lineHeight * 0.5f - side * 0.5f, side, side}, side * 0.5f, textColor, baseKey);
            break;
        }
        case Detail::NodeKind::BulletText:
        {
            float side = 4.f;
            drawList.roundedRect({node.bounds.x + std::max(0.f, node.style->metrics.indent * 0.5f - side * 0.5f), node.bounds.y + node.style->metrics.lineHeight * 0.5f - side * 0.5f, side, side}, side * 0.5f, textColor, baseKey);
            emitText(drawList, node, {node.bounds.x + node.style->metrics.indent, node.bounds.y}, textColor, baseState, baseKey);
            break;
        }
        case Detail::NodeKind::Button:
        case Detail::NodeKind::Selectable:
        case Detail::NodeKind::TableRow:
        case Detail::NodeKind::Tab:
        {
            Rect frameBounds = node.bounds;
            frameBounds.y += activeVisual * node.style->behavior.pressOffset;
            bool tabNode = node.kind == Detail::NodeKind::Tab;
            bool dimmedTab = tabNode && node.windowOwner != InvalidId && Mosaic::windowFocused(this, node.windowOwner) == false;
            bool selectableNode = node.kind == Detail::NodeKind::Selectable || node.kind == Detail::NodeKind::TableRow;
            bool angledHeader = node.kind == Detail::NodeKind::Selectable && node.tableItem().header == true && node.tableItem().columnOptions.angledHeader;
            bool sortedTableHeader = node.kind == Detail::NodeKind::Selectable && node.tableItem().header == true && node.tableItem().sortDirection != SortDirection::None;
            bool menuNode = node.semanticRole == SemanticRole::MenuItem;
            float emphasis = std::max({hoverVisual, activeVisual, navigationFocusVisual});
            float activationVisual = visualState == nullptr ? 0.f : visualState->activationVisual;
            Color frameBorder = Detail::colorWithAlpha(controlBorder, 0.55f + emphasis * 0.45f);
            Color frameFill = tabNode ? Detail::mixColor(dimmedTab ? node.style->colors.tabDimmed : node.style->colors.tab, node.style->colors.tabHovered, hoverVisual) : Detail::mixColor(node.style->colors.button, node.style->colors.buttonHovered, hoverVisual);

            if(tabNode == true)
            {
                frameFill = Detail::mixColor(frameFill, node.style->colors.tabActive, activeVisual);
                frameFill = Detail::mixColor(frameFill, dimmedTab ? node.style->colors.tabDimmedSelected : node.style->colors.tabSelected, selectionVisual);
            }

            if(menuNode == true)
            {
                frameFill = Detail::mixColor(node.style->colors.menu, node.style->colors.menuHovered, hoverVisual);
                frameFill = Detail::mixColor(frameFill, node.style->colors.menuActive, std::max(activeVisual, selectionVisual));
            }
            else
            {
                frameFill = Detail::mixColor(frameFill, node.style->colors.buttonActive, activeVisual);
            }

            if(tabNode == false && selectableNode == false && menuNode == false)
            {
                frameFill = Detail::mixColor(frameFill, node.style->colors.headerActive, selectionVisual);
            }

            if(selectableNode == true)
            {
                frameFill = Detail::mixColor(node.style->colors.background, node.style->colors.headerActive, selectionVisual);
                frameFill = Detail::mixColor(frameFill, node.style->colors.headerHovered, hoverVisual);
            }

            if(tabNode == false && selectableNode == false && node.semanticRole != SemanticRole::Text)
            {
                frameFill = Detail::mixColor(frameFill, accentColor, activationVisual * 0.18f);
            }

            if(tabNode == true)
            {
                textColor = Detail::mixColor(node.style->colors.textDisabled, node.style->colors.text, std::max(hoverVisual, selectionVisual));
            }

            bool drawBackground = node.fillBackground || tabNode == true || selectableNode == true || (node.fillHoverBackground && (emphasis > 0.001f || selectionVisual > 0.001f));

            if(drawBackground == true)
            {
                if(node.fillBackground == false && tabNode == false && selectableNode == false)
                {
                    frameFill = Detail::mixColor(Color{node.style->colors.background.r, node.style->colors.background.g, node.style->colors.background.b, 0.f}, menuNode ? node.style->colors.menuHovered : node.style->colors.headerHovered, std::max(hoverVisual, selectionVisual));
                }

                Detail::drawFrame(drawList, frameBounds, tabNode ? node.style->metrics.tabCornerRadius : menuNode ? node.style->metrics.menuItemCornerRadius : node.style->metrics.frameCornerRadius, tabNode ? node.style->metrics.tabBorderSize : (selectableNode || menuNode || node.fillBackground == false) ? 0.f : node.style->metrics.frameBorderSize, frameFill, frameBorder, baseKey);
            }

            if(navigationFocusVisual > 0.001f)
            {
                float halfWidth = std::max(0.f, frameBounds.width - 4.f) * 0.5f * navigationFocusVisual;
                float center = frameBounds.x + frameBounds.width * 0.5f;
                drawList.line({center - halfWidth, frameBounds.bottom() - 1.f}, {center + halfWidth, frameBounds.bottom() - 1.f}, 1.f, node.style->colors.navigationCursor, baseKey);
            }

            if(tabNode == true && selectionVisual > 0.001f)
            {
                constexpr float underlineInset = 1.f;
                float indicatorHeight = std::clamp(node.style->metrics.tabOverlineSize, 0.f, frameBounds.height);
                float indicatorY = node.tabSelectedOverline ? frameBounds.y : frameBounds.bottom() - indicatorHeight;
                drawList.roundedRect({frameBounds.x + underlineInset, indicatorY, std::max(0.f, (frameBounds.width - underlineInset * 2.f) * selectionVisual), indicatorHeight}, std::min(1.f, indicatorHeight * 0.5f), dimmedTab ? node.style->colors.tabDimmedSelectedOverline : node.style->colors.tabSelectedOverline, baseKey);
            }

            float closeExtent = tabNode && node.tabCloseVisible ? std::min(frameBounds.height, node.style->metrics.controlHeight * 0.7f) : 0.f;
            float unsavedExtent = tabNode && node.tabUnsavedDocument ? node.style->metrics.fontSize * 0.75f : 0.f;
            float sortExtent = sortedTableHeader ? 12.f : 0.f;
            float menuCheckExtent = menuNode && node.menuPopupItem ? node.style->metrics.controlHeight * 0.75f : 0.f;
            float menuShortcutExtent = menuNode && node.valueText.empty() == false ? node.valueData().valueTextSize.x + node.style->metrics.innerSpacing.x : 0.f;
            float menuSubmenuExtent = menuNode && node.menuSubmenu ? node.style->metrics.controlHeight * 0.75f : 0.f;
            float contentWidth = std::max(0.f, frameBounds.width - node.style->metrics.padding * 2.f - closeExtent - unsavedExtent - sortExtent - menuCheckExtent - menuShortcutExtent - menuSubmenuExtent);
            Vec2 textAlignment = selectableNode && node.overrideTextAlignment ? node.textAlignment : selectableNode ? node.style->metrics.selectableTextAlignment : node.style->metrics.buttonTextAlignment;
            float alignment = std::clamp(textAlignment.x, 0.f, 1.f);
            float textX = frameBounds.x + node.style->metrics.padding + menuCheckExtent + std::max(0.f, contentWidth - node.textSize.x) * alignment;

            if(node.arrowButton == true)
            {
                float centerX = frameBounds.x + frameBounds.width * 0.5f;
                float centerY = frameBounds.y + frameBounds.height * 0.5f;
                constexpr float extent = 3.5f;
                Vec2 first;
                Vec2 middle;
                Vec2 last;
                switch(node.direction)
                {
                case Direction::Left:
                    first = {centerX + extent * 0.5f, centerY - extent};
                    middle = {centerX - extent * 0.5f, centerY};
                    last = {centerX + extent * 0.5f, centerY + extent};
                    break;
                case Direction::Right:
                    first = {centerX - extent * 0.5f, centerY - extent};
                    middle = {centerX + extent * 0.5f, centerY};
                    last = {centerX - extent * 0.5f, centerY + extent};
                    break;
                case Direction::Up:
                    first = {centerX - extent, centerY + extent * 0.5f};
                    middle = {centerX, centerY - extent * 0.5f};
                    last = {centerX + extent, centerY + extent * 0.5f};
                    break;
                case Direction::Down:
                    first = {centerX - extent, centerY - extent * 0.5f};
                    middle = {centerX, centerY + extent * 0.5f};
                    last = {centerX + extent, centerY - extent * 0.5f};
                    break;
                }
                drawList.line(first, middle, 1.5f, textColor, baseKey);
                drawList.line(middle, last, 1.5f, textColor, baseKey);
            }
            else if(node.kind != Detail::NodeKind::TableRow)
            {
                if(angledHeader == true && node.textRun != nullptr)
                {
                    constexpr float radiansPerDegree = 0.0174532925f;
                    float angle = std::clamp(node.style->metrics.tableAngledHeadersAngleDegrees, -80.f, 80.f) * radiansPerDegree;
                    float sine = std::sin(angle);
                    float cosine = std::cos(angle);
                    Vec2 axisX = {cosine, -sine};
                    Vec2 axisY = {sine, cosine};
                    Array<Vec2, 4> corners = {{{}, {node.textSize.x * axisX.x, node.textSize.x * axisX.y}, {node.textSize.y * axisY.x, node.textSize.y * axisY.y}, {node.textSize.x * axisX.x + node.textSize.y * axisY.x, node.textSize.x * axisX.y + node.textSize.y * axisY.y}}};
                    const Vec2 & firstCorner = corners.front();
                    float minimumX = firstCorner.x;
                    float minimumY = firstCorner.y;
                    float maximumX = minimumX;
                    float maximumY = minimumY;
                    for(const Vec2 & corner : corners)
                    {
                        minimumX = std::min(minimumX, corner.x);
                        minimumY = std::min(minimumY, corner.y);
                        maximumX = std::max(maximumX, corner.x);
                        maximumY = std::max(maximumY, corner.y);
                    }
                    float footprintWidth = maximumX - minimumX;
                    float footprintHeight = maximumY - minimumY;
                    float alignment = std::clamp(node.style->metrics.tableAngledHeadersTextAlignment, 0.f, 1.f);
                    float availableWidth = std::max(0.f, frameBounds.width - node.style->metrics.framePadding.left - node.style->metrics.framePadding.right);
                    float availableHeight = std::max(0.f, frameBounds.height - node.style->metrics.framePadding.top - node.style->metrics.framePadding.bottom);
                    Vec2 position = {frameBounds.x + node.style->metrics.framePadding.left + std::max(0.f, availableWidth - footprintWidth) * alignment - minimumX, frameBounds.y + node.style->metrics.framePadding.top + std::max(0.f, availableHeight - footprintHeight) * alignment - minimumY};
                    emitPreparedText(drawList, *node.textRun, position, textColor, baseState, baseKey, axisX, axisY);
                }
                else if(node.tableItem().columnOptions.headerLabelVisible == true || node.tableItem().header == false)
                {
                    float verticalAlignment = std::clamp(textAlignment.y, 0.f, 1.f);
                    emitText(drawList, node, {textX, frameBounds.y + std::max(0.f, frameBounds.height - node.style->metrics.lineHeight) * verticalAlignment}, textColor, baseState, baseKey);
                }

                if(node.hyperlink == true && hoverVisual > 0.001f)
                {
                    float underlineY = frameBounds.y + (frameBounds.height + node.style->metrics.lineHeight) * 0.5f - 1.f;
                    drawList.line({textX, underlineY}, {textX + node.textSize.x, underlineY}, 1.f, Detail::colorWithAlpha(textColor, hoverVisual), baseKey);
                }
            }

            if(sortedTableHeader == true)
            {
                float centerX = frameBounds.right() - node.style->metrics.padding - sortExtent * 0.5f;
                float centerY = frameBounds.y + frameBounds.height * 0.5f;
                constexpr float halfWidth = 3.5f;
                constexpr float halfHeight = 2.5f;
                bool ascending = node.tableItem().sortDirection == SortDirection::Ascending;
                Vec2 tip = {centerX, centerY + (ascending ? -halfHeight : halfHeight)};
                float baseY = centerY + (ascending ? halfHeight : -halfHeight);
                Color indicatorColor = Detail::mixColor(textColor, node.style->colors.accent, std::min(0.75f, static_cast<float>(node.tableItem().sortOrder) * 0.12f));
                drawList.line({centerX - halfWidth, baseY}, tip, 1.5f, indicatorColor, baseKey);
                drawList.line(tip, {centerX + halfWidth, baseY}, 1.5f, indicatorColor, baseKey);
            }

            if(menuNode == true && node.menuCheckVisible == true && node.checked == true)
            {
                float centerX = frameBounds.x + node.style->metrics.padding + menuCheckExtent * 0.45f;
                float centerY = frameBounds.y + frameBounds.height * 0.5f;
                drawList.line({centerX - 3.f, centerY}, {centerX - 1.f, centerY + 2.5f}, 1.5f, textColor, baseKey);
                drawList.line({centerX - 1.f, centerY + 2.5f}, {centerX + 4.f, centerY - 3.f}, 1.5f, textColor, baseKey);
            }

            if(menuNode == true && node.valueText.empty() == false)
            {
                emitValueText(drawList, node, {frameBounds.right() - node.style->metrics.padding - menuSubmenuExtent - node.valueData().valueTextSize.x, frameBounds.y + std::max(0.f, frameBounds.height - node.style->metrics.lineHeight) * 0.5f}, node.style->colors.textDisabled, baseState, baseKey);
            }

            if(menuNode == true && node.menuSubmenu == true)
            {
                float centerX = frameBounds.right() - node.style->metrics.padding - menuSubmenuExtent * 0.45f;
                float centerY = frameBounds.y + frameBounds.height * 0.5f;
                drawList.line({centerX - 2.f, centerY - 3.f}, {centerX + 1.f, centerY}, 1.f, textColor, baseKey);
                drawList.line({centerX + 1.f, centerY}, {centerX - 2.f, centerY + 3.f}, 1.f, textColor, baseKey);
            }

            if(tabNode == true && node.tabCloseVisible == true)
            {
                float centerX = frameBounds.right() - closeExtent * 0.5f;
                float centerY = frameBounds.y + frameBounds.height * 0.5f;
                float extent = 3.f;
                Color closeColor = node.tabCloseHovered ? node.style->colors.text : node.style->colors.textDisabled;
                drawList.line({centerX - extent, centerY - extent}, {centerX + extent, centerY + extent}, 1.f, closeColor, baseKey);
                drawList.line({centerX + extent, centerY - extent}, {centerX - extent, centerY + extent}, 1.f, closeColor, baseKey);
            }

            if(tabNode == true && node.tabUnsavedDocument == true)
            {
                constexpr float markerSide = 4.f;
                float markerCenter = frameBounds.right() - closeExtent - unsavedExtent * 0.5f;
                drawList.roundedRect({markerCenter - markerSide * 0.5f, frameBounds.y + (frameBounds.height - markerSide) * 0.5f, markerSide, markerSide}, markerSide * 0.5f, node.style->colors.unsavedMarker, baseKey);
            }

            break;
        }
        case Detail::NodeKind::Toggle:
        {
            Rect frameBounds = node.bounds;
            frameBounds.y += activeVisual * node.style->behavior.pressOffset;
            float switchHeight = std::min(12.f, frameBounds.height - 4.f);
            float switchWidth = 24.f;
            Rect switchBounds = {frameBounds.right() - node.style->metrics.padding - switchWidth, frameBounds.y + (frameBounds.height - switchHeight) * 0.5f, switchWidth, switchHeight};
            Color switchFill = Detail::mixColor(controlColor, accentColor, selectionVisual);
            Detail::drawFrame(drawList, switchBounds, switchHeight * 0.5f, node.style->metrics.frameBorderSize, switchFill, controlBorder, baseKey);
            float knobSide = switchHeight - 4.f;
            float knobTravel = std::max(0.f, switchWidth - knobSide - 4.f);
            Rect knob = {switchBounds.x + 2.f + knobTravel * selectionVisual, switchBounds.y + 2.f, knobSide, knobSide};
            drawList.roundedRect({knob.x, knob.y + 1.f, knob.width, knob.height}, knobSide * 0.5f, Color{0.f, 0.f, 0.f, 0.28f}, baseKey);
            drawList.roundedRect(knob, knobSide * 0.5f, Detail::mixColor(textColor, Color{1.f, 1.f, 1.f, 1.f}, node.disabled ? 0.f : selectionVisual * 0.35f), baseKey);
            emitText(drawList, node, {frameBounds.x + node.style->metrics.padding, frameBounds.y + (frameBounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
            break;
        }
        case Detail::NodeKind::Tree:
        {
            Rect headerBounds = {node.bounds.x, node.bounds.y, node.bounds.width, node.style->metrics.controlHeight};
            headerBounds.y += activeVisual * node.style->behavior.pressOffset;
            Rect treeFrameBounds = node.treeData().frameBounds.empty() == true ? headerBounds : node.treeData().frameBounds;
            treeFrameBounds.y += activeVisual * node.style->behavior.pressOffset;
            RenderState treeFrameState = baseState;
            treeFrameState.clip = node.treeData().frameClip.empty() == true ? baseState.clip : node.treeData().frameClip;
            uint64_t treeFrameKey = internRenderState(treeFrameState);
            RenderState treeLabelState = baseState;
            treeLabelState.clip = node.treeData().labelClip.empty() == true ? baseState.clip : node.treeData().labelClip;
            uint64_t treeLabelKey = internRenderState(treeLabelState);

            if(node.treeData().lines != TreeLineMode::None)
            {
                size_t parent = node.parent;
                while(parent != 0 && nodes[parent].kind != Detail::NodeKind::Tree)
                {
                    parent = nodes[parent].parent;
                }

                if(nodes[parent].kind == Detail::NodeKind::Tree)
                {
                    float branchX = headerBounds.x - node.style->metrics.indent * 0.5f;
                    float centerY = headerBounds.y + headerBounds.height * 0.5f;
                    float branchEndX = headerBounds.x + 5.f;

                    if(node.treeData().lines == TreeLineMode::Full)
                    {
                        Rect parentHeader = {nodes[parent].bounds.x, nodes[parent].bounds.y, nodes[parent].bounds.width, nodes[parent].style->metrics.controlHeight};
                        float branchBeginY = parentHeader.bottom();
                        float rounding = std::clamp(node.style->metrics.treeLinesRounding, 0.f, std::min(std::abs(centerY - branchBeginY), std::abs(branchEndX - branchX)));

                        if(rounding > 0.f)
                        {
                            float verticalEndY = centerY - rounding;
                            drawList.line({branchX, branchBeginY}, {branchX, verticalEndY}, node.style->metrics.treeLinesSize, node.style->colors.treeLines, baseKey);
                            drawList.quadraticBezier({branchX, verticalEndY}, {branchX, centerY}, {branchX + rounding, centerY}, node.style->metrics.treeLinesSize, node.style->colors.treeLines, baseKey);
                            drawList.line({branchX + rounding, centerY}, {branchEndX, centerY}, node.style->metrics.treeLinesSize, node.style->colors.treeLines, baseKey);
                        }
                        else
                        {
                            drawList.line({branchX, branchBeginY}, {branchX, centerY}, node.style->metrics.treeLinesSize, node.style->colors.treeLines, baseKey);
                            drawList.line({branchX, centerY}, {branchEndX, centerY}, node.style->metrics.treeLinesSize, node.style->colors.treeLines, baseKey);
                        }
                    }
                    else
                    {
                        drawList.line({branchX, centerY}, {branchEndX, centerY}, node.style->metrics.treeLinesSize, node.style->colors.treeLines, baseKey);
                    }
                }
            }

            if(node.treeData().framed == true || node.selected == true || hoverVisual > 0.001f || activeVisual > 0.001f || focusVisual > 0.001f)
            {
                Color idleFill = node.selected ? node.style->colors.selection : node.treeData().framed ? node.style->colors.header : Detail::colorWithAlpha(node.style->colors.header, 0.f);
                Color treeFill = Detail::mixColor(idleFill, node.style->colors.headerHovered, std::max(hoverVisual, focusVisual * 0.72f));
                treeFill = Detail::mixColor(treeFill, node.style->colors.headerActive, activeVisual);
                Color treeBorder = Detail::mixColor(node.style->colors.border, node.style->colors.accent, std::max(activeVisual, focusVisual * 0.5f));
                Detail::drawFrame(drawList, treeFrameBounds, node.style->metrics.frameCornerRadius, 0.f, treeFill, treeBorder, treeFrameKey);
            }

            float centerY = headerBounds.y + headerBounds.height * 0.5f;

            if(node.treeData().bullet == true)
            {
                constexpr float side = 4.f;
                drawList.roundedRect({headerBounds.x + 10.f, centerY - side * 0.5f, side, side}, side * 0.5f, textColor, baseKey);
            }
            else if(node.treeData().leaf == false)
            {
                Vec2 closedFirst = {headerBounds.x + 9.f, centerY - 4.f};
                Vec2 closedMiddle = {headerBounds.x + 13.f, centerY};
                Vec2 closedLast = {headerBounds.x + 9.f, centerY + 4.f};
                Vec2 openFirst = {headerBounds.x + 8.f, centerY - 2.f};
                Vec2 openMiddle = {headerBounds.x + 12.f, centerY + 2.f};
                Vec2 openLast = {headerBounds.x + 16.f, centerY - 2.f};
                Vec2 arrowFirst = Detail::mixVector(closedFirst, openFirst, selectionVisual);
                Vec2 arrowMiddle = Detail::mixVector(closedMiddle, openMiddle, selectionVisual);
                Vec2 arrowLast = Detail::mixVector(closedLast, openLast, selectionVisual);
                drawList.line(arrowFirst, arrowMiddle, 1.5f, textColor, baseKey);
                drawList.line(arrowMiddle, arrowLast, 1.5f, textColor, baseKey);
            }

            emitText(drawList, node, {headerBounds.x + node.style->metrics.padding + (node.treeData().alignLabelWithCurrentX ? 0.f : node.style->metrics.indent), headerBounds.y + (headerBounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, treeLabelState, treeLabelKey);

            if(node.treeData().closeVisible == true)
            {
                float side = treeFrameBounds.height;
                Rect closeBounds = {treeFrameBounds.right() - side, treeFrameBounds.y, side, side};
                Color closeColor = Detail::mixColor(textColor, node.style->colors.accent, node.treeData().closeHovered ? 0.45f : 0.f);
                float centerX = closeBounds.x + closeBounds.width * 0.5f;
                float centerY = closeBounds.y + closeBounds.height * 0.5f;
                drawList.line({centerX - 4.f, centerY - 4.f}, {centerX + 4.f, centerY + 4.f}, 1.4f, closeColor, baseKey);
                drawList.line({centerX + 4.f, centerY - 4.f}, {centerX - 4.f, centerY + 4.f}, 1.4f, closeColor, baseKey);
            }

            break;
        }
        case Detail::NodeKind::IconButton:
        case Detail::NodeKind::ImageButton:
        {
            Rect frameBounds = node.bounds;
            frameBounds.y += activeVisual * node.style->behavior.pressOffset;
            Color imageBackground = node.imageData().backgroundEnabled ? Detail::mixColor(node.imageData().background, node.style->colors.buttonHovered, hoverVisual * 0.2f) : controlColor;
            Detail::drawFrame(drawList, frameBounds, node.style->metrics.cornerRadius, node.style->metrics.frameBorderSize, imageBackground, controlBorder, baseKey);

            if(node.imageData().texture != 0)
            {
                RenderState imageState = baseState;
                imageState.texture = node.imageData().texture;
                imageState.sampler = node.imageData().sampler;
                drawList.image(frameBounds.inset(node.imageData().padding), node.imageData().uv, node.tint, internRenderState(imageState));
            }

            break;
        }
        case Detail::NodeKind::Checkbox:
        {
            float side = std::min(12.f, std::min(node.bounds.height, node.style->metrics.controlHeight) - 4.f);
            Rect box = {node.bounds.x + 2.f, node.bounds.y + (node.bounds.height - side) * 0.5f + activeVisual * node.style->behavior.pressOffset, side, side};
            Color boxFill = Detail::mixColor(controlColor, node.style->colors.checkboxSelectedBackground, selectionVisual);
            Detail::drawFrame(drawList, box, node.style->metrics.cornerRadius * 0.65f, node.style->metrics.frameBorderSize, boxFill, controlBorder, baseKey);

            if(selectionVisual > 0.001f)
            {
                Vec2 center = {box.x + side * 0.5f, box.y + side * 0.5f};
                Vec2 first = Detail::mixVector(center, {box.x + side * 0.20f, box.y + side * 0.52f}, selectionVisual);
                Vec2 middle = Detail::mixVector(center, {box.x + side * 0.43f, box.y + side * 0.75f}, selectionVisual);
                Vec2 last = Detail::mixVector(center, {box.x + side * 0.82f, box.y + side * 0.25f}, selectionVisual);
                Color mark = Detail::colorWithAlpha(node.style->colors.checkMark, selectionVisual);
                drawList.line(first, middle, 1.5f, mark, baseKey);
                drawList.line(middle, last, 1.5f, mark, baseKey);
            }

            emitText(drawList, node, {box.right() + node.style->metrics.gap, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
            break;
        }
        case Detail::NodeKind::Combo:
        {
            Detail::ComboGeometry geometry = Detail::comboGeometry(node, node.bounds);
            Color previewFill = Detail::mixColor(node.style->colors.frame, node.style->colors.frameHovered, hoverVisual);
            Color border = Detail::mixColor(controlBorder, accentColor, std::max(focusVisual, selectionVisual * 0.7f));
            Detail::drawFrame(drawList, geometry.control, node.style->metrics.frameCornerRadius, node.style->metrics.frameBorderSize, previewFill, border, baseKey);

            if(node.comboShowArrow == true && geometry.arrow.empty() == false)
            {
                Color arrowFill = Detail::mixColor(node.style->colors.button, node.style->colors.buttonHovered, std::max(hoverVisual, selectionVisual));
                drawList.roundedRect(geometry.arrow, node.style->metrics.frameCornerRadius, arrowFill, baseKey);

                if(node.comboShowPreview == true)
                {
                    drawList.line({geometry.arrow.x, geometry.arrow.y + 1.f}, {geometry.arrow.x, geometry.arrow.bottom() - 1.f}, std::max(1.f, node.style->metrics.frameBorderSize), border, baseKey);
                }

                float centerX = geometry.arrow.x + geometry.arrow.width * 0.5f;
                float centerY = geometry.arrow.y + geometry.arrow.height * 0.5f;
                drawList.line({centerX - 4.f, centerY - 2.f}, {centerX, centerY + 2.f}, 1.f, textColor, baseKey);
                drawList.line({centerX, centerY + 2.f}, {centerX + 4.f, centerY - 2.f}, 1.f, textColor, baseKey);
            }

            if(node.comboShowPreview == true && node.valueText.empty() == false)
            {
                emitValueText(drawList, node, {geometry.preview.x + node.style->metrics.framePadding.left, geometry.preview.y + (geometry.preview.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
            }

            if(node.label.empty() == false && node.labelPlacement != LabelPlacement::Hidden)
            {
                emitText(drawList, node, {geometry.label.x, geometry.label.y + (geometry.label.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
            }

            break;
        }
        case Detail::NodeKind::Radio:
        {
            float side = std::min(node.bounds.height, node.style->metrics.controlHeight) - 5.f;
            Rect circle = {node.bounds.x + 2.f, node.bounds.y + (node.bounds.height - side) * 0.5f + activeVisual * node.style->behavior.pressOffset, side, side};
            Color circleFill = Detail::mixColor(controlColor, accentColor, selectionVisual * 0.22f);
            Detail::drawFrame(drawList, circle, side * 0.5f, node.style->metrics.frameBorderSize, circleFill, controlBorder, baseKey);

            if(selectionVisual > 0.001f)
            {
                float dotSide = side * 0.48f * selectionVisual;
                Rect dot = {circle.x + (side - dotSide) * 0.5f, circle.y + (side - dotSide) * 0.5f, dotSide, dotSide};
                drawList.roundedRect(dot, dotSide * 0.5f, Detail::colorWithAlpha(accentColor, selectionVisual), baseKey);
            }

            emitText(drawList, node, {circle.right() + node.style->metrics.gap, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
            break;
        }
        case Detail::NodeKind::Slider:
        case Detail::NodeKind::Progress:
        {
            bool sliderNode = node.kind == Detail::NodeKind::Slider;

            if(sliderNode == true && node.sliderVertical == true)
            {
                float grabLength = std::clamp(node.style->metrics.grabMinimumSize, 1.f, node.bounds.height);
                float grabHalf = grabLength * 0.5f;
                Rect track = {node.bounds.x + (node.bounds.width - 5.f) * 0.5f, node.bounds.y + grabHalf, 5.f, std::max(0.f, node.bounds.height - grabLength)};
                Detail::drawFrame(drawList, track, track.width * 0.5f, node.style->metrics.frameBorderSize, Detail::mixColor(node.style->colors.frame, node.style->colors.frameHovered, hoverVisual), Detail::mixColor(controlBorder, accentColor, hoverVisual * 0.5f), baseKey);
                float scalar = std::clamp(scalarVisual, 0.f, 1.f);
                float centerY = track.bottom() - track.height * scalar;
                Rect fill = {track.x, centerY, track.width, track.bottom() - centerY};

                if(fill.height > 0.f)
                {
                    drawList.roundedRect(fill, track.width * 0.5f, Detail::mixColor(accentColor, textColor, hoverVisual * 0.28f), baseKey);
                }

                float knobWidth = std::max(1.f, node.bounds.width - 4.f);
                float knobHeight = std::min(node.bounds.height, grabLength + hoverVisual + activeVisual * 2.f);
                Rect knob = {node.bounds.x + (node.bounds.width - knobWidth) * 0.5f, centerY - knobHeight * 0.5f, knobWidth, knobHeight};
                float knobRadius = std::min(knobHeight * 0.5f, std::max(0.f, node.style->metrics.grabCornerRadius));
                drawList.roundedRect({knob.x, knob.y + 1.f, knob.width, knob.height}, knobRadius, Color{0.f, 0.f, 0.f, 0.34f}, baseKey);
                drawList.roundedRect(knob, knobRadius, Detail::mixColor(node.style->colors.sliderGrab, node.style->colors.sliderGrabActive, activeVisual), baseKey);

                if(node.showValueOnTrack == true && node.valueText.empty() == false)
                {
                    emitValueText(drawList, node, {node.bounds.x + (node.bounds.width - node.valueData().valueTextSize.x) * 0.5f, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
                }

                break;
            }

            Detail::SliderGeometry geometry = Detail::sliderGeometry(node, node.bounds);

            if(sliderNode == true && node.textEditData().numeric == true)
            {
                Detail::drawFrame(drawList, geometry.control, node.style->metrics.frameCornerRadius, node.style->metrics.frameBorderSize, node.style->colors.input, node.style->colors.accent, baseKey);

                if(node.valueData().colorMarkerEnabled == true)
                {
                    Rect marker = {geometry.control.x, geometry.control.y, std::min(3.f, geometry.control.width), geometry.control.height};
                    drawList.roundedRect(marker, std::min(node.style->metrics.frameCornerRadius, 1.5f), node.colorMarker, baseKey);
                }

                if(node.label.empty() == false && node.labelPlacement != LabelPlacement::Hidden)
                {
                    emitText(drawList, node, {geometry.label.x, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
                }

                emitValueText(drawList, node, {geometry.control.x + node.style->metrics.framePadding.left, geometry.control.y + (geometry.control.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
                break;
            }

            float progressHeight = std::max(8.f, node.bounds.height - 8.f);
            Rect track = sliderNode ? geometry.track : Rect{node.bounds.x, node.bounds.y + (node.bounds.height - progressHeight) * 0.5f, node.bounds.width, progressHeight};
            Detail::drawFrame(drawList, track, track.height * 0.5f, node.style->metrics.frameBorderSize, Detail::mixColor(node.style->colors.frame, node.style->colors.frameHovered, hoverVisual), Detail::mixColor(controlBorder, accentColor, hoverVisual * 0.5f), baseKey);

            if(sliderNode == true && node.valueData().colorMarkerEnabled == true)
            {
                Rect marker = {geometry.control.x, geometry.control.y, std::min(3.f, geometry.control.width), geometry.control.height};
                drawList.roundedRect(marker, std::min(node.style->metrics.frameCornerRadius, 1.5f), node.colorMarker, baseKey);
            }

            Rect fill = track;

            if(sliderNode == false && node.selected == true)
            {
                float segment = track.width * 0.28f;
                float travel = track.width + segment;
                fill.x = track.x - segment + travel * std::clamp(node.valueData().secondaryScalar, 0.f, 1.f);
                fill.width = segment;
                Rect clipped = Rect::intersection(fill, track);
                fill = clipped;
            }
            else
            {
                fill.width *= std::clamp(scalarVisual, 0.f, 1.f);
            }

            if(fill.width > 0.f)
            {
                Color fillColor = Detail::mixColor(accentColor, Detail::mixColor(accentColor, textColor, 0.28f), hoverVisual);
                drawList.roundedRect(fill, track.height * 0.5f, fillColor, baseKey);
            }

            if(sliderNode == true)
            {
                float centerX = track.x + track.width * std::clamp(scalarVisual, 0.f, 1.f);
                float knobSide = std::min(geometry.maximumGrabSide, 7.f + hoverVisual + activeVisual);
                Rect knob = {centerX - knobSide * 0.5f, node.bounds.y + (node.bounds.height - knobSide) * 0.5f, knobSide, knobSide};
                float knobRadius = std::min(knobSide * 0.5f, std::max(0.f, node.style->metrics.grabCornerRadius));
                drawList.roundedRect({knob.x, knob.y + 1.f, knob.width, knob.height}, knobRadius, Color{0.f, 0.f, 0.f, 0.34f}, baseKey);
                drawList.roundedRect(knob, knobRadius, Detail::mixColor(Detail::mixColor(node.style->colors.sliderGrab, node.style->colors.sliderGrabHovered, hoverVisual), node.style->colors.sliderGrabActive, activeVisual), baseKey);
                if(hoverVisual > 0.001f || activeVisual > 0.001f)
                {
                    float halo = 2.f + activeVisual;
                    Rect haloBounds = {knob.x - halo, knob.y - halo, knob.width + halo * 2.f, knob.height + halo * 2.f};
                    BoxStyle haloStyle;
                    haloStyle.radii = {3.f, 3.f, 3.f, 3.f};
                    haloStyle.borderWidth = 1.f;
                    haloStyle.borderColor = Detail::colorWithAlpha(accentColor, std::max(hoverVisual * 0.35f, activeVisual * 0.65f));
                    drawList.box(haloBounds, haloStyle, baseKey);
                }

                if(node.showValueOnTrack == true && node.valueText.empty() == false)
                {
                    emitValueText(drawList, node, {geometry.control.x + (geometry.control.width - node.valueData().valueTextSize.x) * 0.5f, geometry.control.y + (geometry.control.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
                }

                float tooltipVisual = node.showValueTooltip && activeVisual <= 0.001f && hoverDuration >= static_cast<double>(node.tooltipDelay) ? std::clamp(static_cast<float>((hoverDuration - static_cast<double>(node.tooltipDelay)) / 0.12), 0.f, 1.f) * hoverVisual : 0.f;
                float valuePopupVisual = std::max(node.showValuePopup ? activeVisual : 0.f, tooltipVisual);

                if(valuePopupVisual > 0.001f && node.valueText.empty() == false)
                {
                    float bubbleWidth = node.valueData().valueTextSize.x + 12.f;
                    float bubbleHeight = node.style->metrics.lineHeight + 8.f;
                    float minimumX = baseState.clip.x + 2.f;
                    float maximumX = std::max(minimumX, baseState.clip.right() - bubbleWidth - 2.f);
                    float bubbleX = std::clamp(centerX - bubbleWidth * 0.5f, minimumX, maximumX);
                    float bubbleY = knob.y - bubbleHeight - 7.f;
                    bool above = true;

                    if(bubbleY < baseState.clip.y + 2.f)
                    {
                        bubbleY = knob.bottom() + 7.f;
                        above = false;
                    }

                    Rect bubble = {bubbleX, bubbleY, bubbleWidth, bubbleHeight};
                    Detail::drawFrame(drawList, bubble, node.style->metrics.cornerRadius, node.style->metrics.frameBorderSize, Detail::colorWithAlpha(node.style->colors.panelHeader, valuePopupVisual * 0.98f), Detail::colorWithAlpha(node.style->colors.accent, valuePopupVisual), baseKey);
                    float pointerX = std::clamp(centerX, bubble.x + 6.f, bubble.right() - 6.f);
                    float pointerY = above ? bubble.bottom() : bubble.y;
                    float knobY = above ? knob.y - 1.f : knob.bottom() + 1.f;
                    drawList.line({pointerX - 3.f, pointerY}, {centerX, knobY}, 1.f, Detail::colorWithAlpha(node.style->colors.accent, valuePopupVisual), baseKey);
                    drawList.line({pointerX + 3.f, pointerY}, {centerX, knobY}, 1.f, Detail::colorWithAlpha(node.style->colors.accent, valuePopupVisual), baseKey);
                    emitValueText(drawList, node, {bubble.x + 6.f, bubble.y + (bubble.height - node.style->metrics.lineHeight) * 0.5f}, Detail::colorWithAlpha(node.style->colors.text, valuePopupVisual), baseState, baseKey);
                }

                if(node.label.empty() == false && node.labelPlacement != LabelPlacement::Hidden)
                {
                    emitText(drawList, node, {geometry.label.x, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
                }
            }
            else if(node.label.empty() == false)
            {
                emitText(drawList, node, {node.bounds.x + (node.bounds.width - node.textSize.x) * 0.5f, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
            }

            break;
        }
        case Detail::NodeKind::DragValue:
        {
            if(node.angleDial && node.textEditData().temporaryNumeric == false)
            {
                float diameter = std::min(node.bounds.width, node.bounds.height) - 4.f;
                float radius = std::max(1.f, diameter * 0.5f);
                Vec2 center = {node.bounds.x + node.bounds.width * 0.5f, node.bounds.y + node.bounds.height * 0.5f};
                Rect dial = {center.x - radius, center.y - radius, radius * 2.f, radius * 2.f};
                Color ringColor = Detail::mixColor(node.style->colors.textDisabled, accentColor, std::max({hoverVisual, activeVisual, navigationFocusVisual}));
                Detail::drawFrame(drawList, dial, radius, 1.f, Detail::mixColor(node.style->colors.background, node.style->colors.frameHovered, hoverVisual), ringColor, baseKey);
                float angle = std::fmod(node.valueData().secondaryScalar, 360.f) * 0.0174532925f - 1.570796327f;
                Vec2 tip = {center.x + std::cos(angle) * (radius - 4.f), center.y + std::sin(angle) * (radius - 4.f)};
                drawList.line(center, tip, 1.5f, node.disabled ? textColor : accentColor, baseKey);
                drawList.roundedRect({center.x - 1.5f, center.y - 1.5f, 3.f, 3.f}, 1.5f, ringColor, baseKey);
                break;
            }

            Color inputFill = Detail::mixColor(node.style->colors.frame, node.style->colors.frameHovered, hoverVisual);
            Color inputBorder = Detail::mixColor(controlBorder, accentColor, std::max({hoverVisual * 0.55f, activeVisual, focusVisual}));

            if(node.validation == Validation::Warning)
            {
                inputBorder = node.style->colors.warning;
            }

            if(node.validation == Validation::Error)
            {
                inputBorder = node.style->colors.error;
            }

            bool editing = node.textEditData().temporaryNumeric;
            float emphasis = std::max({hoverVisual, activeVisual, navigationFocusVisual});
            if(editing || node.validation != Validation::Normal)
            {
                Detail::drawFrame(drawList, node.bounds, node.style->metrics.frameCornerRadius, node.style->metrics.frameBorderSize, inputFill, inputBorder, baseKey);
            }
            else
            {
                drawList.roundedRect(node.bounds, node.style->metrics.frameCornerRadius, Detail::colorWithAlpha(node.style->colors.frameHovered, emphasis * 0.6f), baseKey);
                float underlineWidth = std::min(std::max(0.f, node.bounds.width - node.style->metrics.padding * 2.f), std::max(node.textSize.x, node.valueData().valueTextSize.x));
                drawList.line({node.bounds.x + node.style->metrics.padding, node.bounds.bottom() - 2.f}, {node.bounds.x + node.style->metrics.padding + underlineWidth, node.bounds.bottom() - 2.f}, 1.f, Detail::colorWithAlpha(node.style->colors.textLink, 0.22f + emphasis * 0.6f), baseKey);
                textColor = node.disabled ? textColor : node.style->colors.textLink;
            }

            if(node.valueData().colorMarkerEnabled == true)
            {
                float markerWidth = std::min(node.style->metrics.colorMarkerSize, node.bounds.width);
                float markerHeight = std::min(8.f, node.bounds.height);
                Rect marker = {node.bounds.x, node.bounds.y + (node.bounds.height - markerHeight) * 0.5f, markerWidth, markerHeight};
                drawList.roundedRect(marker, std::min(node.style->metrics.frameCornerRadius, 1.5f), node.colorMarker, baseKey);
            }

            if(activeVisual > 0.001f)
            {
                drawList.line({node.bounds.x + 2.f, node.bounds.bottom() - 1.5f}, {node.bounds.right() - 2.f, node.bounds.bottom() - 1.5f}, 1.f, Detail::colorWithAlpha(node.style->colors.accent, activeVisual), baseKey);
            }

            RenderState dragTextState = baseState;
            uint64_t dragTextKey = baseKey;

            if(node.textEditData().temporaryNumeric == true)
            {
                dragTextState.clip = Rect::intersection(baseState.clip, node.bounds.inset(node.style->metrics.frameBorderSize + 1.f));
                dragTextKey = internRenderState(dragTextState);

                if(focused == node.id)
                {
                    Detail::drawTextSelection(this, drawList, node, Detail::colorWithAlpha(node.style->colors.textSelectionBackground, 0.82f), dragTextKey);
                }
            }

            if(node.textEditData().numeric == true)
            {
                emitValueText(drawList, node, {node.bounds.x + node.style->metrics.padding, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, dragTextState, dragTextKey);
            }
            else
            {
                emitText(drawList, node, {node.bounds.x + node.style->metrics.padding - node.textEditData().scrollX, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, dragTextState, dragTextKey);
            }

            if(node.textEditData().temporaryNumeric == true && focused == node.id)
            {
                Vec2 cursorPosition = Detail::textCursorPosition(this, node, node.textEditData().cursor);
                float blink = configuration.inputTextCursorBlink == false || std::fmod(static_cast<float>(input.timestamp), 1.f) < 0.58f ? 1.f : 0.18f;
                float cursorTop = cursorPosition.y + 2.f;
                float cursorBottom = std::min(node.bounds.bottom() - 3.f, cursorPosition.y + node.style->metrics.lineHeight - 2.f);
                drawList.line({cursorPosition.x, cursorTop}, {cursorPosition.x, cursorBottom}, 1.f, Detail::colorWithAlpha(node.style->colors.textCursor, blink), dragTextKey);
            }

            if(node.textEditData().temporaryNumeric == false && (hoverVisual > 0.001f || activeVisual > 0.001f) && node.bounds.width > std::max(node.textSize.x, node.valueData().valueTextSize.x) + node.style->metrics.padding * 2.f + 18.f)
            {
                float centerY = node.bounds.y + node.bounds.height * 0.5f;
                Color arrows = Detail::colorWithAlpha(node.style->colors.textDisabled, std::max(hoverVisual * 0.8f, activeVisual));
                drawList.line({node.bounds.right() - 13.f, centerY}, {node.bounds.right() - 9.f, centerY - 3.f}, 1.f, arrows, baseKey);
                drawList.line({node.bounds.right() - 13.f, centerY}, {node.bounds.right() - 9.f, centerY + 3.f}, 1.f, arrows, baseKey);
                drawList.line({node.bounds.right() - 4.f, centerY}, {node.bounds.right() - 8.f, centerY - 3.f}, 1.f, arrows, baseKey);
                drawList.line({node.bounds.right() - 4.f, centerY}, {node.bounds.right() - 8.f, centerY + 3.f}, 1.f, arrows, baseKey);
            }

            break;
        }
        case Detail::NodeKind::InputText:
        case Detail::NodeKind::InputMultiline:
        {
            Color inputFill = Detail::mixColor(node.style->colors.frame, node.style->colors.frameHovered, hoverVisual * 0.82f);

            if(node.readOnly == true)
            {
                inputFill = Detail::mixColor(inputFill, node.style->colors.background, 0.36f);
            }

            Color inputBorder = Detail::mixColor(controlBorder, accentColor, std::max(hoverVisual * 0.55f, focusVisual));

            if(node.validation == Validation::Warning)
            {
                inputBorder = node.style->colors.warning;
            }

            if(node.validation == Validation::Error)
            {
                inputBorder = node.style->colors.error;
            }

            Detail::drawFrame(drawList, node.bounds, node.style->metrics.cornerRadius, node.style->metrics.frameBorderSize, inputFill, inputBorder, baseKey);

            if(focusVisual > 0.001f)
            {
                drawList.line({node.bounds.x + node.style->metrics.cornerRadius, node.bounds.bottom() - 1.5f}, {node.bounds.right() - node.style->metrics.cornerRadius, node.bounds.bottom() - 1.5f}, 1.f, Detail::colorWithAlpha(node.style->colors.accent, focusVisual), baseKey);
            }

            RenderState inputTextState = baseState;
            inputTextState.clip = Rect::intersection(baseState.clip, node.bounds.inset(node.style->metrics.frameBorderSize + 1.f));
            uint64_t inputTextKey = internRenderState(inputTextState);

            if(focused == node.id)
            {
                Detail::drawTextSelection(this, drawList, node, Detail::colorWithAlpha(node.style->colors.textSelectionBackground, 0.82f), inputTextKey);
            }

            emitText(drawList, node, {node.bounds.x + node.style->metrics.padding - node.textEditData().scrollX, node.bounds.y + node.style->metrics.padding * 0.5f - node.textEditData().scrollY}, node.textEditData().hint ? node.style->colors.textDisabled : textColor, inputTextState, inputTextKey);

            if(focused == node.id)
            {
                Vec2 cursorPosition = Detail::textCursorPosition(this, node, node.textEditData().cursor);
                float blink = configuration.inputTextCursorBlink == false || std::fmod(static_cast<float>(input.timestamp), 1.f) < 0.58f ? 1.f : 0.18f;
                float cursorTop = cursorPosition.y + 2.f;
                float cursorBottom = std::min(node.bounds.bottom() - 3.f, cursorPosition.y + node.style->metrics.lineHeight - 2.f);
                drawList.line({cursorPosition.x, cursorTop}, {cursorPosition.x, cursorBottom}, 1.f, Detail::colorWithAlpha(node.style->colors.textCursor, blink), inputTextKey);
            }

            if(focused == node.id && node.textEditData().compositionBegin != node.textEditData().compositionEnd)
            {
                Vec2 begin = Detail::textCursorPosition(this, node, node.textEditData().compositionBegin);
                Vec2 end = Detail::textCursorPosition(this, node, node.textEditData().compositionEnd);
                float underlineY = begin.y + node.style->metrics.lineHeight - 2.f;
                drawList.line({begin.x, underlineY}, {std::max(begin.x + 1.f, end.x), underlineY}, 1.f, node.style->colors.accent, inputTextKey);
            }

            break;
        }
        case Detail::NodeKind::ColorEdit:
        {
            Detail::ColorEditGeometry geometry = Detail::colorEditGeometry(node, node.bounds);
            const ColorTextData * colorText = findColorTextData(node);

            if(node.colorShowInputs == false)
            {
                Detail::drawFrame(drawList, geometry.control, node.style->metrics.cornerRadius, node.colorBorder ? node.style->metrics.frameBorderSize : 0.f, controlColor, controlBorder, baseKey);
            }

            for(uint8_t channel = 0; channel != geometry.channelCount; ++channel)
            {
                Rect channelBounds = geometry.channels[channel];
                Detail::drawFrame(drawList, channelBounds, node.style->metrics.frameCornerRadius, node.colorBorder ? node.style->metrics.frameBorderSize : 0.f, node.style->colors.input, controlBorder, baseKey);
                Color markers[] = {Color{1.f, 0.08f, 0.08f, 1.f}, Color{0.12f, 0.92f, 0.18f, 1.f}, Color{0.18f, 0.36f, 1.f, 1.f}, Color{0.72f, 0.78f, 0.84f, 1.f}};

                if(node.colorMarkers == true)
                {
                    float markerWidth = std::min(node.style->metrics.colorMarkerSize, channelBounds.width);
                    drawList.rect({channelBounds.x, channelBounds.y, markerWidth, channelBounds.height}, markers[channel], baseKey);
                }

                Vec2 valueSize = colorText == nullptr ? Vec2{} : colorText->sizes[channel];

                if(colorText != nullptr && colorText->runs[channel] != nullptr)
                {
                    emitPreparedText(drawList, *colorText->runs[channel], {channelBounds.x + std::max(4.f, (channelBounds.width - valueSize.x) * 0.5f), channelBounds.y + (channelBounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
                }
            }

            if(node.label.empty() == false && node.labelPlacement != LabelPlacement::Hidden)
            {
                emitText(drawList, node, {geometry.label.x, node.bounds.y + (node.bounds.height - node.style->metrics.lineHeight) * 0.5f}, textColor, baseState, baseKey);
            }

            Rect swatch = geometry.preview.inset(node.colorShowInputs ? 1.f : 3.f);
            constexpr float checkerSide = 5.f;
            for(float y = 0.f; node.colorShowPreview == true && node.colorAlphaBackground == true && y < swatch.height; y += checkerSide)
            {
                for(float x = 0.f; x < swatch.width; x += checkerSide)
                {
                    int column = static_cast<int>(x / checkerSide);
                    int row = static_cast<int>(y / checkerSide);
                    drawList.rect({swatch.x + x, swatch.y + y, std::min(checkerSide, swatch.width - x), std::min(checkerSide, swatch.height - y)}, ((column + row) & 1) == 0 ? Color::fromBytes(160, 165, 174) : Color::fromBytes(78, 83, 92), baseKey);
                }
            }

            if(node.colorShowPreview == true)
            {
                drawList.rect(swatch, node.tint, baseKey);

                if(node.colorAlphaPreviewHalf == true)
                {
                    Color opaque = node.tint;
                    opaque.a = 1.f;
                    drawList.rect({swatch.x + swatch.width * 0.5f, swatch.y, swatch.width * 0.5f, swatch.height}, opaque, baseKey);
                }

                if(node.colorBorder == true)
                {
                    drawList.line({swatch.x, swatch.y}, {swatch.right(), swatch.y}, 1.f, node.style->colors.borderStrong, baseKey);
                    drawList.line({swatch.right(), swatch.y}, {swatch.right(), swatch.bottom()}, 1.f, node.style->colors.borderStrong, baseKey);
                    drawList.line({swatch.right(), swatch.bottom()}, {swatch.x, swatch.bottom()}, 1.f, node.style->colors.borderStrong, baseKey);
                    drawList.line({swatch.x, swatch.bottom()}, {swatch.x, swatch.y}, 1.f, node.style->colors.borderStrong, baseKey);
                }
            }

            if(selectionVisual > 0.001f)
            {
                drawList.roundedRect({node.bounds.x + 2.f, node.bounds.y + 3.f, 2.f, std::max(0.f, node.bounds.height - 6.f)}, 1.f, Detail::colorWithAlpha(node.style->colors.accent, selectionVisual), baseKey);
            }

            break;
        }
        case Detail::NodeKind::Separator:
        {
            float y = node.bounds.y + node.bounds.height * 0.5f;
            float size = std::max(0.f, node.style->metrics.separatorSize);

            if(size > 0.f)
            {
                drawList.line({node.bounds.x, y}, {node.bounds.right(), y}, size, node.style->colors.borderStrong, baseKey);
            }

            break;
        }
        case Detail::NodeKind::SeparatorText:
        {
            float centerY = node.bounds.y + node.bounds.height * 0.5f;
            float available = std::max(0.f, node.bounds.width - node.textSize.x - node.style->metrics.separatorTextPadding.x * 2.f);
            float textX = node.bounds.x + node.style->metrics.separatorTextPadding.x + available * std::clamp(node.style->metrics.separatorTextAlignment.x, 0.f, 1.f);
            float leftEnd = std::max(node.bounds.x, textX - node.style->metrics.separatorTextPadding.x);
            float rightBegin = std::min(node.bounds.right(), textX + node.textSize.x + node.style->metrics.separatorTextPadding.x);
            float border = std::max(0.f, node.style->metrics.separatorTextBorderSize);

            if(border > 0.f)
            {
                if(leftEnd > node.bounds.x)
                {
                    drawList.line({node.bounds.x, centerY}, {leftEnd, centerY}, border, node.style->colors.borderStrong, baseKey);
                }

                if(node.bounds.right() > rightBegin)
                {
                    drawList.line({rightBegin, centerY}, {node.bounds.right(), centerY}, border, node.style->colors.borderStrong, baseKey);
                }
            }

            emitText(drawList, node, {textX, node.bounds.y + std::max(0.f, node.bounds.height - node.style->metrics.lineHeight) * std::clamp(node.style->metrics.separatorTextAlignment.y, 0.f, 1.f)}, textColor, baseState, baseKey);
            break;
        }
        case Detail::NodeKind::Image:
            if(node.imageData().backgroundEnabled == true || node.imageData().borderSize > 0.f)
            {
                Color background = node.imageData().backgroundEnabled == true ? node.imageData().background : Color{};
                Detail::drawFrame(drawList, node.bounds, node.imageData().rounding, node.imageData().borderSize, background, node.imageData().borderColor, baseKey);
            }

            if(node.imageData().texture != 0)
            {
                RenderState imageState = baseState;
                imageState.texture = node.imageData().texture;
                imageState.sampler = node.imageData().sampler;
                float imageInset = node.imageData().padding + std::max(0.f, node.imageData().borderSize);
                Rect imageBounds = {node.bounds.x + imageInset, node.bounds.y + imageInset, std::max(0.f, node.bounds.width - imageInset * 2.f), std::max(0.f, node.bounds.height - imageInset * 2.f)};
                float imageRadius = std::max(0.f, node.imageData().rounding - imageInset);
                uint64_t imageKey = internRenderState(imageState);

                if(imageRadius > 0.f)
                {
                    drawList.roundedImage(imageBounds, node.imageData().uv, imageRadius, node.tint, imageKey);
                }
                else
                {
                    drawList.image(imageBounds, node.imageData().uv, node.tint, imageKey);
                }
            }

            break;
        case Detail::NodeKind::Canvas:
        {
            drawList.pushClip(canvasPass == CanvasLayer::Local ? node.clip : viewport.bounds, baseKey);
            DrawCommandVector & commands = canvasCommands(node, canvasPass);
            Vec2 offset = {node.content.x, node.content.y};
            Rect canvasClip = canvasPass == CanvasLayer::Local ? node.clip : viewport.bounds;
            uint32_t transformDepth = 0;
            auto emitCommand = [this, &drawList, baseKey, &offset, &canvasClip, &transformDepth](DrawCommand & command)
            {
                if(command.renderKey == 0)
                {
                    command.renderKey = baseKey;
                }
                else if(command.renderKey <= frame.renderStates.size())
                {
                    RenderState state = frame.renderStates[static_cast<size_t>(command.renderKey - 1)];
                    const RenderState & baseState = frame.renderStates[static_cast<size_t>(baseKey - 1)];
                    state.linePenumbra = baseState.linePenumbra;
                    state.fillFeather = baseState.fillFeather;
                    state.curveQuality = baseState.curveQuality;
                    state.ellipseQuality = baseState.ellipseQuality;
                    state.rectangleQuality = baseState.rectangleQuality;

                    if(state.clip.empty() == true)
                    {
                        state.clip = canvasClip;
                    }

                    command.renderKey = internRenderState(state);
                }

                switch(command.type)
                {
                case DrawCommandType::PushTransform:
                    if(transformDepth == 0)
                    {
                        command.payload.transform.transform.translation = command.payload.transform.transform.translation + offset;
                    }

                    ++transformDepth;
                    break;
                case DrawCommandType::PopTransform:
                    if(transformDepth != 0)
                    {
                        --transformDepth;
                    }

                    break;
                case DrawCommandType::Rect:
                case DrawCommandType::RoundedRect:
                case DrawCommandType::Image:
                case DrawCommandType::PushClip:
                    if(transformDepth == 0)
                    {
                        command.payload.rectangle.bounds.x += offset.x;
                        command.payload.rectangle.bounds.y += offset.y;
                    }

                    break;
                case DrawCommandType::RectBatch:
                    if(transformDepth == 0)
                    {
                        for(RectInstance & instance : drawCommandStorage.mutableRectangleSpan(command.payload.rectBatch.instances))
                        {
                            instance.bounds.x += offset.x;
                            instance.bounds.y += offset.y;
                        }
                    }

                    break;
                case DrawCommandType::QuadBatch:
                    if(transformDepth == 0)
                    {
                        for(QuadInstance & instance : drawCommandStorage.mutableQuadSpan(command.payload.quadBatch.instances))
                        {
                            for(Vertex & vertex : instance.vertices)
                            {
                                vertex.position = vertex.position + offset;
                            }
                        }
                    }

                    break;
                case DrawCommandType::Box:
                    if(transformDepth == 0)
                    {
                        command.payload.box.bounds.x += offset.x;
                        command.payload.box.bounds.y += offset.y;

                        if(command.payload.box.style.fill.type != FillType::Solid)
                        {
                            command.payload.box.style.fill.from = command.payload.box.style.fill.from + offset;

                            if(command.payload.box.style.fill.type == FillType::Linear)
                            {
                                command.payload.box.style.fill.to = command.payload.box.style.fill.to + offset;
                            }
                        }
                    }

                    break;
                case DrawCommandType::Gradient:
                    if(transformDepth == 0)
                    {
                        command.payload.gradient.bounds.x += offset.x;
                        command.payload.gradient.bounds.y += offset.y;
                    }

                    break;
                case DrawCommandType::Line:
                    if(transformDepth == 0)
                    {
                        command.payload.line.first = command.payload.line.first + offset;
                        command.payload.line.second = command.payload.line.second + offset;
                    }

                    break;
                case DrawCommandType::Polyline:
                    if(transformDepth == 0)
                    {
                        for(Vec2 & point : drawCommandStorage.mutablePointSpan(command.payload.polyline.points))
                        {
                            point = point + offset;
                        }
                    }

                    break;
                case DrawCommandType::Path:
                    if(transformDepth == 0)
                    {
                        command.payload.path.center = command.payload.path.center + offset;
                        for(Vec2 & point : drawCommandStorage.mutablePointSpan(command.payload.path.points))
                        {
                            point = point + offset;
                        }
                        for(ColoredPoint & point : drawCommandStorage.mutableColoredPointSpan(command.payload.path.coloredPoints))
                        {
                            point.position = point.position + offset;
                        }
                    }

                    break;
                case DrawCommandType::CustomGeometry:
                    if(transformDepth == 0)
                    {
                        for(Vertex & vertex : drawCommandStorage.mutableVertexSpan(command.payload.custom.vertices))
                        {
                            vertex.position = vertex.position + offset;
                        }
                    }

                    break;
                case DrawCommandType::TextGeometry:
                    if(transformDepth == 0)
                    {
                        command.payload.textGeometry.translation = command.payload.textGeometry.translation + offset;
                    }

                    break;
                case DrawCommandType::NineSlice:
                    if(transformDepth == 0)
                    {
                        command.payload.nineSlice.bounds.x += offset.x;
                        command.payload.nineSlice.bounds.y += offset.y;
                    }

                    break;
                case DrawCommandType::Grid:
                    if(transformDepth == 0)
                    {
                        command.payload.grid.bounds.x += offset.x;
                        command.payload.grid.bounds.y += offset.y;
                        command.payload.grid.style.origin = command.payload.grid.style.origin + offset;
                    }

                    break;
                case DrawCommandType::PopClip:
                    break;
                }
                drawList.commands().emplace_back(std::move(command));
            };

            for(DrawCommand & command : commands)
            {
                emitCommand(command);
            }

            if(canvasPass == CanvasLayer::Local && hasCanvasCommands(node, CanvasLayer::Overlay) == true)
            {
                transformDepth = 0;
                DrawCommandVector & overlayCommands = canvasCommands(node, CanvasLayer::Overlay);

                for(DrawCommand & command : overlayCommands)
                {
                    emitCommand(command);
                }
            }

            drawList.popClip(baseKey);
            break;
        }
        case Detail::NodeKind::Scroll:
        case Detail::NodeKind::Table:
        {
            const Persistent & scrollState = state(node);

            if(node.kind == Detail::NodeKind::Scroll && (node.scrollOptions().background == true || node.scrollOptions().framed == true))
            {
                Detail::drawFrame(drawList, node.bounds, node.style->metrics.childCornerRadius, node.scrollOptions().framed ? node.style->metrics.childBorderSize : 0.f, node.scrollOptions().frameStyle ? node.style->colors.frame : node.style->colors.background, node.scrollOptions().frameStyle ? node.style->colors.borderStrong : node.style->colors.border, baseKey);
            }

            if(node.kind == Detail::NodeKind::Scroll && (node.scrollOptions().resizeX == true || node.scrollOptions().resizeY == true))
            {
                Color grip = node.scrollResizeHovered || state(node).scrollData().resizingArea ? node.style->colors.resizeGripHovered : node.style->colors.resizeGrip;

                if(node.scrollOptions().resizeX == true && node.scrollOptions().resizeY == true)
                {
                    for(float inset = 4.f; inset <= 10.f; inset += 3.f)
                    {
                        drawList.line({node.bounds.right() - inset, node.bounds.bottom() - 2.f}, {node.bounds.right() - 2.f, node.bounds.bottom() - inset}, 1.f, grip, baseKey);
                    }
                }
                else if(node.scrollOptions().resizeX == true)
                {
                    drawList.rect({node.bounds.right() - 2.f, node.bounds.y + 4.f, 2.f, std::max(0.f, node.bounds.height - 8.f)}, grip, baseKey);
                }
                else
                {
                    drawList.rect({node.bounds.x + 4.f, node.bounds.bottom() - 2.f, std::max(0.f, node.bounds.width - 8.f), 2.f}, grip, baseKey);
                }
            }

            if(node.kind == Detail::NodeKind::Table)
            {
                const TableState & tableState = this->tableState(node.id);
                Rect tableBounds = node.childrenClip.empty() == true ? node.bounds : node.childrenClip;

                if(node.tableOptions().highlightHoveredColumn == true)
                {
                    const PointerState * pointer = input.primaryPointer();

                    if(pointer != nullptr)
                    {
                        for(const TableColumnState & column : tableState.columns)
                        {
                            if(column.options.enabled == true && column.options.visible == true && column.bodyBounds.contains(pointer->position) == true)
                            {
                                drawList.rect(column.bodyBounds, Detail::colorWithAlpha(node.style->colors.headerHovered, 0.22f), baseKey);
                                break;
                            }
                        }
                    }
                }

                for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = nodes[child].nextSibling)
                {
                    const Node & cell = nodes[child];

                    if(cell.bounds.empty() == true)
                    {
                        continue;
                    }

                    if(cell.kind == Detail::NodeKind::TableRow)
                    {
                        continue;
                    }

                    if(tableState.headersSubmitted == true && cell.tableItem().row == 0)
                    {
                        drawList.rect(cell.bounds, node.style->colors.tableHeader, baseKey);
                    }
                    else if(cell.tableItem().rowBackgroundsEnabled[0])
                    {
                        drawList.rect(cell.bounds, cell.tableItem().rowBackgrounds[0], baseKey);
                    }
                    else if(node.tableOptions().rowBackground == true && (cell.tableItem().row & 1U) != 0)
                    {
                        drawList.rect(cell.bounds, node.style->colors.tableRowAlternate, baseKey);
                    }
                    else if(node.tableOptions().rowBackground == true)
                    {
                        drawList.rect(cell.bounds, node.style->colors.tableRow, baseKey);
                    }

                    if(cell.tableItem().rowBackgroundsEnabled[1])
                    {
                        drawList.rect(cell.bounds, cell.tableItem().rowBackgrounds[1], baseKey);
                    }

                    if(cell.tableItem().cellBackgroundEnabled == true)
                    {
                        drawList.rect(cell.bounds, cell.tableItem().cellBackground, baseKey);
                    }

                    bool drawBodyBorder = node.tableOptions().bordersInBody;

                    if(node.tableOptions().bordersInBodyUntilResize == true)
                    {
                        drawBodyBorder = std::any_of(tableState.columns.begin(), tableState.columns.end(),
                                                     [](const TableColumnState & column)
                                                     {
                                                         return column.resizing;
                                                     });
                    }

                    if(node.tableOptions().bordersInnerHorizontal == true && (cell.tableItem().row == 0 || drawBodyBorder == true))
                    {
                        drawList.line({cell.bounds.x, cell.bounds.bottom()}, {cell.bounds.right(), cell.bounds.bottom()}, node.style->metrics.borderWidth, node.style->colors.tableBorderLight, baseKey);
                    }
                }

                if(node.tableOptions().bordersInnerVertical == true)
                {
                    for(const TableColumnState & column : tableState.columns)
                    {
                        if(column.options.enabled == false)
                        {
                            continue;
                        }

                        if(column.options.visible == false)
                        {
                            continue;
                        }

                        if(column.lastBounds.empty() == true)
                        {
                            continue;
                        }

                        drawList.line({column.lastBounds.right(), tableBounds.y}, {column.lastBounds.right(), tableBounds.bottom()}, node.style->metrics.borderWidth, node.style->colors.tableBorderLight, baseKey);
                    }
                }

                if(node.tableOptions().bordersOuterHorizontal == true)
                {
                    drawList.line({tableBounds.x, tableBounds.y}, {tableBounds.right(), tableBounds.y}, node.style->metrics.borderWidth, node.style->colors.tableBorderStrong, baseKey);
                    drawList.line({tableBounds.x, tableBounds.bottom()}, {tableBounds.right(), tableBounds.bottom()}, node.style->metrics.borderWidth, node.style->colors.tableBorderStrong, baseKey);
                }

                if(node.tableOptions().bordersOuterVertical == true)
                {
                    drawList.line({tableBounds.x, tableBounds.y}, {tableBounds.x, tableBounds.bottom()}, node.style->metrics.borderWidth, node.style->colors.tableBorderStrong, baseKey);
                    drawList.line({tableBounds.right(), tableBounds.y}, {tableBounds.right(), tableBounds.bottom()}, node.style->metrics.borderWidth, node.style->colors.tableBorderStrong, baseKey);
                }
            }
            Detail::drawScrollbar(drawList, scrollState.scrollData().verticalTrack, scrollState.scrollData().verticalThumb, true, *node.style, hoverVisual, scrollState.scrollData().draggingAxis == 2 ? activeVisual : 0.f, baseKey);
            Detail::drawScrollbar(drawList, scrollState.scrollData().horizontalTrack, scrollState.scrollData().horizontalThumb, false, *node.style, hoverVisual, scrollState.scrollData().draggingAxis == 1 ? activeVisual : 0.f, baseKey);
            break;
        }

        default:
            break;
        }

        if(node.kind == Detail::NodeKind::Window && node.windowData().scrollable == true)
        {
            const Persistent & scrollState = state(node);
            Detail::drawScrollbar(drawList, scrollState.scrollData().verticalTrack, scrollState.scrollData().verticalThumb, true, *node.style, hoverVisual, scrollState.scrollData().draggingAxis == 2 ? activeVisual : 0.f, baseKey);
            Detail::drawScrollbar(drawList, scrollState.scrollData().horizontalTrack, scrollState.scrollData().horizontalThumb, false, *node.style, hoverVisual, scrollState.scrollData().draggingAxis == 1 ? activeVisual : 0.f, baseKey);
        }

        DragPhase dragPhase = dragDrop.phase();

        if(dragDrop.targetHighlightVisible() == true && dragDrop.target() == node.id && (dragPhase == DragPhase::Enter || dragPhase == DragPhase::Over))
        {
            Rect target = node.bounds.inset(1.f);
            constexpr float thickness = 2.f;
            drawList.roundedRect(target, node.style->metrics.frameCornerRadius, node.style->colors.dragDropTargetBackground, baseKey);
            drawList.line({target.x, target.y}, {target.right(), target.y}, thickness, node.style->colors.dragDropTarget, baseKey);
            drawList.line({target.right(), target.y}, {target.right(), target.bottom()}, thickness, node.style->colors.dragDropTarget, baseKey);
            drawList.line({target.right(), target.bottom()}, {target.x, target.bottom()}, thickness, node.style->colors.dragDropTarget, baseKey);
            drawList.line({target.x, target.bottom()}, {target.x, target.y}, thickness, node.style->colors.dragDropTarget, baseKey);
        }

        if(configuration.debugHighlightIdConflicts == true && node.idConflict == true)
        {
            Rect conflictBounds = node.bounds.inset(1.f);
            BoxStyle conflictStyle;
            conflictStyle.borderWidth = 2.f;
            conflictStyle.borderColor = node.style->colors.error;
            conflictStyle.radii.topLeft = node.style->metrics.frameCornerRadius;
            conflictStyle.radii.topRight = node.style->metrics.frameCornerRadius;
            conflictStyle.radii.bottomRight = node.style->metrics.frameCornerRadius;
            conflictStyle.radii.bottomLeft = node.style->metrics.frameCornerRadius;
            drawList.box(conflictBounds, conflictStyle, baseKey);
        }

        emitMetadata();

        if(node.kind != Detail::NodeKind::Canvas || canvasPass == CanvasLayer::Local)
        {
            emitChildren();
        }
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
