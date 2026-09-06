#include "Context.hpp"
#include "Interaction.hpp"
#include "Utility.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Vec2 transformPoint(const Transform2D & transform, const Vec2 & point) noexcept
        {
            Vec2 result;
            result.x = transform.translation.x + transform.axisX.x * point.x + transform.axisY.x * point.y;
            result.y = transform.translation.y + transform.axisX.y * point.x + transform.axisY.y * point.y;

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool inverseVector(const Transform2D & transform, const Vec2 & vector, Vec2 * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            float determinant = transform.axisX.x * transform.axisY.y - transform.axisX.y * transform.axisY.x;

            if(std::abs(determinant) <= 0.000001f)
            {
                return false;
            }

            float inverse = 1.f / determinant;
            _out->x = (vector.x * transform.axisY.y - vector.y * transform.axisY.x) * inverse;
            _out->y = (vector.y * transform.axisX.x - vector.x * transform.axisX.y) * inverse;

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool inversePoint(const Transform2D & transform, const Vec2 & point, Vec2 * const _out) noexcept
        {
            Vec2 relative = point - transform.translation;
            bool result = Detail::inverseVector(transform, relative, _out);

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Rect transformBounds(const Transform2D & transform, const Rect & bounds) noexcept
        {
            Vec2 first = Detail::transformPoint(transform, {bounds.x, bounds.y});
            Vec2 second = Detail::transformPoint(transform, {bounds.right(), bounds.y});
            Vec2 third = Detail::transformPoint(transform, {bounds.right(), bounds.bottom()});
            Vec2 fourth = Detail::transformPoint(transform, {bounds.x, bounds.bottom()});
            float minimumX = std::min(std::min(first.x, second.x), std::min(third.x, fourth.x));
            float minimumY = std::min(std::min(first.y, second.y), std::min(third.y, fourth.y));
            float maximumX = std::max(std::max(first.x, second.x), std::max(third.x, fourth.x));
            float maximumY = std::max(std::max(first.y, second.y), std::max(third.y, fourth.y));

            return {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Transform2D canvasInteractionTransform(const Context * ui, Id canvas) noexcept
        {
            auto iterator = ui->canvasInteractionStates.find(canvas);

            if(iterator == ui->canvasInteractionStates.end())
            {
                return {};
            }

            return iterator->second.transform;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Response absoluteInteraction(Context * ui, const Key & key, const Rect & bounds, const ItemBehaviorOptions & behavior, bool enabled, const SourceLocation & location)
        {
            LayoutOptions absoluteLayout;
            absoluteLayout.width = SizeRule::Fill;
            absoluteLayout.height = SizeRule::Fill;
            auto absoluteScope = Mosaic::absolute(ui, absoluteLayout, location);
            LayoutOptions layout;
            layout.absoluteRect = bounds;
            size_t node = ui->addNode(NodeKind::Button, key, {}, layout, location, SemanticRole::Button, true);
            Context::Node & item = ui->nodes[node];
            item.fillBackground = false;
            item.fillHoverBackground = false;
            item.disabled = item.disabled || enabled == false;
            Response response = ui->interact(node, behavior);
            item.response = response;

            return response;
        }
        //////////////////////////////////////////////////////////////////////////
        void addVirtualSpacer(Context * ui, float extent, Orientation orientation, const SourceLocation & location)
        {
            if(extent <= 0.f)
            {
                return;
            }

            LayoutOptions layout;

            if(orientation == Orientation::Horizontal)
            {
                layout.width = Dimension::fixed(extent);
            }
            else
            {
                layout.height = Dimension::fixed(extent);
            }

            (void)ui->addNode(NodeKind::Spacer, {}, {}, layout, location, SemanticRole::None, false, true);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float itemOffset(size_t item, const VirtualListOptions & options) noexcept
        {
            if(options.offset != nullptr)
            {
                return std::max(0.f, options.offset(item, options.userData));
            }

            float extent = options.fixedExtent > 0.f ? options.fixedExtent : std::max(0.f, options.estimatedExtent);
            float result = static_cast<float>(item) * (extent + std::max(0.f, options.spacing));

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float itemExtent(size_t item, const VirtualListOptions & options) noexcept
        {
            if(options.fixedExtent > 0.f)
            {
                return options.fixedExtent;
            }

            if(options.extent != nullptr)
            {
                return std::max(0.f, options.extent(item, options.userData));
            }

            return std::max(0.f, options.estimatedExtent);
        }
        //////////////////////////////////////////////////////////////////////////
        struct VirtualGridIdentityContext
        {
            VirtualItemIdentity identity = nullptr;
            void * userData = nullptr;
            size_t columns = 1;
        };
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Id virtualGridRowIdentity(size_t row, void * userData) noexcept
        {
            VirtualGridIdentityContext * context = static_cast<VirtualGridIdentityContext *>(userData);

            if(context == nullptr || context->identity == nullptr)
            {
                return InvalidId;
            }

            size_t index = row * context->columns;

            return context->identity(index, context->userData);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Id resourceIdentity(size_t index, void * userData) noexcept
        {
            ResourceTileSpan * resources = static_cast<ResourceTileSpan *>(userData);

            if(resources == nullptr || index >= resources->size())
            {
                return InvalidId;
            }

            return (*resources)[index].resource;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Id treeIdentity(size_t index, void * userData) noexcept
        {
            TreeRowSpan * rows = static_cast<TreeRowSpan *>(userData);

            if(rows == nullptr || index >= rows->size())
            {
                return InvalidId;
            }

            return (*rows)[index].item;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Response editorCanvas(Context * ui, const Key & key, StringView label, const LayoutOptions & layout, PointerButton pointerButton, Canvas * const _out, const SourceLocation & location)
        {
            size_t node = ui->addNode(NodeKind::Canvas, key, label, layout, location, SemanticRole::Group, true);
            ItemBehaviorOptions behavior;
            behavior.keyboardActivation = true;
            behavior.pointerButton = pointerButton;
            Response response = ui->interact(node, behavior);
            ui->nodes[node].response = response;
            uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);
            *_out = Canvas(ui, token, ui->nodes[node].id, true);

            return response;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Vec2 graphPinPosition(const GraphNode & node, const NodePin & pin, size_t pinIndex) noexcept
        {
            float spacing = 18.f;
            float y = node.bounds.y + 28.f + static_cast<float>(pinIndex) * spacing;
            float x = pin.direction == NodePinDirection::Input ? node.bounds.x : node.bounds.right();

            return {x, y};
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool findGraphPin(GraphNodeSpan nodes, Id pinId, Vec2 * const _out) noexcept
        {
            for(const GraphNode & node : nodes)
            {
                for(size_t pinIndex = 0; pinIndex != node.pins.size(); ++pinIndex)
                {
                    const NodePin & pin = node.pins[pinIndex];

                    if(pin.id != pinId)
                    {
                        continue;
                    }

                    *_out = Detail::graphPinPosition(node, pin, pinIndex);

                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        struct GraphPinLocation
        {
            const GraphNode * node = nullptr;
            const NodePin * pin = nullptr;
            Vec2 position;
        };
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool findGraphPin(GraphNodeSpan nodes, Id pinId, GraphPinLocation * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            for(const GraphNode & node : nodes)
            {
                for(size_t pinIndex = 0; pinIndex != node.pins.size(); ++pinIndex)
                {
                    const NodePin & pin = node.pins[pinIndex];

                    if(pin.id != pinId)
                    {
                        continue;
                    }

                    _out->node = &node;
                    _out->pin = &pin;
                    _out->position = Detail::graphPinPosition(node, pin, pinIndex);

                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool graphPinsCompatible(const NodePin & first, const NodePin & second, const NodeGraphOptions & options) noexcept
        {
            if(first.id == second.id)
            {
                return false;
            }

            if(first.direction == second.direction)
            {
                return false;
            }

            const NodePin & output = first.direction == NodePinDirection::Output ? first : second;
            const NodePin & input = first.direction == NodePinDirection::Input ? first : second;

            if(options.connectionValidator != nullptr)
            {
                bool returnedValue = options.connectionValidator(output, input, options.connectionUserData);

                return returnedValue;
            }

            auto returnedValue = output.type == 0 || input.type == 0 || output.type == input.type;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ResourceTileResponse resourceListRow(Context * ui, const ResourceTile & resource, SelectionModel * selection, IdSpan orderedItems, const SelectionOptions & selectionOptions, float height, const SourceLocation & location)
        {
            ResourceTileResponse result;
            auto resourceScope = Mosaic::scope(ui, resource.key, location);
            LayoutOptions overlayLayout;
            overlayLayout.width = SizeRule::Fill;
            overlayLayout.height = Dimension::fixed(std::max(1.f, height));
            auto overlayScope = Mosaic::overlay(ui, Key("Resource list row overlay"), overlayLayout, location);
            SelectableOptions selectableOptions = selectionOptions.item;
            selectableOptions.width = SizeRule::Fill;
            selectableOptions.height = SizeRule::Fill;
            selectableOptions.allowDoubleClick = true;
            bool selected = selection != nullptr ? selection->selected(resource.resource) : resource.selected;
            Response row = Mosaic::selectable(ui, Key("Resource list row selection"), {}, selected, selectableOptions, location);

            if(selection != nullptr)
            {
                row = Mosaic::selectionItem(ui, row, selection, resource.resource, orderedItems, selectionOptions);
            }

            result.response = row;
            result.activated = row.doubleClicked();
            LayoutOptions contentsLayout;
            contentsLayout.width = SizeRule::Fill;
            contentsLayout.height = SizeRule::Fill;
            contentsLayout.gap = ui->currentStyle->metrics.innerSpacing.x;
            auto contentsScope = Mosaic::row(ui, Key("Resource list row contents"), contentsLayout, location);

            if(resource.thumbnail.texture != 0 || resource.thumbnail.semanticFallback.empty() == false)
            {
                Icon rowIcon = resource.thumbnail;
                float iconExtent = std::max(1.f, height - ui->currentStyle->metrics.framePadding.top - ui->currentStyle->metrics.framePadding.bottom);
                rowIcon.logicalSize = {iconExtent, iconExtent};
                (void)Mosaic::icon(ui, rowIcon, location);
            }

            LayoutOptions labelLayout;
            labelLayout.width = SizeRule::Fill;
            TextOptions labelOptions;
            labelOptions.layout = labelLayout;
            (void)Mosaic::text(ui, resource.label, labelOptions, location);

            if(resource.loading == true)
            {
                (void)Mosaic::typeBadge(ui, "Loading", ui->currentStyle->colors.textDisabled, location);
            }
            else if(resource.error == true)
            {
                (void)Mosaic::typeBadge(ui, "Error", ui->currentStyle->colors.error, location);
            }
            else if(resource.type.empty() == false)
            {
                (void)Mosaic::typeBadge(ui, resource.type, ui->currentStyle->colors.accent, location);
            }

            if(resource.dragType != 0 && resource.dragPayload.empty() == false)
            {
                result.beginDrag = Mosaic::beginDragDropSource(ui, result.response, resource.dragType, resource.dragPayload);
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Response icon(Context * ui, const Icon & value, const SourceLocation & location)
    {
        if(value.texture == 0)
        {
            Response response = Mosaic::text(ui, value.semanticFallback, location);

            return response;
        }

        ImageOptions options;
        options.uv = value.uv;
        options.tint = value.tint;
        Response response = Mosaic::image(ui, value.texture, value.logicalSize, options, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response highlightedText(Context * ui, StringView value, HighlightedTextRangeSpan ranges, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.gap = 0.f;
        layout.crossAxisAlignment = CrossAxisAlignment::Baseline;
        auto textRow = Mosaic::row(ui, layout, location);
        Response response;
        response.id = textRow.id();
        size_t cursor = 0;

        for(const HighlightedTextRange & range : ranges)
        {
            size_t begin = std::min(range.begin, value.size());
            size_t end = std::min(std::max(range.end, begin), value.size());

            if(begin > cursor)
            {
                (void)Mosaic::text(ui, value.substr(cursor, begin - cursor), location);
            }

            if(end > begin)
            {
                Theme highlight = *ui->currentStyle;
                highlight.colors.text = range.color;
                auto colorScope = Mosaic::styleScope(ui, highlight, location);
                (void)Mosaic::text(ui, value.substr(begin, end - begin), location);
            }

            cursor = std::max(cursor, end);
        }

        if(cursor < value.size())
        {
            (void)Mosaic::text(ui, value.substr(cursor), location);
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeRowResponse treeRow(Context * ui, const TreeRow & row, SelectionModel * selection, IdSpan orderedItems, const SelectionOptions & selectionOptions, const SourceLocation & location)
    {
        TreeRowResponse result;
        LayoutOptions overlayLayout;
        overlayLayout.width = SizeRule::Fill;
        overlayLayout.height = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
        auto rowScope = Mosaic::scope(ui, row.key, location);
        auto background = Mosaic::overlay(ui, Key("Tree row overlay"), overlayLayout, location);
        SelectableOptions selectableOptions;
        selectableOptions.width = SizeRule::Fill;
        selectableOptions.height = SizeRule::Fill;
        selectableOptions.allowDoubleClick = true;
        bool selected = selection != nullptr ? selection->selected(row.item) : row.selected;
        Response rowResponse = Mosaic::selectable(ui, Key("Tree row selection"), {}, selected, selectableOptions, location);
        Context::Node * selectableNode = ui->findFrameNode(rowResponse.item);

        if(selectableNode != nullptr)
        {
            selectableNode->semanticRole = SemanticRole::TreeItem;
            ui->nodeSemanticName(*selectableNode).assign(row.label);
        }

        if(selection != nullptr)
        {
            rowResponse = Mosaic::selectionItem(ui, rowResponse, selection, row.item, orderedItems, selectionOptions);
        }

        result.response = rowResponse;
        LayoutOptions contentsLayout;
        contentsLayout.width = SizeRule::Fill;
        contentsLayout.height = SizeRule::Fill;
        contentsLayout.gap = ui->currentStyle->metrics.innerSpacing.x;
        auto contents = Mosaic::row(ui, Key("Tree row contents"), contentsLayout, location);
        float indent = static_cast<float>(row.depth) * ui->currentStyle->metrics.indent;
        (void)Mosaic::spacer(ui, indent, location);

        if(row.leaf == false)
        {
            ButtonOptions arrowOptions;
            arrowOptions.width = Dimension::fixed(ui->currentStyle->metrics.lineHeight);
            arrowOptions.height = Dimension::fixed(ui->currentStyle->metrics.lineHeight);
            arrowOptions.fillBackground = false;
            Response arrow = Mosaic::arrowButton(ui, Key("Tree row expand"), row.expanded ? Direction::Down : Direction::Right, arrowOptions, location);
            result.toggleExpanded = arrow.clicked();
        }
        else
        {
            (void)Mosaic::spacer(ui, ui->currentStyle->metrics.lineHeight, location);
        }

        if(row.leadingIcon.texture != 0 || row.leadingIcon.semanticFallback.empty() == false)
        {
            (void)Mosaic::icon(ui, row.leadingIcon, location);
        }

        if(row.renameActive == true && row.renameValue != nullptr)
        {
            TextInputOptions inputOptions;
            inputOptions.selectAllOnFocus = true;
            Response rename = Mosaic::inputText(ui, "Rename", row.renameValue, inputOptions, location);
            result.renameChanged = rename.changed();
            result.renameCommitted = rename.committed();
            result.renameCancelled = rename.canceled();
        }
        else
        {
            LayoutOptions labelLayout;
            labelLayout.width = SizeRule::Fill;
            TextOptions labelOptions;
            labelOptions.layout = labelLayout;
            (void)Mosaic::text(ui, row.label, labelOptions, location);
            result.beginRename = rowResponse.doubleClicked();
        }

        for(size_t badgeIndex = 0; badgeIndex != row.badges.size(); ++badgeIndex)
        {
            auto badgeScope = Mosaic::scope(ui, Key(badgeIndex), location);
            (void)Mosaic::typeBadge(ui, row.badges[badgeIndex].text, row.badges[badgeIndex].color, location);
        }

        for(size_t actionIndex = 0; actionIndex != row.trailingActions.size(); ++actionIndex)
        {
            const TreeTrailingAction & action = row.trailingActions[actionIndex];
            auto actionScope = Mosaic::scope(ui, Key(actionIndex), location);
            auto enabledScope = Mosaic::disabledScope(ui, action.enabled == false, location);
            Response actionResponse;

            if(action.icon.texture != 0)
            {
                actionResponse = Mosaic::imageButton(ui, action.key, action.icon.texture, action.icon.logicalSize, location);
            }
            else
            {
                actionResponse = Mosaic::smallButton(ui, action.key, action.icon.semanticFallback, location);
            }

            if(actionResponse.clicked() == true)
            {
                result.activatedAction = actionResponse.id;
            }

            if(action.tooltip.empty() == false)
            {
                Vec2 tooltipSize = {420.f, 0.f};
                Mosaic::itemTooltip(ui, actionResponse, action.tooltip, tooltipSize, location);
            }
        }

        if(row.dragEnabled == true && row.dragType != 0 && row.dragPayload.empty() == false)
        {
            result.beginDrag = Mosaic::beginDragDropSource(ui, rowResponse, row.dragType, row.dragPayload);
        }

        ItemRef dragSource = ui->dragDrop.sourceItem();

        if(dragSource.valid() == true && ui->dragDrop.phase() == DragPhase::Over && rowResponse.hovered() == true)
        {
            Id scrollArea = row.scrollArea;
            const Context::InteractionSnapshotItem * snapshot = ui->findInteractionSnapshot(rowResponse.item);

            if(scrollArea == InvalidId && snapshot != nullptr && snapshot->scrollTransformCount != 0)
            {
                size_t transformIndex = snapshot->scrollTransformBegin;
                scrollArea = ui->interactionScrollTransforms[transformIndex].id;
            }

            Rect scrollBounds;
            Vec2 scrollValue;
            const PointerState * dragPointer = ui->input.primaryPointer();

            if(scrollArea != InvalidId && dragPointer != nullptr && Mosaic::debugBounds(ui, scrollArea, &scrollBounds) == true && Mosaic::scrollOffset(ui, scrollArea, &scrollValue) == true)
            {
                float margin = std::max(1.f, row.autoScrollMargin);
                float normalized = 0.f;

                if(dragPointer->position.y < scrollBounds.y + margin)
                {
                    normalized = -std::clamp((scrollBounds.y + margin - dragPointer->position.y) / margin, 0.f, 1.f);
                }

                if(dragPointer->position.y > scrollBounds.bottom() - margin)
                {
                    normalized = std::clamp((dragPointer->position.y - (scrollBounds.bottom() - margin)) / margin, 0.f, 1.f);
                }

                if(normalized != 0.f)
                {
                    result.autoScrollDelta = normalized * std::max(0.f, row.autoScrollSpeed) * ui->input.deltaTime;
                    scrollValue.y += result.autoScrollDelta;
                    Mosaic::scrollTo(ui, scrollArea, scrollValue);
                }
            }
        }

        if(row.dropEnabled == true && row.acceptedDropType != 0)
        {
            DragDropAcceptResult accepted = Mosaic::acceptDragDropPayload(ui, rowResponse, row.acceptedDropType);

            if(accepted.accepted() == true)
            {
                Rect bounds;
                Vec2 pointer;

                if(Mosaic::debugBounds(ui, rowResponse.item, &bounds) == true && Mosaic::pointerPosition(ui, &pointer) == true && bounds.height > 0.f)
                {
                    float ratio = std::clamp((pointer.y - bounds.y) / bounds.height, 0.f, 1.f);

                    if(row.leaf == false && ratio >= 0.25f && ratio <= 0.75f)
                    {
                        result.dropZone = TreeDropZone::Inside;
                    }
                    else
                    {
                        result.dropZone = ratio < 0.5f ? TreeDropZone::Before : TreeDropZone::After;
                    }
                }

                result.dropped = accepted.delivery;
            }
        }

        if(row.leaf == false && row.expanded == false && rowResponse.hovered() == true && ui->dragDrop.phase() == DragPhase::Over && ui->dragDrop.sourceItem().valid() == true)
        {
            result.autoExpand = Mosaic::itemStationaryHoverDuration(ui, rowResponse.id) >= static_cast<double>(std::max(0.f, row.autoExpandDelay));
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool treeView(Context * ui, const Key & key, StringView label, TreeRowSpan rows, SelectionModel * selection, TreeViewResponse * const _out, const TreeViewOptions & options, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return false;
        }

        TreeViewResponse localResponse;
        TreeViewResponse * response = _out == nullptr ? &localResponse : _out;
        response->id = InvalidId;
        response->visibleRows = {};
        response->rows.clear();
        response->navigationItem = std::numeric_limits<size_t>::max();
        response->navigationRequested = false;
        auto treeScope = Mosaic::scope(ui, key, location);
        ScrollOptions scrollOptions = options.scroll;
        scrollOptions.axes = ScrollAxes::Vertical;
        scrollOptions.contentOrientation = Orientation::Vertical;
        auto scrollScope = Mosaic::scrollArea(ui, Key("Tree view scroll"), label, scrollOptions, options.layout, location);
        response->id = scrollScope.id();
        auto pendingFocus = ui->treeViewPendingFocus.find(response->id);

        if(pendingFocus != ui->treeViewPendingFocus.end() && pendingFocus->second >= rows.size())
        {
            ui->treeViewPendingFocus.erase(pendingFocus);
        }

        VirtualListOptions virtualOptions;
        virtualOptions.fixedExtent = ui->currentStyle->metrics.controlHeight;
        virtualOptions.spacing = 0.f;
        virtualOptions.overscan = options.overscan;
        virtualOptions.scrollArea = scrollScope.id();
        TreeRowSpan rowSpan = rows;
        virtualOptions.identity = Detail::treeIdentity;
        virtualOptions.userData = &rowSpan;
        virtualOptions.anchor = options.anchor;
        virtualOptions.scrollToItem = options.scrollToItem;
        virtualOptions.scrollToAlignment = options.scrollToAlignment;
        VirtualListState virtualState;

        if(Mosaic::beginVirtualList(ui, rows.size(), virtualOptions, &virtualState, location) == false)
        {
            return false;
        }

        response->visibleRows = virtualState.range;
        response->rows.reserve(virtualState.range.end - virtualState.range.begin);
        SelectionOptions selectionOptions = options.selection;
        selectionOptions.item.selectOnNavigation = true;
        IdVector semanticParents;

        for(size_t index = virtualState.range.begin; index != virtualState.range.end; ++index)
        {
            TreeRow row = rows[index];
            row.scrollArea = scrollScope.id();
            TreeRowResponse rowResponse = Mosaic::treeRow(ui, row, selection, options.orderedItems, selectionOptions, location);
            TreeViewRowResponse outputRow;
            outputRow.index = index;
            outputRow.row = rowResponse;
            response->rows.emplace_back(outputRow);

            if(semanticParents.size() <= row.depth)
            {
                semanticParents.resize(static_cast<size_t>(row.depth) + 1, InvalidId);
            }

            Id semanticParent = scrollScope.id();

            if(row.depth != 0)
            {
                size_t parentDepth = static_cast<size_t>(row.depth - 1);

                if(parentDepth < semanticParents.size() && semanticParents[parentDepth] != InvalidId)
                {
                    semanticParent = semanticParents[parentDepth];
                }
            }

            Context::Node * rowNode = ui->findFrameNode(rowResponse.response.item);

            if(rowNode != nullptr)
            {
                rowNode->parentId = semanticParent;
            }

            semanticParents[row.depth] = rowResponse.response.id;
            semanticParents.resize(static_cast<size_t>(row.depth) + 1);
            auto focusRequest = ui->treeViewPendingFocus.find(response->id);

            if(focusRequest != ui->treeViewPendingFocus.end() && focusRequest->second == index)
            {
                Mosaic::focus(ui, rowResponse.response.id);
                ui->treeViewPendingFocus.erase(focusRequest);
            }

            if(Mosaic::navigationFocus(ui) != rowResponse.response.id)
            {
                continue;
            }

            if(ui->input.keyPressed(KeyCode::F2) == true && row.renameActive == false && row.renameValue != nullptr)
            {
                response->rows.back().row.beginRename = true;
            }

            if(ui->input.keyPressed(KeyCode::Right) == true && row.leaf == false)
            {
                if(row.expanded == false)
                {
                    response->rows.back().row.toggleExpanded = true;
                }
                else if(index + 1 < rows.size() && rows[index + 1].depth > row.depth)
                {
                    response->navigationItem = index + 1;
                    response->navigationRequested = true;
                }
            }

            if(ui->input.keyPressed(KeyCode::Left) == true)
            {
                if(row.leaf == false && row.expanded == true)
                {
                    response->rows.back().row.toggleExpanded = true;
                }
                else if(row.depth != 0)
                {
                    size_t parentIndex = index;

                    while(parentIndex != 0)
                    {
                        --parentIndex;

                        if(rows[parentIndex].depth < row.depth)
                        {
                            response->navigationItem = parentIndex;
                            response->navigationRequested = true;

                            break;
                        }
                    }
                }
            }

            if(ui->input.keyPressed(KeyCode::Up) == true && index != 0)
            {
                response->navigationItem = index - 1;
                response->navigationRequested = true;
            }

            if(ui->input.keyPressed(KeyCode::Down) == true && index + 1 < rows.size())
            {
                response->navigationItem = index + 1;
                response->navigationRequested = true;
            }

            if(ui->input.keyPressed(KeyCode::Home) == true && rows.empty() == false)
            {
                response->navigationItem = 0;
                response->navigationRequested = true;
            }

            if(ui->input.keyPressed(KeyCode::End) == true && rows.empty() == false)
            {
                response->navigationItem = rows.size() - 1;
                response->navigationRequested = true;
            }

            size_t pageSize = std::max<size_t>(1, virtualState.range.end - virtualState.range.begin);

            if(ui->input.keyPressed(KeyCode::PageUp) == true)
            {
                response->navigationItem = index > pageSize ? index - pageSize : 0;
                response->navigationRequested = true;
            }

            if(ui->input.keyPressed(KeyCode::PageDown) == true && rows.empty() == false)
            {
                response->navigationItem = std::min(rows.size() - 1, index + pageSize);
                response->navigationRequested = true;
            }
        }

        Mosaic::endVirtualList(ui, virtualState, virtualOptions, location);

        if(response->navigationRequested == true)
        {
            ui->treeViewPendingFocus[response->id] = response->navigationItem;
            float target = static_cast<float>(response->navigationItem) * ui->currentStyle->metrics.controlHeight;
            Mosaic::scrollTo(ui, response->id, target);
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool beginVirtualList(Context * ui, size_t itemCount, const VirtualListOptions & options, VirtualListState * const _out, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        *_out = {};
        _out->itemCount = itemCount;

        if(itemCount == 0)
        {
            if(options.anchor != nullptr)
            {
                *options.anchor = {};
            }

            return true;
        }

        float scroll = 0.f;
        float viewportExtent = options.orientation == Orientation::Vertical ? ui->viewport.bounds.height : ui->viewport.bounds.width;
        Vec2 scrollValue;

        if(options.scrollArea != InvalidId && Mosaic::scrollOffset(ui, options.scrollArea, &scrollValue) == true)
        {
            scroll = options.orientation == Orientation::Vertical ? scrollValue.y : scrollValue.x;
            Rect scrollBounds;

            if(Mosaic::debugBounds(ui, options.scrollArea, &scrollBounds) == true)
            {
                viewportExtent = options.orientation == Orientation::Vertical ? scrollBounds.height : scrollBounds.width;
            }
        }

        if(options.scrollArea != InvalidId && options.scrollToItem < itemCount)
        {
            float itemBegin = Detail::itemOffset(options.scrollToItem, options);
            float itemSize = Detail::itemExtent(options.scrollToItem, options);
            float alignment = std::clamp(options.scrollToAlignment, 0.f, 1.f);
            float target = itemBegin - std::max(0.f, viewportExtent - itemSize) * alignment;

            if(options.orientation == Orientation::Vertical)
            {
                scrollValue.y = target;
            }
            else
            {
                scrollValue.x = target;
            }

            Mosaic::scrollTo(ui, options.scrollArea, scrollValue);
            scroll = target;
        }

        if(options.scrollArea != InvalidId && options.anchor != nullptr && options.anchor->valid == true && options.anchor->itemCount != itemCount)
        {
            size_t anchorItem = std::min(options.anchor->item, itemCount - 1);

            if(options.identity != nullptr && options.anchor->identity != InvalidId)
            {
                for(size_t item = 0; item != itemCount; ++item)
                {
                    if(options.identity(item, options.userData) == options.anchor->identity)
                    {
                        anchorItem = item;

                        break;
                    }
                }
            }

            float target = std::max(0.f, Detail::itemOffset(anchorItem, options) - options.anchor->offset);

            if(options.orientation == Orientation::Vertical)
            {
                scrollValue.y = target;
            }
            else
            {
                scrollValue.x = target;
            }

            Mosaic::scrollTo(ui, options.scrollArea, scrollValue);
            scroll = target;
        }

        if(options.fixedExtent > 0.f)
        {
            ListClipperOptions clipper;
            clipper.orientation = options.orientation;
            clipper.spacing = options.spacing;
            clipper.overscan = options.overscan;
            bool result = Mosaic::beginListClipper(ui, itemCount, options.fixedExtent, &_out->range, options.scrollArea, clipper, location);
            float stride = options.fixedExtent + std::max(0.f, options.spacing);
            _out->leadingExtent = static_cast<float>(_out->range.begin) * stride;
            _out->totalExtent = static_cast<float>(itemCount) * stride - std::max(0.f, options.spacing);
            _out->trailingExtent = std::max(0.f, _out->totalExtent - static_cast<float>(_out->range.end) * stride);

            if(options.anchor != nullptr)
            {
                options.anchor->item = _out->range.begin;
                options.anchor->itemCount = itemCount;
                options.anchor->identity = options.identity == nullptr || _out->range.begin >= itemCount ? InvalidId : options.identity(_out->range.begin, options.userData);
                options.anchor->offset = Detail::itemOffset(_out->range.begin, options) - scroll;
                options.anchor->valid = _out->range.begin < itemCount;
            }

            return result;
        }

        size_t low = 0;
        size_t high = itemCount;

        while(low < high)
        {
            size_t middle = low + (high - low) / 2;

            if(Detail::itemOffset(middle, options) + Detail::itemExtent(middle, options) < scroll)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }

        size_t begin = low > options.overscan ? low - options.overscan : 0;
        float visibleEnd = scroll + viewportExtent;
        size_t end = begin;

        while(end < itemCount && Detail::itemOffset(end, options) < visibleEnd)
        {
            ++end;
        }

        end = std::min(itemCount, end + options.overscan);
        float total = Detail::itemOffset(itemCount - 1, options) + Detail::itemExtent(itemCount - 1, options);
        float leading = Detail::itemOffset(begin, options);
        float trailing = end >= itemCount ? 0.f : std::max(0.f, total - Detail::itemOffset(end, options));
        _out->range = {begin, end};
        _out->leadingExtent = leading;
        _out->trailingExtent = trailing;
        _out->totalExtent = total;
        Detail::addVirtualSpacer(ui, leading, options.orientation, location);

        if(options.anchor != nullptr)
        {
            options.anchor->item = begin;
            options.anchor->itemCount = itemCount;
            options.anchor->identity = options.identity == nullptr || begin >= itemCount ? InvalidId : options.identity(begin, options.userData);
            options.anchor->offset = leading - scroll;
            options.anchor->valid = begin < itemCount;
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void endVirtualList(Context * ui, const VirtualListState & state, const VirtualListOptions & options, const SourceLocation & location)
    {
        if(options.fixedExtent > 0.f)
        {
            ListClipperOptions clipper;
            clipper.orientation = options.orientation;
            clipper.spacing = options.spacing;
            clipper.overscan = options.overscan;
            Mosaic::endListClipper(ui, state.range, state.itemCount, options.fixedExtent, clipper, location);

            return;
        }

        Detail::addVirtualSpacer(ui, state.trailingExtent, options.orientation, location);
    }
    //////////////////////////////////////////////////////////////////////////
    bool beginVirtualGrid(Context * ui, size_t itemCount, float availableWidth, const VirtualGridOptions & options, VirtualGridState * const _out, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        float stride = std::max(1.f, options.itemSize.x + std::max(0.f, options.spacing.x));
        size_t columns = std::max(options.minimumColumns, static_cast<size_t>(std::max(1.f, std::floor((availableWidth + std::max(0.f, options.spacing.x)) / stride))));
        size_t rowCount = columns == 0 ? 0 : (itemCount + columns - 1) / columns;
        VirtualListOptions listOptions;
        listOptions.fixedExtent = options.itemSize.y;
        listOptions.spacing = options.spacing.y;
        listOptions.overscan = options.overscanRows;
        listOptions.scrollArea = options.scrollArea;
        Detail::VirtualGridIdentityContext identityContext;
        identityContext.identity = options.identity;
        identityContext.userData = options.userData;
        identityContext.columns = columns;
        listOptions.identity = options.identity == nullptr ? nullptr : Detail::virtualGridRowIdentity;
        listOptions.userData = &identityContext;
        VirtualScrollAnchor rowAnchor;

        if(options.anchor != nullptr)
        {
            rowAnchor = *options.anchor;
            rowAnchor.item /= columns;
            rowAnchor.itemCount = rowCount;
            listOptions.anchor = &rowAnchor;
        }

        listOptions.scrollToItem = options.scrollToItem == std::numeric_limits<size_t>::max() ? options.scrollToItem : options.scrollToItem / columns;
        listOptions.scrollToAlignment = options.scrollToAlignment;
        VirtualListState listState;

        if(Mosaic::beginVirtualList(ui, rowCount, listOptions, &listState, location) == false)
        {
            return false;
        }

        if(options.anchor != nullptr)
        {
            options.anchor->item = std::min(itemCount, rowAnchor.item * columns);
            options.anchor->itemCount = itemCount;
            options.anchor->identity = rowAnchor.identity;
            options.anchor->offset = rowAnchor.offset;
            options.anchor->valid = rowAnchor.valid;
        }

        _out->columns = columns;
        _out->firstRow = listState.range.begin;
        _out->lastRow = listState.range.end;
        _out->range.begin = std::min(itemCount, listState.range.begin * columns);
        _out->range.end = std::min(itemCount, listState.range.end * columns);
        _out->leadingExtent = listState.leadingExtent;
        _out->trailingExtent = listState.trailingExtent;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void endVirtualGrid(Context * ui, const VirtualGridState & state, const VirtualGridOptions & options, const SourceLocation & location)
    {
        (void)options;
        Detail::addVirtualSpacer(ui, state.trailingExtent, Orientation::Vertical, location);
    }
    //////////////////////////////////////////////////////////////////////////
    ResourceTileResponse resourceTile(Context * ui, const ResourceTile & tile, SelectionModel * selection, IdSpan orderedItems, const SourceLocation & location)
    {
        ResourceTileResponse result;
        auto tileScope = Mosaic::scope(ui, tile.key, location);
        LayoutOptions layout;
        layout.width = Dimension::fixed(std::max(tile.thumbnail.logicalSize.x, 96.f));
        layout.gap = ui->currentStyle->metrics.innerSpacing.y;
        auto contents = Mosaic::column(ui, Key("Resource tile contents"), layout, location);

        if(tile.loading == true)
        {
            result.response = Mosaic::thumbnailPlaceholder(ui, Key("Resource loading thumbnail"), tile.thumbnail.logicalSize, "Loading", location);
        }
        else if(tile.error == true)
        {
            result.response = Mosaic::thumbnailPlaceholder(ui, Key("Resource error thumbnail"), tile.thumbnail.logicalSize, "Unavailable", location);
        }
        else if(tile.thumbnail.texture != 0)
        {
            ImageButtonOptions imageOptions;
            imageOptions.uv = tile.thumbnail.uv;
            imageOptions.tint = tile.thumbnail.tint;
            imageOptions.backgroundEnabled = true;
            result.response = Mosaic::imageButton(ui, Key("Resource thumbnail"), tile.thumbnail.texture, tile.thumbnail.logicalSize, imageOptions, location);
        }
        else
        {
            result.response = Mosaic::thumbnailPlaceholder(ui, Key("Resource thumbnail placeholder"), tile.thumbnail.logicalSize, tile.thumbnail.semanticFallback, location);
        }

        SelectableOptions selectableOptions;
        selectableOptions.width = SizeRule::Fill;
        selectableOptions.overrideTextAlignment = true;
        selectableOptions.textAlignment = {0.5f, 0.5f};
        bool selected = selection != nullptr ? selection->selected(tile.resource) : tile.selected;
        Response label = Mosaic::selectable(ui, Key("Resource label"), tile.label, selected, selectableOptions, location);

        if(selection != nullptr)
        {
            label = Mosaic::selectionItem(ui, label, selection, tile.resource, orderedItems);

            if(result.response.clicked() == true)
            {
                result.response = Mosaic::selectionItem(ui, result.response, selection, tile.resource, orderedItems);
            }
        }

        bool thumbnailActivated = result.response.doubleClicked();
        bool labelActivated = label.doubleClicked();

        if(label.clicked() == true)
        {
            result.response = label;
        }

        result.activated = thumbnailActivated || labelActivated;

        if(tile.type.empty() == false)
        {
            (void)Mosaic::typeBadge(ui, tile.type, ui->currentStyle->colors.accent, location);
        }

        if(tile.dragType != 0 && tile.dragPayload.empty() == false)
        {
            result.beginDrag = Mosaic::beginDragDropSource(ui, result.response, tile.dragType, tile.dragPayload);
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool resourceBrowser(Context * ui, const Key & key, StringView label, ResourceTileSpan resources, SelectionModel * selection, ResourceBrowserResponse * const _out, const ResourceBrowserOptions & options, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return false;
        }

        ResourceBrowserResponse localResponse;
        ResourceBrowserResponse * response = _out == nullptr ? &localResponse : _out;
        response->id = InvalidId;
        response->visibleItems = {};
        response->items.clear();
        auto browserScope = Mosaic::scope(ui, key, location);
        ScrollOptions scrollOptions = options.scroll;
        scrollOptions.axes = ScrollAxes::Vertical;
        scrollOptions.contentOrientation = Orientation::Vertical;
        auto scrollScope = Mosaic::scrollArea(ui, Key("Resource browser scroll"), label, scrollOptions, options.layout, location);
        response->id = scrollScope.id();

        if(resources.empty() == true)
        {
            (void)Mosaic::editorStateMessage(ui, "No resources", "No resources match the current catalog query.", {}, location);

            return true;
        }

        if(options.mode == ResourceBrowserMode::List)
        {
            VirtualListOptions virtualOptions;
            virtualOptions.fixedExtent = std::max(1.f, options.listRowHeight);
            virtualOptions.spacing = 0.f;
            virtualOptions.overscan = options.overscan;
            virtualOptions.scrollArea = scrollScope.id();
            ResourceTileSpan resourceSpan = resources;
            virtualOptions.identity = Detail::resourceIdentity;
            virtualOptions.userData = &resourceSpan;
            virtualOptions.anchor = options.anchor;
            virtualOptions.scrollToItem = options.scrollToItem;
            virtualOptions.scrollToAlignment = options.scrollToAlignment;
            VirtualListState virtualState;

            if(Mosaic::beginVirtualList(ui, resources.size(), virtualOptions, &virtualState, location) == false)
            {
                return false;
            }

            response->visibleItems = virtualState.range;
            response->items.reserve(virtualState.range.end - virtualState.range.begin);

            for(size_t index = virtualState.range.begin; index != virtualState.range.end; ++index)
            {
                ResourceBrowserItemResponse itemResponse;
                itemResponse.index = index;
                itemResponse.item = Detail::resourceListRow(ui, resources[index], selection, options.orderedItems, options.selection, virtualOptions.fixedExtent, location);
                response->items.emplace_back(itemResponse);
            }

            Mosaic::endVirtualList(ui, virtualState, virtualOptions, location);

            return true;
        }

        float availableWidth = options.availableWidth;

        if(availableWidth <= 0.f)
        {
            Rect browserBounds;

            if(Mosaic::debugBounds(ui, scrollScope.id(), &browserBounds) == true)
            {
                availableWidth = browserBounds.width;
            }
            else
            {
                availableWidth = ui->viewport.bounds.width;
            }
        }

        VirtualGridOptions gridOptions;
        gridOptions.itemSize = options.tileSize;
        gridOptions.spacing = options.tileSpacing;
        gridOptions.minimumColumns = options.minimumColumns;
        gridOptions.overscanRows = options.overscan;
        gridOptions.scrollArea = scrollScope.id();
        ResourceTileSpan resourceSpan = resources;
        gridOptions.identity = Detail::resourceIdentity;
        gridOptions.userData = &resourceSpan;
        gridOptions.anchor = options.anchor;
        gridOptions.scrollToItem = options.scrollToItem;
        gridOptions.scrollToAlignment = options.scrollToAlignment;
        VirtualGridState virtualState;

        if(Mosaic::beginVirtualGrid(ui, resources.size(), availableWidth, gridOptions, &virtualState, location) == false)
        {
            return false;
        }

        response->visibleItems = virtualState.range;
        response->items.reserve(virtualState.range.end - virtualState.range.begin);
        LayoutOptions gridLayout;
        gridLayout.width = SizeRule::Fill;
        gridLayout.gap = options.tileSpacing.x;
        auto visibleGrid = Mosaic::grid(ui, Key("Resource browser tiles"), static_cast<uint32_t>(virtualState.columns), gridLayout, location);

        for(size_t index = virtualState.range.begin; index != virtualState.range.end; ++index)
        {
            ResourceBrowserItemResponse itemResponse;
            itemResponse.index = index;
            itemResponse.item = Mosaic::resourceTile(ui, resources[index], selection, options.orderedItems, location);
            response->items.emplace_back(itemResponse);
        }

        Mosaic::endVirtualGrid(ui, virtualState, gridOptions, location);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    Response typeBadge(Context * ui, StringView type, const Color & color, const SourceLocation & location)
    {
        Theme badgeTheme = *ui->currentStyle;
        badgeTheme.colors.button = color;
        badgeTheme.colors.buttonHovered = color;
        badgeTheme.colors.buttonActive = color;
        badgeTheme.metrics.framePadding = {5.f, 1.f};
        auto badgeStyle = Mosaic::styleScope(ui, badgeTheme, location);
        LayoutOptions layout;
        layout.height = Dimension::fixed(ui->currentStyle->metrics.lineHeight);
        size_t node = ui->addNode(Detail::NodeKind::Button, {}, type, layout, location, SemanticRole::Text, false, true);
        Context::Node & badge = ui->nodes[node];
        badge.fillBackground = true;
        Response response;
        response.id = badge.id;
        response.item = badge.item;
        badge.response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response filterChip(Context * ui, const Key & key, StringView label, bool * removed, const SourceLocation & location)
    {
        auto chipScope = Mosaic::scope(ui, key, location);
        LayoutOptions layout;
        layout.gap = 0.f;
        auto chip = Mosaic::row(ui, Key("Filter chip"), layout, location);
        Response response = Mosaic::smallButton(ui, Key("Filter label"), label, location);

        if(removed != nullptr)
        {
            Response close = Mosaic::smallButton(ui, Key("Remove filter"), "x", location);

            if(close.clicked() == true)
            {
                *removed = true;
                response = close;
            }
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response thumbnailPlaceholder(Context * ui, const Key & key, const Vec2 & size, StringView label, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = Dimension::fixed(std::max(0.f, size.x));
        layout.height = Dimension::fixed(std::max(0.f, size.y));
        Canvas placeholder = Mosaic::canvas(ui, key, "Thumbnail placeholder", layout, location);
        Rect bounds;

        if(placeholder.contentRect(&bounds) == true)
        {
            (void)placeholder.rect(bounds, ui->currentStyle->colors.frame);
            (void)placeholder.line({0.f, 0.f}, {bounds.width, bounds.height}, 1.f, ui->currentStyle->colors.border);
            (void)placeholder.line({bounds.width, 0.f}, {0.f, bounds.height}, 1.f, ui->currentStyle->colors.border);

            if(label.empty() == false)
            {
                Vec2 textSize;
                (void)placeholder.text({ui->currentStyle->metrics.padding, ui->currentStyle->metrics.padding}, label, ui->currentStyle->colors.textDisabled, &textSize);
            }
        }

        Response response;
        (void)Mosaic::itemResponse(ui, placeholder.id(), &response);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response thumbnailPlaceholder(Context * ui, const Vec2 & size, StringView label, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = Dimension::fixed(std::max(0.f, size.x));
        layout.height = Dimension::fixed(std::max(0.f, size.y));
        Canvas placeholder = Mosaic::canvas(ui, "Thumbnail placeholder", layout, location);
        Rect bounds;

        if(placeholder.contentRect(&bounds) == true)
        {
            (void)placeholder.rect(bounds, ui->currentStyle->colors.frame);
            (void)placeholder.line({0.f, 0.f}, {bounds.width, bounds.height}, 1.f, ui->currentStyle->colors.border);
            (void)placeholder.line({bounds.width, 0.f}, {0.f, bounds.height}, 1.f, ui->currentStyle->colors.border);

            if(label.empty() == false)
            {
                Vec2 textSize;
                (void)placeholder.text({ui->currentStyle->metrics.padding, ui->currentStyle->metrics.padding}, label, ui->currentStyle->colors.textDisabled, &textSize);
            }
        }

        Response response;
        (void)Mosaic::itemResponse(ui, placeholder.id(), &response);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response editorStateMessage(Context * ui, StringView title, StringView detail, const Icon & iconValue, const SourceLocation & location)
    {
        auto message = Mosaic::row(ui, {}, location);
        Response response;
        response.id = message.id();

        if(iconValue.texture != 0 || iconValue.semanticFallback.empty() == false)
        {
            response = Mosaic::icon(ui, iconValue, location);
        }

        auto labels = Mosaic::column(ui, {}, location);
        (void)Mosaic::text(ui, title, location);
        Theme detailTheme = *ui->currentStyle;
        detailTheme.colors.text = ui->currentStyle->colors.textDisabled;
        auto detailStyle = Mosaic::styleScope(ui, detailTheme, location);
        TextOptions textOptions;
        textOptions.wordWrap = true;
        textOptions.layout.width = SizeRule::Fill;
        (void)Mosaic::text(ui, detail, textOptions, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Canvas beginDesignSurface(Context * ui, const Key & key, StringView label, DesignSurfaceState * state, const DesignSurfaceOptions & options, DesignSurfaceResponse * const _out, const SourceLocation & location)
    {
        Canvas surface;
        Response response = Detail::editorCanvas(ui, key, label, options.layout, options.panButton, &surface, location);
        DesignSurfaceResponse result;
        result.response = response;
        (void)surface.contentRect(&result.bounds);
        Vec2 pointer;
        bool hasPointer = surface.localPointerPosition(&pointer);
        DesignSurfaceState localState;
        DesignSurfaceState * resolvedState = state == nullptr ? &localState : state;
        float previousZoom = std::clamp(resolvedState->zoom, options.minimumZoom, options.maximumZoom);
        resolvedState->zoom = previousZoom;
        Vec2 wheel;

        if(Mosaic::consumeWheel(ui, surface.id(), &wheel) == true && hasPointer == true)
        {
            Vec2 relative = pointer - resolvedState->pan;
            Vec2 worldBefore = relative * (1.f / previousZoom);
            float zoomFactor = std::exp(-wheel.y * options.wheelZoomSpeed);
            float updatedZoom = std::clamp(previousZoom * zoomFactor, options.minimumZoom, options.maximumZoom);
            resolvedState->pan = pointer - worldBefore * updatedZoom;
            resolvedState->zoom = updatedZoom;
            result.zoomDelta = updatedZoom - previousZoom;
        }

        const PointerState * pointerState = ui->input.primaryPointer();

        if(response.pressed() == true && pointerState != nullptr)
        {
            resolvedState->pointerAnchor = pointerState->position;
            resolvedState->pointerTracked = true;
        }

        if(response.active() == true && pointerState != nullptr)
        {
            Vec2 delta = pointerState->position - resolvedState->pointerAnchor;
            resolvedState->pointerAnchor = pointerState->position;
            resolvedState->pan = resolvedState->pan + delta;
            result.panDelta = delta;
            result.panning = true;
        }

        if(response.released() == true || response.canceled() == true)
        {
            resolvedState->pointerTracked = false;
        }

        if(hasPointer == true && pointerState != nullptr && pointerState->isDown(PointerButton::Primary) == true && result.bounds.empty() == false)
        {
            float margin = std::max(1.f, options.autoPanMargin);
            Vec2 autoPan;

            if(pointer.x < margin)
            {
                autoPan.x = std::clamp((margin - pointer.x) / margin, 0.f, 1.f);
            }

            if(pointer.x > result.bounds.width - margin)
            {
                autoPan.x = -std::clamp((pointer.x - (result.bounds.width - margin)) / margin, 0.f, 1.f);
            }

            if(pointer.y < margin)
            {
                autoPan.y = std::clamp((margin - pointer.y) / margin, 0.f, 1.f);
            }

            if(pointer.y > result.bounds.height - margin)
            {
                autoPan.y = -std::clamp((pointer.y - (result.bounds.height - margin)) / margin, 0.f, 1.f);
            }

            if(autoPan.x != 0.f || autoPan.y != 0.f)
            {
                Vec2 delta = autoPan * (std::max(0.f, options.autoPanSpeed) * ui->input.deltaTime);
                resolvedState->pan = resolvedState->pan + delta;
                result.panDelta = result.panDelta + delta;
                result.autoPanning = true;
            }
        }

        Transform2D transform;
        transform.translation = resolvedState->pan;
        transform.axisX = {resolvedState->zoom, 0.f};
        transform.axisY = {0.f, resolvedState->zoom};
        result.worldTransform = transform;

        if(hasPointer == true)
        {
            (void)Detail::inversePoint(transform, pointer, &result.pointerWorld);
        }

        Response selectionResponse;

        if(options.selectionMode != DesignSelectionMode::None && result.bounds.empty() == false)
        {
            Detail::ItemBehaviorOptions selectionBehavior;
            selectionBehavior.pointerButton = PointerButton::Primary;
            selectionBehavior.keyboardActivation = false;
            selectionResponse = Detail::absoluteInteraction(ui, Key("Design surface selection"), result.bounds, selectionBehavior, true, location);

            if(selectionResponse.pressed() == true && hasPointer == true)
            {
                resolvedState->selectionAnchor = result.pointerWorld;
                resolvedState->selectionBounds = {result.pointerWorld.x, result.pointerWorld.y, 0.f, 0.f};
                resolvedState->selecting = true;
                resolvedState->lasso.clear();

                if(options.selectionMode == DesignSelectionMode::Lasso)
                {
                    resolvedState->lasso.push_back(result.pointerWorld);
                }
            }

            if(selectionResponse.active() == true && resolvedState->selecting == true && hasPointer == true)
            {
                if(options.selectionMode == DesignSelectionMode::Marquee)
                {
                    float minimumX = std::min(resolvedState->selectionAnchor.x, result.pointerWorld.x);
                    float minimumY = std::min(resolvedState->selectionAnchor.y, result.pointerWorld.y);
                    float maximumX = std::max(resolvedState->selectionAnchor.x, result.pointerWorld.x);
                    float maximumY = std::max(resolvedState->selectionAnchor.y, result.pointerWorld.y);
                    resolvedState->selectionBounds = {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
                    result.selectionBounds = resolvedState->selectionBounds;
                    result.marquee = true;
                }
                else if(options.selectionMode == DesignSelectionMode::Lasso)
                {
                    bool appendPoint = resolvedState->lasso.empty() == true;

                    if(appendPoint == false)
                    {
                        Vec2 difference = result.pointerWorld - resolvedState->lasso.back();
                        float distanceSquared = difference.x * difference.x + difference.y * difference.y;
                        float minimumDistance = 2.f / std::max(0.001f, resolvedState->zoom);
                        appendPoint = distanceSquared >= minimumDistance * minimumDistance;
                    }

                    if(appendPoint == true)
                    {
                        resolvedState->lasso.push_back(result.pointerWorld);
                        result.lassoChanged = true;
                    }
                }
            }

            if(selectionResponse.released() == true || selectionResponse.canceled() == true)
            {
                result.selectionFinished = resolvedState->selecting;
                result.selectionBounds = resolvedState->selectionBounds;
                resolvedState->selecting = false;
            }
        }

        if(state != nullptr)
        {
            result.lasso = Vec2Span(resolvedState->lasso.data(), resolvedState->lasso.size());
        }

        (void)surface.setLayer(CanvasLayer::Background);

        if(options.drawGrid == true && result.bounds.empty() == false)
        {
            GridStyle grid = options.grid;
            Transform2D scaleTransform;
            scaleTransform.axisX = {resolvedState->zoom, 0.f};
            scaleTransform.axisY = {0.f, resolvedState->zoom};
            grid.origin = resolvedState->pan + Detail::transformPoint(scaleTransform, options.grid.origin);
            grid.axisX = grid.axisX * resolvedState->zoom;
            grid.axisY = grid.axisY * resolvedState->zoom;
            (void)surface.grid(result.bounds, grid);
        }

        if(options.drawRulers == true && result.bounds.empty() == false)
        {
            float rulerSize = std::max(0.f, options.rulerSize);
            Color rulerColor = ui->currentStyle->colors.panel;
            (void)surface.rect({0.f, 0.f, result.bounds.width, rulerSize}, rulerColor);
            (void)surface.rect({0.f, 0.f, rulerSize, result.bounds.height}, rulerColor);
            (void)surface.line({rulerSize, rulerSize}, {result.bounds.width, rulerSize}, 1.f, ui->currentStyle->colors.separator);
            (void)surface.line({rulerSize, rulerSize}, {rulerSize, result.bounds.height}, 1.f, ui->currentStyle->colors.separator);
        }

        (void)surface.setLayer(CanvasLayer::Overlay);
        (void)surface.pushTransform(transform, StrokeScale::Screen);

        for(const DesignGuide & guide : options.guides)
        {
            (void)surface.line(guide.begin, guide.end, 1.f, guide.color);
        }

        if(result.marquee == true)
        {
            Color fill = ui->currentStyle->colors.accent;
            fill.a *= 0.12f;
            BoxStyle selectionStyle;
            selectionStyle.fill = Mosaic::solidFill(fill);
            selectionStyle.borderWidth = 1.f;
            selectionStyle.borderColor = ui->currentStyle->colors.accent;
            (void)surface.box(result.selectionBounds, selectionStyle);
        }

        if(resolvedState->lasso.size() > 1)
        {
            (void)surface.polyline(Vec2Span(resolvedState->lasso.data(), resolvedState->lasso.size()), 1.f, ui->currentStyle->colors.accent, false);
        }

        (void)surface.popTransform();
        (void)surface.setLayer(CanvasLayer::Local);
        (void)surface.pushTransform(transform, StrokeScale::Screen);

        if(_out != nullptr)
        {
            *_out = result;
        }

        return surface;
    }
    //////////////////////////////////////////////////////////////////////////
    bool endDesignSurface(Canvas * surface)
    {
        if(surface == nullptr)
        {
            return false;
        }

        if(surface->setLayer(CanvasLayer::Local) == false)
        {
            return false;
        }

        bool result = surface->popTransform();

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    GizmoResponse gizmo(Context * ui, Canvas & canvasValue, const Key & key, const Rect & bounds, const GizmoOptions & options, SnapProvider snapProvider, void * snapUserData, const SourceLocation & location)
    {
        GizmoResponse result;
        Transform2D transform = Detail::canvasInteractionTransform(ui, canvasValue.id());
        Rect screenBounds = Detail::transformBounds(transform, bounds);
        float padding = std::max(options.size, options.thickness * 2.f);
        screenBounds = {screenBounds.x - padding, screenBounds.y - padding, screenBounds.width + padding * 2.f, screenBounds.height + padding * 2.f};
        Detail::ItemBehaviorOptions behavior;
        behavior.keyboardActivation = true;
        result.response = Detail::absoluteInteraction(ui, key, screenBounds, behavior, options.enabled, location);
        Context::Persistent & persistent = ui->state(result.response.id);
        const PointerState * pointer = ui->input.primaryPointer();

        if(result.response.pressed() == true && pointer != nullptr)
        {
            persistent.dragLastPosition = pointer->position;
            result.phase = EditorTransactionPhase::Begin;
        }

        if(result.response.active() == true && pointer != nullptr)
        {
            Vec2 screenDelta = pointer->position - persistent.dragLastPosition;
            persistent.dragLastPosition = pointer->position;
            Vec2 worldDelta;

            if(Detail::inverseVector(transform, screenDelta, &worldDelta) == true)
            {
                SnapResult snap;

                if(snapProvider != nullptr)
                {
                    SnapQuery query;
                    query.owner = result.response.id;
                    query.movingBounds = bounds;
                    query.proposedDelta = worldDelta;
                    query.threshold = options.size;

                    if(snapProvider(query, &snap, snapUserData) == true)
                    {
                        worldDelta = snap.adjustedDelta;

                        if(snap.snappedX == true)
                        {
                            (void)canvasValue.line(snap.firstGuideBegin, snap.firstGuideEnd, options.thickness, options.color);
                        }

                        if(snap.snappedY == true)
                        {
                            (void)canvasValue.line(snap.secondGuideBegin, snap.secondGuideEnd, options.thickness, options.color);
                        }
                    }
                }

                result.delta = worldDelta;
                result.scalarDelta = options.kind == GizmoKind::Rotation ? worldDelta.x : (std::abs(worldDelta.x) >= std::abs(worldDelta.y) ? worldDelta.x : worldDelta.y);
                result.phase = EditorTransactionPhase::Change;
                Detail::setFlag(result.response, 6);
            }
        }

        if(result.response.released() == true)
        {
            result.phase = EditorTransactionPhase::Commit;
        }

        if(result.response.canceled() == true)
        {
            result.phase = EditorTransactionPhase::Cancel;
        }

        switch(options.kind)
        {
        case GizmoKind::Point:
            (void)canvasValue.circleFilled({bounds.x, bounds.y}, options.size * 0.5f, options.color);
            break;
        case GizmoKind::Axis:
            (void)canvasValue.line({bounds.x, bounds.y}, {bounds.right(), bounds.bottom()}, options.thickness, options.color);
            (void)canvasValue.circleFilled({bounds.right(), bounds.bottom()}, options.size * 0.5f, options.color);
            break;
        case GizmoKind::Rotation:
            (void)canvasValue.circle({bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f}, std::max(bounds.width, bounds.height) * 0.5f, options.thickness, options.color);
            break;
        default:
            BoxStyle handleStyle;
            handleStyle.fill = Mosaic::solidFill({options.color.r, options.color.g, options.color.b, 0.12f});
            handleStyle.borderWidth = options.thickness;
            handleStyle.borderColor = options.color;
            (void)canvasValue.box(bounds, handleStyle);
            break;
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    LayoutBoxEditResponse layoutBoxEditor(Context * ui, Canvas & canvasValue, const Key & key, LayoutBoxCellSpan cells, LayoutBoxSplitterSpan splitters, const LayoutBoxEditOptions & options, SnapProvider snapProvider, void * snapUserData, const SourceLocation & location)
    {
        LayoutBoxEditResponse result;
        auto editorScope = Mosaic::scope(ui, key, location);

        for(const LayoutBoxCell & cell : cells)
        {
            auto cellScope = Mosaic::scope(ui, Key(cell.id), location);
            Color cellColor = cell.selected == true ? ui->currentStyle->colors.accent : ui->currentStyle->colors.borderStrong;

            if(options.drawCellBounds == true)
            {
                BoxStyle cellStyle;
                cellStyle.fill = Mosaic::solidFill({0.f, 0.f, 0.f, 0.f});
                cellStyle.borderWidth = 1.f;
                cellStyle.borderColor = cellColor;
                (void)canvasValue.box(cell.bounds, cellStyle);
            }

            if(options.drawSafeArea == true && cell.safeArea == true && cell.contentBounds.empty() == false)
            {
                Color safeAreaColor = ui->currentStyle->colors.warning;
                safeAreaColor.a *= 0.72f;
                BoxStyle safeAreaStyle;
                safeAreaStyle.fill = Mosaic::solidFill({0.f, 0.f, 0.f, 0.f});
                safeAreaStyle.borderWidth = 1.f;
                safeAreaStyle.borderColor = safeAreaColor;
                (void)canvasValue.box(cell.contentBounds, safeAreaStyle);
            }

            Detail::ItemBehaviorOptions behavior;
            behavior.keyboardActivation = true;
            Response cellResponse = Detail::absoluteInteraction(ui, Key("Layout cell"), cell.bounds, behavior, true, location);

            if(cellResponse.clicked() == true && result.kind == LayoutBoxEditKind::None)
            {
                result.response = cellResponse;
                result.kind = LayoutBoxEditKind::Select;
                result.item = cell.id;
                result.phase = EditorTransactionPhase::Commit;
            }
        }

        constexpr float snapRatios[] = {0.25f, 1.f / 3.f, 0.5f, 2.f / 3.f, 0.75f};

        for(const LayoutBoxSplitter & splitter : splitters)
        {
            auto splitterScope = Mosaic::scope(ui, Key(splitter.id), location);
            GizmoOptions gizmoOptions;
            gizmoOptions.kind = GizmoKind::Splitter;
            gizmoOptions.size = std::max(1.f, options.handleThickness);
            gizmoOptions.color = ui->currentStyle->colors.accent;
            GizmoResponse splitterResponse = Mosaic::gizmo(ui, canvasValue, Key("Layout splitter"), splitter.bounds, gizmoOptions, snapProvider, snapUserData, location);

            if(splitterResponse.response.pressed() == false && splitterResponse.response.active() == false && splitterResponse.response.released() == false && splitterResponse.response.canceled() == false)
            {
                continue;
            }

            result.response = splitterResponse.response;
            result.kind = LayoutBoxEditKind::MoveSplitter;
            result.item = splitter.id;
            result.before = splitter.before;
            result.after = splitter.after;
            result.delta = splitterResponse.delta;
            result.phase = splitterResponse.phase;
            result.paired = options.pairedEdges == true || ui->input.modifiers.shift == true;

            const LayoutBoxCell * before = nullptr;
            const LayoutBoxCell * after = nullptr;

            for(const LayoutBoxCell & cell : cells)
            {
                if(cell.id == splitter.before)
                {
                    before = &cell;
                }

                if(cell.id == splitter.after)
                {
                    after = &cell;
                }
            }

            if(before == nullptr || after == nullptr)
            {
                continue;
            }

            bool horizontal = splitter.orientation == Orientation::Horizontal;
            float combinedBegin = horizontal == true ? before->bounds.x : before->bounds.y;
            float combinedEnd = horizontal == true ? after->bounds.right() : after->bounds.bottom();
            float splitterPosition = horizontal == true ? splitter.bounds.x + result.delta.x : splitter.bounds.y + result.delta.y;
            float combinedExtent = combinedEnd - combinedBegin;

            if(combinedExtent <= 0.f)
            {
                continue;
            }

            float minimumPosition = combinedBegin + std::max(before->minimumSize, splitter.beforeMinimum);
            float maximumPosition = combinedEnd - std::max(after->minimumSize, splitter.afterMinimum);
            splitterPosition = std::clamp(splitterPosition, minimumPosition, maximumPosition);
            float resolvedDelta = splitterPosition - (horizontal == true ? splitter.bounds.x : splitter.bounds.y);

            if(options.snapping == true && options.snappingTemporarilyDisabled == false)
            {
                float threshold = std::max(0.f, options.snapThreshold);

                for(float ratio : snapRatios)
                {
                    float candidate = combinedBegin + combinedExtent * ratio;
                    float distance = std::abs(candidate - splitterPosition);

                    if(distance > threshold)
                    {
                        continue;
                    }

                    splitterPosition = candidate;
                    resolvedDelta = splitterPosition - (horizontal == true ? splitter.bounds.x : splitter.bounds.y);
                    result.snapRatio = ratio;
                    result.snapped = true;

                    if(horizontal == true)
                    {
                        (void)canvasValue.line({candidate, before->bounds.y}, {candidate, before->bounds.bottom()}, 1.f, ui->currentStyle->colors.accent);
                    }
                    else
                    {
                        (void)canvasValue.line({before->bounds.x, candidate}, {before->bounds.right(), candidate}, 1.f, ui->currentStyle->colors.accent);
                    }

                    break;
                }
            }

            if(horizontal == true)
            {
                result.delta.x = resolvedDelta;
                result.delta.y = 0.f;
            }
            else
            {
                result.delta.x = 0.f;
                result.delta.y = resolvedDelta;
            }
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    Response timeline(Context * ui, const Key & key, StringView label, TimelineTrackSpan tracks, TimelineKeyframeSpan keyframes, TimelineState * state, TimelineResponse * const _out, const LayoutOptions & layout, const SourceLocation & location)
    {
        TimelineOptions options;
        options.layout = layout;
        Response response = Mosaic::timeline(ui, key, label, tracks, keyframes, state, options, _out, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response timeline(Context * ui, const Key & key, StringView label, TimelineTrackSpan tracks, TimelineKeyframeSpan keyframes, TimelineState * state, const TimelineOptions & options, TimelineResponse * const _out, const SourceLocation & location)
    {
        TimelineResponse timelineResult;
        Canvas canvasValue;
        Response response = Detail::editorCanvas(ui, key, label, options.layout, PointerButton::Primary, &canvasValue, location);
        Rect bounds;

        if(canvasValue.contentRect(&bounds) == true && state != nullptr)
        {
            double duration = std::max(0.000001, state->visibleEnd - state->visibleBegin);
            float rulerHeight = ui->currentStyle->metrics.controlHeight;
            float contentX = std::clamp(state->trackWidth, 0.f, bounds.width);
            float contentWidth = std::max(0.f, bounds.width - contentX);
            (void)canvasValue.rect(bounds, ui->currentStyle->colors.background);
            (void)canvasValue.rect({0.f, 0.f, contentX, bounds.height}, ui->currentStyle->colors.panel);
            (void)canvasValue.line({contentX, 0.f}, {contentX, bounds.height}, 1.f, ui->currentStyle->colors.separator);

            for(uint32_t tick = 0; tick <= 10; ++tick)
            {
                float ratio = static_cast<float>(tick) / 10.f;
                float x = contentX + ratio * contentWidth;
                (void)canvasValue.line({x, 0.f}, {x, bounds.height}, tick % 5U == 0U ? 1.f : 0.5f, tick % 5U == 0U ? ui->currentStyle->colors.separator : ui->currentStyle->colors.tableBorderLight);
            }

            float workBeginRatio = static_cast<float>((state->workBegin - state->visibleBegin) / duration);
            float workEndRatio = static_cast<float>((state->workEnd - state->visibleBegin) / duration);
            float loopBeginRatio = static_cast<float>((state->loopBegin - state->visibleBegin) / duration);
            float loopEndRatio = static_cast<float>((state->loopEnd - state->visibleBegin) / duration);
            Color workColor = ui->currentStyle->colors.accent;
            workColor.a *= 0.18f;
            (void)canvasValue.rect({contentX + workBeginRatio * contentWidth, 0.f, (workEndRatio - workBeginRatio) * contentWidth, rulerHeight}, workColor);
            Color loopColor = ui->currentStyle->colors.warning;
            loopColor.a *= 0.72f;
            (void)canvasValue.line({contentX + loopBeginRatio * contentWidth, rulerHeight - 3.f}, {contentX + loopEndRatio * contentWidth, rulerHeight - 3.f}, 2.f, loopColor);

            for(double marker : options.markers)
            {
                float markerRatio = static_cast<float>((marker - state->visibleBegin) / duration);
                float markerX = contentX + markerRatio * contentWidth;
                (void)canvasValue.line({markerX, 0.f}, {markerX, bounds.height}, 1.f, ui->currentStyle->colors.warning);
            }

            float trackHeight = std::max(1.f, state->trackHeight);
            float visibleTrackExtent = std::max(0.f, bounds.height - rulerHeight);
            float maximumTrackScroll = std::max(0.f, static_cast<float>(tracks.size()) * trackHeight - visibleTrackExtent);
            Vec2 wheel;

            if(Mosaic::consumeWheel(ui, canvasValue.id(), &wheel) == true)
            {
                state->trackScroll = std::clamp(state->trackScroll + wheel.y * trackHeight * 2.f, 0.f, maximumTrackScroll);
            }

            state->trackScroll = std::clamp(state->trackScroll, 0.f, maximumTrackScroll);
            size_t firstTrack = static_cast<size_t>(state->trackScroll / trackHeight);
            float firstTrackOffset = state->trackScroll - static_cast<float>(firstTrack) * trackHeight;
            size_t visibleTrackCount = static_cast<size_t>(std::ceil(visibleTrackExtent / trackHeight)) + 1;
            size_t lastTrack = std::min(tracks.size(), firstTrack + visibleTrackCount);
            Context::TimelineInteractionState & interaction = ui->timelineInteractions[response.id];

            for(size_t trackIndex = firstTrack; trackIndex != lastTrack; ++trackIndex)
            {
                const TimelineTrack & track = tracks[trackIndex];
                float y = rulerHeight + static_cast<float>(trackIndex - firstTrack) * trackHeight - firstTrackOffset;
                Color rowColor = track.selected ? ui->currentStyle->colors.headerActive : (trackIndex % 2U == 0U ? ui->currentStyle->colors.tableRow : ui->currentStyle->colors.tableRowAlternate);
                (void)canvasValue.rect({0.f, y, bounds.width, trackHeight}, rowColor);
                Vec2 textSize;
                (void)canvasValue.text({ui->currentStyle->metrics.padding + static_cast<float>(track.depth) * ui->currentStyle->metrics.indent, y + 2.f}, track.label, track.locked ? ui->currentStyle->colors.textDisabled : ui->currentStyle->colors.text, &textSize);
            }

            for(const TimelineKeyframe & keyframe : keyframes)
            {
                size_t trackIndex = firstTrack;

                for(; trackIndex != lastTrack; ++trackIndex)
                {
                    if(tracks[trackIndex].id == keyframe.track)
                    {
                        break;
                    }
                }

                if(trackIndex == lastTrack)
                {
                    continue;
                }

                float ratio = static_cast<float>((keyframe.time - state->visibleBegin) / duration);
                float x = contentX + ratio * contentWidth;
                float y = rulerHeight + (static_cast<float>(trackIndex - firstTrack) + 0.5f) * trackHeight - firstTrackOffset;
                (void)canvasValue.regularPolygonFilled({x, y}, 5.f, 4, 0.7853982f, keyframe.selected ? ui->currentStyle->colors.accent : ui->currentStyle->colors.sliderGrab);
                auto keyframeScope = Mosaic::scope(ui, Key(keyframe.id), location);
                Detail::ItemBehaviorOptions keyframeBehavior;
                keyframeBehavior.keyboardActivation = true;
                Response keyframeResponse = Detail::absoluteInteraction(ui, Key("Timeline keyframe"), {x - 7.f, y - 7.f, 14.f, 14.f}, keyframeBehavior, tracks[trackIndex].locked == false, location);

                if(keyframeResponse.pressed() == true)
                {
                    const PointerState * keyframePointer = ui->input.primaryPointer();
                    interaction.keyframe = keyframe.id;
                    interaction.keyframeBeginTime = keyframe.time;
                    interaction.keyframeTime = keyframe.time;

                    if(keyframePointer != nullptr)
                    {
                        interaction.keyframePointerAnchor = keyframePointer->position;
                    }

                    timelineResult.item = keyframe.id;
                    timelineResult.time = keyframe.time;
                    timelineResult.phase = EditorTransactionPhase::Begin;
                }

                const PointerState * keyframePointer = ui->input.primaryPointer();

                if(keyframeResponse.active() == true && keyframePointer != nullptr && contentWidth > 0.f && interaction.keyframe == keyframe.id)
                {
                    float pointerDistance = keyframePointer->position.x - interaction.keyframePointerAnchor.x;
                    double delta = static_cast<double>(pointerDistance / contentWidth) * duration;
                    double candidate = interaction.keyframeBeginTime + delta;
                    double pixelToTime = duration / static_cast<double>(contentWidth);
                    double threshold = static_cast<double>(std::max(0.f, options.snapThreshold)) * pixelToTime;

                    if(options.snapFrames == true && options.frameRate > 0.0)
                    {
                        double frame = std::round(candidate * options.frameRate) / options.frameRate;

                        if(std::abs(frame - candidate) <= threshold)
                        {
                            candidate = frame;
                            timelineResult.snapped = true;
                        }
                    }

                    if(options.snapProvider != nullptr)
                    {
                        TimelineSnapQuery query;
                        query.owner = keyframe.id;
                        query.time = candidate;
                        query.threshold = threshold;
                        query.frames = options.snapFrames;
                        query.keyframes = options.snapKeyframes;
                        query.markers = options.snapMarkers;
                        query.boundaries = options.snapBoundaries;
                        TimelineSnapResult snap;

                        if(options.snapProvider(query, &snap, options.snapUserData) == true && snap.snapped == true)
                        {
                            candidate = snap.time;
                            timelineResult.snapped = true;
                        }
                    }

                    interaction.keyframeTime = candidate;
                    timelineResult.item = keyframe.id;
                    timelineResult.time = candidate;
                    timelineResult.timeDelta = candidate - interaction.keyframeBeginTime;
                    timelineResult.keyframesChanged = timelineResult.timeDelta != 0.0;
                    timelineResult.duplicateRequested = options.allowDuplicate == true && ui->input.modifiers.alt == true;
                    timelineResult.scaleRequested = options.allowScale == true && ui->input.modifiers.shift == true;
                    timelineResult.phase = EditorTransactionPhase::Change;
                    Detail::setFlag(response, 6, timelineResult.keyframesChanged);
                }

                if(keyframeResponse.released() == true)
                {
                    timelineResult.item = keyframe.id;
                    timelineResult.time = interaction.keyframeTime;
                    timelineResult.timeDelta = interaction.keyframeTime - interaction.keyframeBeginTime;
                    timelineResult.keyframesChanged = timelineResult.timeDelta != 0.0;
                    timelineResult.phase = EditorTransactionPhase::Commit;
                    interaction.keyframe = InvalidId;
                }

                if(keyframeResponse.canceled() == true)
                {
                    timelineResult.item = keyframe.id;
                    timelineResult.time = interaction.keyframeBeginTime;
                    timelineResult.timeDelta = 0.0;
                    timelineResult.phase = EditorTransactionPhase::Cancel;
                    interaction.keyframe = InvalidId;
                }
            }

            float playheadRatio = static_cast<float>((state->playhead - state->visibleBegin) / duration);
            float playheadX = contentX + playheadRatio * contentWidth;
            (void)canvasValue.line({playheadX, 0.f}, {playheadX, bounds.height}, 1.f, ui->currentStyle->colors.error);
            Vec2 pointer;
            bool hasPointer = canvasValue.localPointerPosition(&pointer);

            if(response.pressed() == true && hasPointer == true && pointer.x >= contentX && pointer.y > rulerHeight && contentWidth > 0.f)
            {
                interaction.selectionAnchor = pointer;
                interaction.selectionBounds = {pointer.x, pointer.y, 0.f, 0.f};
                interaction.selectionBegin = state->visibleBegin + duration * static_cast<double>((pointer.x - contentX) / contentWidth);
                interaction.selectionEnd = interaction.selectionBegin;
                interaction.firstSelectedTrack = std::min(tracks.size(), static_cast<size_t>((state->trackScroll + std::max(0.f, pointer.y - rulerHeight)) / trackHeight));
                interaction.lastSelectedTrack = std::min(tracks.size(), interaction.firstSelectedTrack + 1U);
                interaction.selecting = true;
                timelineResult.phase = EditorTransactionPhase::Begin;
            }

            if(response.active() == true && hasPointer == true && pointer.x >= contentX && pointer.y <= rulerHeight && contentWidth > 0.f)
            {
                double previous = state->playhead;
                float ratio = std::clamp((pointer.x - contentX) / contentWidth, 0.f, 1.f);
                state->playhead = state->visibleBegin + duration * static_cast<double>(ratio);
                timelineResult.time = state->playhead;
                timelineResult.timeDelta = state->playhead - previous;
                timelineResult.playheadChanged = state->playhead != previous;
                timelineResult.phase = response.pressed() == true ? EditorTransactionPhase::Begin : EditorTransactionPhase::Change;
                Detail::setFlag(response, 6, timelineResult.playheadChanged);
            }

            if(response.active() == true && interaction.selecting == true && hasPointer == true && contentWidth > 0.f)
            {
                float minimumX = std::clamp(std::min(interaction.selectionAnchor.x, pointer.x), contentX, bounds.width);
                float maximumX = std::clamp(std::max(interaction.selectionAnchor.x, pointer.x), contentX, bounds.width);
                float minimumY = std::clamp(std::min(interaction.selectionAnchor.y, pointer.y), rulerHeight, bounds.height);
                float maximumY = std::clamp(std::max(interaction.selectionAnchor.y, pointer.y), rulerHeight, bounds.height);
                interaction.selectionBounds = {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
                interaction.selectionBegin = state->visibleBegin + duration * static_cast<double>((minimumX - contentX) / contentWidth);
                interaction.selectionEnd = state->visibleBegin + duration * static_cast<double>((maximumX - contentX) / contentWidth);
                float firstTrackPosition = state->trackScroll + std::max(0.f, minimumY - rulerHeight);
                float lastTrackPosition = state->trackScroll + std::max(0.f, maximumY - rulerHeight);
                interaction.firstSelectedTrack = std::min(tracks.size(), static_cast<size_t>(firstTrackPosition / trackHeight));
                interaction.lastSelectedTrack = std::min(tracks.size(), static_cast<size_t>(lastTrackPosition / trackHeight) + 1U);
                timelineResult.selectionBounds = interaction.selectionBounds;
                timelineResult.selectionBegin = interaction.selectionBegin;
                timelineResult.selectionEnd = interaction.selectionEnd;
                timelineResult.firstSelectedTrack = interaction.firstSelectedTrack;
                timelineResult.lastSelectedTrack = interaction.lastSelectedTrack;
                timelineResult.marquee = true;
                timelineResult.phase = EditorTransactionPhase::Change;
                Color selectionFill = ui->currentStyle->colors.accent;
                selectionFill.a *= 0.16f;
                BoxStyle selectionStyle;
                selectionStyle.fill = Mosaic::solidFill(selectionFill);
                selectionStyle.borderWidth = 1.f;
                selectionStyle.borderColor = ui->currentStyle->colors.accent;
                (void)canvasValue.box(timelineResult.selectionBounds, selectionStyle);
            }

            if(response.released() == true)
            {
                timelineResult.phase = EditorTransactionPhase::Commit;
                timelineResult.selectionFinished = interaction.selecting;

                if(interaction.selecting == true)
                {
                    timelineResult.selectionBounds = interaction.selectionBounds;
                    timelineResult.selectionBegin = interaction.selectionBegin;
                    timelineResult.selectionEnd = interaction.selectionEnd;
                    timelineResult.firstSelectedTrack = interaction.firstSelectedTrack;
                    timelineResult.lastSelectedTrack = interaction.lastSelectedTrack;
                }

                interaction.selecting = false;
            }

            if(response.canceled() == true)
            {
                timelineResult.phase = EditorTransactionPhase::Cancel;
                timelineResult.selectionFinished = interaction.selecting;

                if(interaction.selecting == true)
                {
                    timelineResult.selectionBounds = interaction.selectionBounds;
                    timelineResult.selectionBegin = interaction.selectionBegin;
                    timelineResult.selectionEnd = interaction.selectionEnd;
                    timelineResult.firstSelectedTrack = interaction.firstSelectedTrack;
                    timelineResult.lastSelectedTrack = interaction.lastSelectedTrack;
                }

                interaction.selecting = false;
            }
        }

        if(_out != nullptr)
        {
            *_out = timelineResult;
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response curveEditor(Context * ui, const Key & key, StringView label, CurvePointSpan points, const Rect & valueRange, CurveEditorResponse * const _out, const LayoutOptions & layout, const SourceLocation & location)
    {
        CurveEditorOptions options;
        options.layout = layout;
        Response response = Mosaic::curveEditor(ui, key, label, points, valueRange, options, _out, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response curveEditor(Context * ui, const Key & key, StringView label, CurvePointSpan points, const Rect & valueRange, const CurveEditorOptions & options, CurveEditorResponse * const _out, const SourceLocation & location)
    {
        CurveEditorResponse curveResult;
        Canvas canvasValue;
        Response response = Detail::editorCanvas(ui, key, label, options.layout, PointerButton::Primary, &canvasValue, location);
        Rect bounds;

        if(canvasValue.contentRect(&bounds) == true && valueRange.width > 0.f && valueRange.height > 0.f)
        {
            GridStyle grid;
            grid.origin = {0.f, bounds.height};
            grid.minorSpacing = std::max(8.f, bounds.width / 10.f);
            (void)canvasValue.grid(bounds, grid);
            auto mapPoint = [&bounds, &valueRange](const Vec2 & point)
            {
                float x = (point.x - valueRange.x) / valueRange.width * bounds.width;
                float y = bounds.height - (point.y - valueRange.y) / valueRange.height * bounds.height;

                return Vec2{x, y};
            };
            auto unmapPoint = [&bounds, &valueRange](const Vec2 & point)
            {
                float x = valueRange.x + point.x / bounds.width * valueRange.width;
                float y = valueRange.y + (bounds.height - point.y) / bounds.height * valueRange.height;

                return Vec2{x, y};
            };

            for(size_t index = 1; index < points.size(); ++index)
            {
                Vec2 first = mapPoint(points[index - 1].value);
                Vec2 firstControl = mapPoint(points[index - 1].value + points[index - 1].outgoingTangent);
                Vec2 secondControl = mapPoint(points[index].value + points[index].incomingTangent);
                Vec2 second = mapPoint(points[index].value);
                (void)canvasValue.bezierCubic(first, firstControl, secondControl, second, 2.f, points[index].color);
            }

            for(const CurvePoint & point : points)
            {
                Vec2 position = mapPoint(point.value);

                if(options.editTangents == true && point.selected == true)
                {
                    Vec2 incoming = mapPoint(point.value + point.incomingTangent);
                    Vec2 outgoing = mapPoint(point.value + point.outgoingTangent);
                    Color tangentColor = ui->currentStyle->colors.textDisabled;
                    (void)canvasValue.line(incoming, position, 1.f, tangentColor);
                    (void)canvasValue.line(position, outgoing, 1.f, tangentColor);
                    (void)canvasValue.circleFilled(incoming, options.tangentRadius, tangentColor);
                    (void)canvasValue.circleFilled(outgoing, options.tangentRadius, tangentColor);
                }

                float radius = point.selected == true ? options.pointRadius : std::max(1.f, options.pointRadius - 1.f);
                Color color = point.selected == true ? ui->currentStyle->colors.accent : point.color;
                (void)canvasValue.circleFilled(position, radius, color);
            }

            Vec2 pointer;

            if(response.pressed() == true && canvasValue.localPointerPosition(&pointer) == true)
            {
                float hitRadius = std::max(1.f, options.hitRadius);
                float nearestDistance = hitRadius * hitRadius;
                Id nearest = InvalidId;
                CurveEditorResponse::Handle nearestHandle = CurveEditorResponse::Handle::Point;

                for(const CurvePoint & point : points)
                {
                    if(options.editTangents == true && point.selected == true)
                    {
                        Vec2 incoming = mapPoint(point.value + point.incomingTangent);
                        Vec2 incomingDelta = pointer - incoming;
                        float incomingDistance = incomingDelta.x * incomingDelta.x + incomingDelta.y * incomingDelta.y;

                        if(incomingDistance < nearestDistance)
                        {
                            nearestDistance = incomingDistance;
                            nearest = point.id;
                            nearestHandle = CurveEditorResponse::Handle::IncomingTangent;
                        }

                        Vec2 outgoing = mapPoint(point.value + point.outgoingTangent);
                        Vec2 outgoingDelta = pointer - outgoing;
                        float outgoingDistance = outgoingDelta.x * outgoingDelta.x + outgoingDelta.y * outgoingDelta.y;

                        if(outgoingDistance < nearestDistance)
                        {
                            nearestDistance = outgoingDistance;
                            nearest = point.id;
                            nearestHandle = CurveEditorResponse::Handle::OutgoingTangent;
                        }
                    }

                    Vec2 position = mapPoint(point.value);
                    Vec2 delta = pointer - position;
                    float distance = delta.x * delta.x + delta.y * delta.y;

                    if(distance < nearestDistance)
                    {
                        nearestDistance = distance;
                        nearest = point.id;
                        nearestHandle = CurveEditorResponse::Handle::Point;
                    }
                }

                Context::CurveEditorInteractionState & state = ui->curveEditorInteractions[response.id];
                state.point = nearest;
                state.handle = nearestHandle;
                state.selectionAnchor = unmapPoint(pointer);
                state.boxSelecting = nearest == InvalidId && options.boxSelection == true;
                curveResult.point = nearest;
                curveResult.handle = nearestHandle;
                curveResult.boxSelection = state.boxSelecting;
                curveResult.phase = EditorTransactionPhase::Begin;
            }

            const PointerState * pointerState = ui->input.primaryPointer();

            if(response.active() == true && pointerState != nullptr)
            {
                Context::CurveEditorInteractionState & state = ui->curveEditorInteractions[response.id];
                curveResult.point = state.point;
                curveResult.handle = state.handle;

                if(state.boxSelecting == true && canvasValue.localPointerPosition(&pointer) == true)
                {
                    Vec2 current = unmapPoint(pointer);
                    float minimumX = std::min(state.selectionAnchor.x, current.x);
                    float minimumY = std::min(state.selectionAnchor.y, current.y);
                    float maximumX = std::max(state.selectionAnchor.x, current.x);
                    float maximumY = std::max(state.selectionAnchor.y, current.y);
                    curveResult.selectionBounds = {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
                    curveResult.boxSelection = true;
                    Rect visualBounds;
                    visualBounds.x = (minimumX - valueRange.x) / valueRange.width * bounds.width;
                    visualBounds.y = bounds.height - (maximumY - valueRange.y) / valueRange.height * bounds.height;
                    visualBounds.width = curveResult.selectionBounds.width / valueRange.width * bounds.width;
                    visualBounds.height = curveResult.selectionBounds.height / valueRange.height * bounds.height;
                    Color selectionFill = ui->currentStyle->colors.accent;
                    selectionFill.a *= 0.15f;
                    (void)canvasValue.rect(visualBounds, selectionFill);
                }
                else
                {
                    curveResult.delta.x = pointerState->delta.x / bounds.width * valueRange.width;
                    curveResult.delta.y = -pointerState->delta.y / bounds.height * valueRange.height;
                    curveResult.changed = curveResult.point != InvalidId && (curveResult.delta.x != 0.f || curveResult.delta.y != 0.f);
                }

                curveResult.phase = EditorTransactionPhase::Change;
                Detail::setFlag(response, 6, curveResult.changed);
            }

            if(response.released() == true || response.canceled() == true)
            {
                auto iterator = ui->curveEditorInteractions.find(response.id);

                if(iterator != ui->curveEditorInteractions.end())
                {
                    curveResult.point = iterator->second.point;
                    curveResult.handle = iterator->second.handle;
                    curveResult.boxSelection = iterator->second.boxSelecting;
                    ui->curveEditorInteractions.erase(iterator);
                }

                curveResult.phase = response.canceled() == true ? EditorTransactionPhase::Cancel : EditorTransactionPhase::Commit;
            }
        }

        if(_out != nullptr)
        {
            *_out = curveResult;
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response nodeGraph(Context * ui, const Key & key, StringView label, GraphNodeSpan nodes, GraphLinkSpan links, DesignSurfaceState * state, NodeGraphResponse * const _out, const LayoutOptions & layout, const SourceLocation & location)
    {
        NodeGraphOptions options;
        options.layout = layout;
        Response response = Mosaic::nodeGraph(ui, key, label, nodes, links, state, options, _out, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response nodeGraph(Context * ui, const Key & key, StringView label, GraphNodeSpan nodes, GraphLinkSpan links, DesignSurfaceState * state, const NodeGraphOptions & options, NodeGraphResponse * const _out, const SourceLocation & location)
    {
        NodeGraphResponse graphResult;
        DesignSurfaceOptions surfaceOptions;
        surfaceOptions.layout = options.layout;
        DesignSurfaceResponse surfaceResponse;
        Canvas canvasValue = Mosaic::beginDesignSurface(ui, key, label, state, surfaceOptions, &surfaceResponse, location);

        for(const GraphLink & link : links)
        {
            Vec2 from;
            Vec2 to;

            if(Detail::findGraphPin(nodes, link.outputPin, &from) == false)
            {
                continue;
            }

            if(Detail::findGraphPin(nodes, link.inputPin, &to) == false)
            {
                continue;
            }

            float tangent = std::max(40.f, std::abs(to.x - from.x) * 0.5f);
            (void)canvasValue.bezierCubic(from, {from.x + tangent, from.y}, {to.x - tangent, to.y}, to, 2.f, link.color);
        }

        for(const GraphNode & node : nodes)
        {
            BoxStyle style;
            style.fill.color = node.selected ? ui->currentStyle->colors.headerActive : ui->currentStyle->colors.panel;
            style.radii = {ui->currentStyle->metrics.cornerRadius, ui->currentStyle->metrics.cornerRadius, ui->currentStyle->metrics.cornerRadius, ui->currentStyle->metrics.cornerRadius};
            style.borderWidth = 1.f;
            style.borderColor = node.selected ? ui->currentStyle->colors.accent : ui->currentStyle->colors.borderStrong;
            (void)canvasValue.box(node.bounds, style);
            Vec2 textSize;
            (void)canvasValue.text({node.bounds.x + ui->currentStyle->metrics.padding, node.bounds.y + ui->currentStyle->metrics.padding}, node.title, ui->currentStyle->colors.text, &textSize);

            for(size_t pinIndex = 0; pinIndex != node.pins.size(); ++pinIndex)
            {
                const NodePin & pin = node.pins[pinIndex];
                Vec2 pinPosition = Detail::graphPinPosition(node, pin, pinIndex);
                (void)canvasValue.circleFilled(pinPosition, 5.f, pin.color);
            }

            GizmoOptions gizmoOptions;
            gizmoOptions.kind = GizmoKind::Box;
            gizmoOptions.color = node.selected ? ui->currentStyle->colors.accent : Color{0.f, 0.f, 0.f, 0.f};
            GizmoResponse nodeResponse = Mosaic::gizmo(ui, canvasValue, Key(node.id), node.bounds, gizmoOptions, nullptr, nullptr, location);

            if(nodeResponse.response.pressed() == true || nodeResponse.response.active() == true || nodeResponse.response.released() == true)
            {
                graphResult.node = node.id;
                graphResult.delta = nodeResponse.delta;
                graphResult.phase = nodeResponse.phase;
            }

            if(options.allowConnections == true)
            {
                for(size_t pinIndex = 0; pinIndex != node.pins.size(); ++pinIndex)
                {
                    const NodePin & pin = node.pins[pinIndex];
                    Vec2 pinPosition = Detail::graphPinPosition(node, pin, pinIndex);
                    float hitRadius = std::max(options.pinHitRadius, 5.f);
                    Rect pinBounds = {pinPosition.x - hitRadius, pinPosition.y - hitRadius, hitRadius * 2.f, hitRadius * 2.f};
                    GizmoOptions pinOptions;
                    pinOptions.kind = GizmoKind::Point;
                    pinOptions.color = pin.color;
                    pinOptions.size = 5.f;
                    GizmoResponse pinResponse = Mosaic::gizmo(ui, canvasValue, Key(pin.id), pinBounds, pinOptions, nullptr, nullptr, location);

                    if(pinResponse.response.pressed() == true)
                    {
                        Context::NodeGraphInteractionState & interaction = ui->nodeGraphInteractions[surfaceResponse.response.id];
                        interaction.pin = pin.id;
                        interaction.type = pin.type;
                        interaction.direction = pin.direction;
                        interaction.connecting = true;
                        graphResult.pin = pin.id;
                        graphResult.phase = EditorTransactionPhase::Begin;
                    }

                    if(pinResponse.response.active() == true)
                    {
                        graphResult.pin = pin.id;
                        graphResult.connectionPosition = surfaceResponse.pointerWorld;
                        graphResult.connectionPreview = true;
                        graphResult.phase = EditorTransactionPhase::Change;
                    }

                    if(pinResponse.response.released() == true || pinResponse.response.canceled() == true)
                    {
                        auto interactionIterator = ui->nodeGraphInteractions.find(surfaceResponse.response.id);

                        if(interactionIterator != ui->nodeGraphInteractions.end())
                        {
                            Context::NodeGraphInteractionState interaction = interactionIterator->second;
                            graphResult.pin = interaction.pin;

                            if(pinResponse.response.canceled() == false)
                            {
                                float targetDistance = options.pinHitRadius * options.pinHitRadius;
                                Detail::GraphPinLocation source;
                                Detail::GraphPinLocation target;
                                bool hasSource = Detail::findGraphPin(nodes, interaction.pin, &source);

                                for(const GraphNode & targetNode : nodes)
                                {
                                    for(size_t targetIndex = 0; targetIndex != targetNode.pins.size(); ++targetIndex)
                                    {
                                        const NodePin & targetPin = targetNode.pins[targetIndex];
                                        Vec2 targetPosition = Detail::graphPinPosition(targetNode, targetPin, targetIndex);
                                        Vec2 difference = surfaceResponse.pointerWorld - targetPosition;
                                        float distance = difference.x * difference.x + difference.y * difference.y;

                                        if(distance >= targetDistance)
                                        {
                                            continue;
                                        }

                                        if(hasSource == false || Detail::graphPinsCompatible(*source.pin, targetPin, options) == false)
                                        {
                                            continue;
                                        }

                                        targetDistance = distance;
                                        target.node = &targetNode;
                                        target.pin = &targetPin;
                                        target.position = targetPosition;
                                    }
                                }

                                if(hasSource == true && target.pin != nullptr)
                                {
                                    const NodePin & output = source.pin->direction == NodePinDirection::Output ? *source.pin : *target.pin;
                                    const NodePin & input = source.pin->direction == NodePinDirection::Input ? *source.pin : *target.pin;
                                    graphResult.outputPin = output.id;
                                    graphResult.inputPin = input.id;
                                    graphResult.connectionCommitted = true;
                                    graphResult.connectionAccepted = true;
                                    graphResult.phase = EditorTransactionPhase::Commit;
                                }
                            }

                            if(graphResult.connectionCommitted == false)
                            {
                                graphResult.phase = EditorTransactionPhase::Cancel;
                            }

                            ui->nodeGraphInteractions.erase(interactionIterator);
                        }
                    }
                }
            }
        }

        auto connectionIterator = ui->nodeGraphInteractions.find(surfaceResponse.response.id);

        if(connectionIterator != ui->nodeGraphInteractions.end() && connectionIterator->second.connecting == true)
        {
            Detail::GraphPinLocation source;

            if(Detail::findGraphPin(nodes, connectionIterator->second.pin, &source) == true)
            {
                Vec2 target = surfaceResponse.pointerWorld;
                float direction = source.pin->direction == NodePinDirection::Output ? 1.f : -1.f;
                float tangent = std::max(40.f, std::abs(target.x - source.position.x) * 0.5f);
                Vec2 firstControl = {source.position.x + direction * tangent, source.position.y};
                Vec2 secondControl = {target.x - direction * tangent, target.y};
                (void)canvasValue.bezierCubic(source.position, firstControl, secondControl, target, 2.f, source.pin->color);
                graphResult.pin = source.pin->id;
                graphResult.connectionPosition = target;
                graphResult.connectionPreview = true;
            }
        }

        if(options.showMinimap == true && nodes.empty() == false && surfaceResponse.bounds.empty() == false)
        {
            Rect worldBounds = nodes.front().bounds;

            for(size_t index = 1; index != nodes.size(); ++index)
            {
                const Rect & nodeBounds = nodes[index].bounds;
                float minimumX = std::min(worldBounds.x, nodeBounds.x);
                float minimumY = std::min(worldBounds.y, nodeBounds.y);
                float maximumX = std::max(worldBounds.right(), nodeBounds.right());
                float maximumY = std::max(worldBounds.bottom(), nodeBounds.bottom());
                worldBounds = {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
            }

            Vec2 minimapSize = options.minimapSize;
            minimapSize.x = std::clamp(minimapSize.x, 48.f, surfaceResponse.bounds.width);
            minimapSize.y = std::clamp(minimapSize.y, 36.f, surfaceResponse.bounds.height);
            Rect minimapBounds = {surfaceResponse.bounds.width - minimapSize.x - 8.f, surfaceResponse.bounds.height - minimapSize.y - 8.f, minimapSize.x, minimapSize.y};
            float worldWidth = std::max(1.f, worldBounds.width);
            float worldHeight = std::max(1.f, worldBounds.height);
            float minimapScale = std::min((minimapBounds.width - 8.f) / worldWidth, (minimapBounds.height - 8.f) / worldHeight);
            Vec2 minimapOrigin = {minimapBounds.x + 4.f - worldBounds.x * minimapScale, minimapBounds.y + 4.f - worldBounds.y * minimapScale};
            (void)canvasValue.popTransform();
            BoxStyle minimapStyle;
            minimapStyle.fill.color = ui->currentStyle->colors.popup;
            minimapStyle.borderWidth = 1.f;
            minimapStyle.borderColor = ui->currentStyle->colors.borderStrong;
            minimapStyle.radii = {4.f, 4.f, 4.f, 4.f};
            (void)canvasValue.box(minimapBounds, minimapStyle);

            for(const GraphNode & node : nodes)
            {
                Rect preview = {minimapOrigin.x + node.bounds.x * minimapScale, minimapOrigin.y + node.bounds.y * minimapScale, std::max(2.f, node.bounds.width * minimapScale), std::max(2.f, node.bounds.height * minimapScale)};
                Color previewColor = node.selected == true ? ui->currentStyle->colors.accent : ui->currentStyle->colors.header;
                (void)canvasValue.rect(preview, previewColor);
            }

            Detail::ItemBehaviorOptions minimapBehavior;
            minimapBehavior.pointerButton = PointerButton::Primary;
            Response minimapResponse = Detail::absoluteInteraction(ui, Key("Node graph minimap"), minimapBounds, minimapBehavior, true, location);

            if((minimapResponse.pressed() == true || minimapResponse.active() == true) && state != nullptr)
            {
                Vec2 pointer;

                if(canvasValue.localPointerPosition(&pointer) == true)
                {
                    Vec2 world;
                    world.x = worldBounds.x + (pointer.x - minimapBounds.x - 4.f) / minimapScale;
                    world.y = worldBounds.y + (pointer.y - minimapBounds.y - 4.f) / minimapScale;
                    state->pan.x = surfaceResponse.bounds.width * 0.5f - world.x * state->zoom;
                    state->pan.y = surfaceResponse.bounds.height * 0.5f - world.y * state->zoom;
                    graphResult.minimapInteracted = true;
                }
            }

            (void)canvasValue.pushTransform(surfaceResponse.worldTransform, StrokeScale::Screen);
        }

        (void)Mosaic::endDesignSurface(&canvasValue);

        if(_out != nullptr)
        {
            *_out = graphResult;
        }

        return surfaceResponse.response;
    }
    //////////////////////////////////////////////////////////////////////////
    void setEditorTransactionCallback(Context * ui, EditorTransactionCallback callback, void * userData) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->editorTransactionCallback = callback;
        ui->editorTransactionUserData = userData;
    }
    //////////////////////////////////////////////////////////////////////////
    bool emitEditorTransaction(Context * ui, const EditorTransaction & transaction)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->editorTransactionCallback == nullptr)
        {
            return false;
        }

        ui->editorTransactionCallback(transaction, ui->editorTransactionUserData);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool registerPropertyEditor(Context * ui, TypeId type, PropertyEditorCallback callback, void * userData)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(type == 0)
        {
            return false;
        }

        if(callback == nullptr)
        {
            return false;
        }

        Context::PropertyEditorRegistration registration;
        registration.callback = callback;
        registration.userData = userData;
        ui->propertyEditors[type] = registration;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void unregisterPropertyEditor(Context * ui, TypeId type) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->propertyEditors.erase(type);
    }
    //////////////////////////////////////////////////////////////////////////
    PropertyEditResponse propertyEditor(Context * ui, const Key & key, TypeId type, StringView label, void * value, const PropertyEditOptions & options, const SourceLocation & location)
    {
        PropertyEditResponse result;

        if(ui == nullptr)
        {
            return result;
        }

        auto propertyScope = Mosaic::scope(ui, key, location);
        LayoutOptions rowLayout;
        rowLayout.width = SizeRule::Fill;
        rowLayout.crossAxisAlignment = CrossAxisAlignment::Center;
        auto propertyRow = Mosaic::row(ui, Key("Property row"), rowLayout, location);
        TextOptions labelOptions;
        labelOptions.layout.width = SizeRule::Fill;
        (void)Mosaic::text(ui, label, labelOptions, location);
        auto valueScope = Mosaic::scope(ui, Key("Property value"), location);
        auto disabled = Mosaic::disabledScope(ui, options.readOnly, location);
        result.value = Mosaic::customProperty(ui, Key("Editor"), type, label, value, options, location);
        result.changed = result.value.changed();

        if(result.value.pressed() == true)
        {
            result.phase = EditorTransactionPhase::Begin;
        }

        if(result.value.active() == true)
        {
            result.phase = EditorTransactionPhase::Change;
        }

        if(result.value.committed() == true || result.value.released() == true)
        {
            result.phase = EditorTransactionPhase::Commit;
        }

        if(result.value.canceled() == true)
        {
            result.phase = EditorTransactionPhase::Cancel;
        }

        if(options.resettable == true)
        {
            auto resetEnabled = Mosaic::disabledScope(ui, options.readOnly, location);
            result.reset = Mosaic::smallButton(ui, Key("Reset property"), "Reset", location);
            result.resetRequested = result.reset.clicked();
        }

        if(options.resourceType != 0)
        {
            auto pickerEnabled = Mosaic::disabledScope(ui, options.readOnly, location);
            result.resourcePicker = Mosaic::smallButton(ui, Key("Pick resource"), "...", location);
            result.resourcePickerRequested = result.resourcePicker.clicked();
        }

        if(options.validationMessage.empty() == false && options.validation != Validation::Normal)
        {
            Vec2 tooltipSize = {420.f, 0.f};
            Mosaic::itemTooltip(ui, result.value, options.validationMessage, tooltipSize, location);
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    Response customProperty(Context * ui, const Key & key, TypeId type, StringView label, void * value, const PropertyEditOptions & options, const SourceLocation & location)
    {
        Response response;

        if(ui == nullptr)
        {
            return response;
        }

        auto propertyScope = Mosaic::scope(ui, key, location);
        auto iterator = ui->propertyEditors.find(type);

        if(iterator == ui->propertyEditors.end())
        {
            auto disabled = Mosaic::disabledScope(ui, true, location);
            response = Mosaic::button(ui, Key("Missing property editor"), "No property editor", {}, location);

            return response;
        }

        Context::PropertyEditorRegistration & registration = iterator->second;
        response = registration.callback(ui, label, value, options, registration.userData);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response customProperty(Context * ui, TypeId type, StringView label, void * value, const PropertyEditOptions & options, const SourceLocation & location)
    {
        Id callsite = combineId(hashBytes(location.file_name()), static_cast<Id>(location.line()));
        Response response = Mosaic::customProperty(ui, Key(callsite), type, label, value, options, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
