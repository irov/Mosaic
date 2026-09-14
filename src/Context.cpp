#include "Charconv.hpp"
#include "Context.hpp"
#include "ContextDetail.hpp"
#include "Interaction.hpp"
#include "Layout.hpp"
#include "NativeSurface.hpp"
#include "Popup.hpp"
#include "Render.hpp"
#include "TextEdit.hpp"
#include "Utility.hpp"
#include "Window.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

namespace Mosaic
{
    namespace Detail
    {
        [[nodiscard]] StringView drawCommandName(DrawCommandType type) noexcept
        {
            switch(type)
            {
            case DrawCommandType::Rect:
                return "Rect";
            case DrawCommandType::RectBatch:
                return "RectBatch";
            case DrawCommandType::QuadBatch:
                return "QuadBatch";
            case DrawCommandType::Box:
                return "Box";
            case DrawCommandType::RoundedRect:
                return "RoundedRect";
            case DrawCommandType::Gradient:
                return "Gradient";
            case DrawCommandType::Line:
                return "Line";
            case DrawCommandType::Polyline:
                return "Polyline";
            case DrawCommandType::Path:
                return "Path";
            case DrawCommandType::Image:
                return "Image";
            case DrawCommandType::CustomGeometry:
                return "CustomGeometry";
            case DrawCommandType::TextGeometry:
                return "TextGeometry";
            case DrawCommandType::NineSlice:
                return "NineSlice";
            case DrawCommandType::Grid:
                return "Grid";
            case DrawCommandType::PushTransform:
                return "PushTransform";
            case DrawCommandType::PopTransform:
                return "PopTransform";
            case DrawCommandType::PushClip:
                return "PushClip";
            case DrawCommandType::PopClip:
                return "PopClip";
            }

            return "Unknown";
        }
        //////////////////////////////////////////////////////////////////////////
        inline constexpr size_t MaximumTextCacheEntries = 4096;
        inline constexpr size_t MaximumTextCacheMemory = 32 * 1024 * 1024;
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool usesItemWidth(NodeKind kind) noexcept
        {
            return kind == NodeKind::Combo || kind == NodeKind::Slider || kind == NodeKind::DragValue || kind == NodeKind::InputText || kind == NodeKind::InputMultiline || kind == NodeKind::ColorEdit || kind == NodeKind::Progress;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Context::Node * findCanvasNode(Context * ui, Id canvas)
        {
            if(ui == nullptr)
            {
                return nullptr;
            }

            Context::Node * node = ui->findFrameNode(canvas);

            if(node == nullptr)
            {
                ui->frame.diagnostics.emplace_back("Canvas command target was not submitted in this frame");

                return nullptr;
            }

            if(node->kind != NodeKind::Canvas)
            {
                ui->frame.diagnostics.emplace_back("Canvas command target is not a canvas");

                return nullptr;
            }

            return node;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool itemPickerStructuralNode(NodeKind kind) noexcept
        {
            switch(kind)
            {
            case NodeKind::Root:
            case NodeKind::Scope:
            case NodeKind::Row:
            case NodeKind::Column:
            case NodeKind::Grid:
            case NodeKind::Overlay:
            case NodeKind::Absolute:
            case NodeKind::Clip:
            case NodeKind::Disabled:
            case NodeKind::Interaction:
            case NodeKind::Style:
            case NodeKind::Backdrop:
            case NodeKind::Spacer:
                return true;
            default:
                return false;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t cachedTextMemory(const Context::CachedText & text) noexcept
        {
            size_t result = sizeof(Context::CachedText) + text.text.capacity() * sizeof(char) + text.offsets.capacity() * sizeof(float) + text.clusters.capacity() * sizeof(size_t) + text.positions.capacity() * sizeof(Vec2) + text.lines.capacity() * sizeof(ShapedTextLine) + text.batches.capacity() * sizeof(Context::PreparedTextRunBatch);
            for(const Context::PreparedTextRunBatch & batch : text.batches)
            {
                result += batch.rectangles.capacity() * sizeof(TexturedRectInstance);
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t drawCommandStorageMemory(const DrawCommandStorage & storage) noexcept
        {
            size_t result = 0;
            result += storage.rectangles.capacity() * sizeof(RectInstance);
            result += storage.quads.capacity() * sizeof(QuadInstance);
            result += storage.points.capacity() * sizeof(Vec2);
            result += storage.coloredPoints.capacity() * sizeof(ColoredPoint);
            result += storage.colors.capacity() * sizeof(Color);
            result += storage.vertices.capacity() * sizeof(Vertex);
            result += storage.indices.capacity() * sizeof(uint32_t);

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t textLogTreeDepth(const Context * ui, size_t nodeIndex, size_t root) noexcept
        {
            size_t depth = 0;
            for(size_t parent = ui->nodes[nodeIndex].parent; parent != 0 && parent != root; parent = ui->nodes[parent].parent)
            {
                if(ui->nodes[parent].kind == Detail::NodeKind::Tree)
                {
                    ++depth;
                }
            }

            return depth;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool nodeDescendsFrom(const Context * ui, Id node, Id ancestor) noexcept
        {
            if(node == ancestor)
            {
                return true;
            }

            const Context::Node * current = ui->findFrameNode(node);
            while(current != nullptr && current->parent < ui->nodes.size())
            {
                const Context::Node & parent = ui->nodes[current->parent];

                if(parent.id == ancestor)
                {
                    return true;
                }

                if(parent.parent == current->parent)
                {
                    break;
                }

                current = &parent;
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Id nodeWindow(const Context * ui, Id node) noexcept
        {
            const Context::Node * value = ui->findFrameNode(node);

            if(value == nullptr)
            {
                return InvalidId;
            }

            return value->kind == Detail::NodeKind::Window ? value->id : value->windowOwner;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool dockHierarchyMatches(const Context * ui, Id first, Id second) noexcept
        {
            const Context::Node * firstWindow = ui->findFrameNode(Detail::nodeWindow(ui, first));
            const Context::Node * secondWindow = ui->findFrameNode(Detail::nodeWindow(ui, second));

            return firstWindow != nullptr && secondWindow != nullptr && firstWindow->windowData().dockNode != 0 && firstWindow->windowData().dockNode == secondWindow->windowData().dockNode && firstWindow->windowData().dockGroup == secondWindow->windowData().dockGroup;
        }
        //////////////////////////////////////////////////////////////////////////
        void appendTextLogNodes(Context * ui, size_t firstNode, size_t root, Id window, size_t maximumDepth, bool indent, String * output)
        {
            for(size_t index = firstNode; index != ui->nodes.size(); ++index)
            {
                const Context::Node & node = ui->nodes[index];

                if(node.visible == false)
                {
                    continue;
                }

                if(node.label.empty() == true)
                {
                    continue;
                }

                if((window != InvalidId && node.windowOwner != window))
                {
                    continue;
                }

                if(node.kind == Detail::NodeKind::Root)
                {
                    continue;
                }

                if(node.kind == Detail::NodeKind::Scope)
                {
                    continue;
                }

                if(node.kind == Detail::NodeKind::Row)
                {
                    continue;
                }

                if(node.kind == Detail::NodeKind::Column)
                {
                    continue;
                }

                if(node.kind == Detail::NodeKind::Grid)
                {
                    continue;
                }

                if(node.kind == Detail::NodeKind::Overlay)
                {
                    continue;
                }

                size_t depth = Detail::textLogTreeDepth(ui, index, root);

                if(depth > maximumDepth)
                {
                    continue;
                }

                if(indent == true)
                {
                    output->append(depth * 2, ' ');
                }

                output->append(node.label);

                if(node.valueText.empty() == false && node.valueText != node.label)
                {
                    output->append("\t");
                    output->append(node.valueText);
                }

                output->append("\n");
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void copyFocusedWindowText(Context * ui)
        {
            if(ui->configuration.windowCopyContentsWithPrimaryC == false)
            {
                return;
            }

            if(ui->input.modifiers.primary == false)
            {
                return;
            }

            if(ui->input.keyPressed(KeyCode::C) == false)
            {
                return;
            }

            const Context::Node * focusedNode = ui->findFrameNode(ui->focused);

            if(focusedNode != nullptr)
            {
                bool textEditor = focusedNode->kind == Detail::NodeKind::InputText;

                if(focusedNode->kind == Detail::NodeKind::InputMultiline)
                {
                    textEditor = true;
                }

                if(textEditor == true)
                {
                    if(ui->state(focusedNode->id).editing == true)
                    {
                        return;
                    }
                }
            }

            Id window = focusedNode == nullptr ? ui->pointerWindow : focusedNode->windowOwner;

            if(window == InvalidId)
            {
                window = ui->pointerWindow;
            }

            if(window == InvalidId)
            {
                return;
            }

            String output;
            Detail::appendTextLogNodes(ui, 1, 0, window, std::numeric_limits<size_t>::max(), true, &output);

            if(output.empty() == false)
            {
                ui->platform->setClipboardText(output);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void finishTextLog(Context * ui)
        {
            if(ui->textLogEnabled == false)
            {
                return;
            }

            Detail::appendTextLogNodes(ui, ui->textLogStartNode, ui->textLogRootNode, ui->textLogWindow, ui->textLogMaximumDepth, ui->textLogIndent, &ui->textLogBuffer);

            if(ui->textLogTarget == TextLogTarget::Clipboard)
            {
                ui->platform->setClipboardText(ui->textLogBuffer);
            }
            else if(ui->textLogTarget == TextLogTarget::Terminal)
            {
                bool written = ui->platform->writeConsole(ui->textLogBuffer);

                if(written == false)
                {
                    ui->frame.diagnostics.emplace_back("Platform backend could not write the text log to the console");
                }
            }
            else
            {
                StringView filename = ui->textLogFilename.empty() == true ? StringView{"mosaic_log.txt"} : StringView{ui->textLogFilename};
                const std::byte * data = reinterpret_cast<const std::byte *>(ui->textLogBuffer.data());
                ByteSpan bytes(data, ui->textLogBuffer.size());
                bool written = ui->platform->writeFile(filename, bytes);

                if(written == false)
                {
                    ui->frame.diagnostics.emplace_back("Platform backend could not write the text log file");
                }
            }

            ui->textLogEnabled = false;
            ui->textLogBuffer.clear();
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool isComposableNumber(StringView value, const Context::TextCacheAttributes & attributes) noexcept
        {
            if(attributes.editable == true)
            {
                return false;
            }

            if(attributes.wordWrap == true)
            {
                return false;
            }

            if(attributes.font != MonospaceFont)
            {
                return false;
            }

            if(value.size() < 2)
            {
                return false;
            }

            for(char character : value)
            {
                bool accepted = character >= '0' && character <= '9';
                switch(character)
                {
                case '+':
                case '-':
                case '.':
                case 'e':
                case 'E':
                case '%':
                case ' ':
                {
                    accepted = true;
                    break;
                }
                default:
                {
                    break;
                }
                }

                if(accepted == false)
                {
                    return false;
                }
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        void resetTransientText(Context::CachedText & text)
        {
            text.attributes = {};
            text.text.clear();
            text.size = {};
            text.offsets.clear();
            text.clusters.clear();
            text.positions.clear();
            text.lines.clear();
            text.batches.clear();
            text.key = 0;
            text.lastFrame = 0;
            text.previous = nullptr;
            text.next = nullptr;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] uint32_t quantizeFontSize(float size) noexcept
        {
            auto returnedValue = static_cast<uint32_t>(std::max(1.f, std::round(size * 64.f)));

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Rect unionBounds(const Rect & left, const Rect & right) noexcept
        {
            if(left.empty() == true)
            {
                return right;
            }

            if(right.empty() == true)
            {
                return left;
            }

            float minimumX = std::min(left.x, right.x);
            float minimumY = std::min(left.y, right.y);
            float maximumX = std::max(left.right(), right.right());
            float maximumY = std::max(left.bottom(), right.bottom());
            Rect result = {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        void addListClipperSpacer(Context * ui, float extent, Orientation orientation, const SourceLocation & location)
        {
            LayoutOptions layout;

            if(orientation == Orientation::Horizontal)
            {
                layout.width = Dimension::fixed(std::max(0.f, extent));
            }
            else
            {
                layout.height = Dimension::fixed(std::max(0.f, extent));
            }

            (void)ui->addNode(Detail::NodeKind::Spacer, {}, {}, layout, location, SemanticRole::None, false, true);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] const char * nodeKindName(Detail::NodeKind kind) noexcept
        {
            switch(kind)
            {
            case Detail::NodeKind::Root:
                return "Root";
            case Detail::NodeKind::Scope:
                return "Scope";
            case Detail::NodeKind::Window:
                return "Window";
            case Detail::NodeKind::Row:
                return "Row";
            case Detail::NodeKind::Column:
                return "Column";
            case Detail::NodeKind::Grid:
                return "Grid";
            case Detail::NodeKind::Overlay:
                return "Overlay";
            case Detail::NodeKind::Scroll:
                return "Scroll";
            case Detail::NodeKind::Split:
                return "Split";
            case Detail::NodeKind::Absolute:
                return "Absolute";
            case Detail::NodeKind::Clip:
                return "Clip";
            case Detail::NodeKind::Disabled:
                return "Disabled";
            case Detail::NodeKind::Interaction:
                return "Interaction";
            case Detail::NodeKind::Style:
                return "Style";
            case Detail::NodeKind::Backdrop:
                return "Backdrop";
            case Detail::NodeKind::Text:
                return "Text";
            case Detail::NodeKind::Bullet:
                return "Bullet";
            case Detail::NodeKind::BulletText:
                return "BulletText";
            case Detail::NodeKind::Button:
                return "Button";
            case Detail::NodeKind::IconButton:
                return "IconButton";
            case Detail::NodeKind::Checkbox:
                return "Checkbox";
            case Detail::NodeKind::Radio:
                return "RadioButton";
            case Detail::NodeKind::Selectable:
                return "Selectable";
            case Detail::NodeKind::Tab:
                return "Tab";
            case Detail::NodeKind::Toggle:
                return "Toggle";
            case Detail::NodeKind::Combo:
                return "Combo";
            case Detail::NodeKind::Slider:
                return "Slider";
            case Detail::NodeKind::DragValue:
                return "DragValue";
            case Detail::NodeKind::Progress:
                return "ProgressBar";
            case Detail::NodeKind::Separator:
                return "Separator";
            case Detail::NodeKind::SeparatorText:
                return "SeparatorText";
            case Detail::NodeKind::Spacer:
                return "Spacer";
            case Detail::NodeKind::Image:
                return "Image";
            case Detail::NodeKind::ImageButton:
                return "ImageButton";
            case Detail::NodeKind::InputText:
                return "InputText";
            case Detail::NodeKind::InputMultiline:
                return "InputMultiline";
            case Detail::NodeKind::ColorEdit:
                return "ColorEdit";
            case Detail::NodeKind::Tree:
                return "TreeItem";
            case Detail::NodeKind::Table:
                return "Table";
            case Detail::NodeKind::TableRow:
                return "TableRow";
            case Detail::NodeKind::TableCell:
                return "TableCell";
            case Detail::NodeKind::Canvas:
                return "Canvas";
            case Detail::NodeKind::NativeSurface:
                return "NativeSurface";
            }

            return "Unknown";
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool usesAncestorVisualClip(Detail::NodeKind kind) noexcept
        {
            switch(kind)
            {
            case Detail::NodeKind::Window:
            case Detail::NodeKind::Button:
            case Detail::NodeKind::IconButton:
            case Detail::NodeKind::Checkbox:
            case Detail::NodeKind::Radio:
            case Detail::NodeKind::Selectable:
            case Detail::NodeKind::Tab:
            case Detail::NodeKind::Toggle:
            case Detail::NodeKind::Combo:
            case Detail::NodeKind::Slider:
            case Detail::NodeKind::DragValue:
            case Detail::NodeKind::Progress:
            case Detail::NodeKind::ImageButton:
            case Detail::NodeKind::InputText:
            case Detail::NodeKind::InputMultiline:
            case Detail::NodeKind::ColorEdit:
            case Detail::NodeKind::Tree:
                return true;
            default:
                return false;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Rect expandedTextPreparationBounds(const Rect & bounds, float margin) noexcept
        {
            return {bounds.x - margin, bounds.y - margin, bounds.width + margin * 2.f, bounds.height + margin * 2.f};
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool shouldPrepareNodeText(const Context * ui, size_t index) noexcept
        {
            const Context::Node & node = ui->nodes[index];

            if(node.visible == false)
            {
                return false;
            }

            if(node.id == ui->active)
            {
                return true;
            }

            if(node.id == ui->captured)
            {
                return true;
            }

            if(node.id == ui->focused)
            {
                return true;
            }

            if(node.bounds.empty() == true)
            {
                return false;
            }

            float margin = std::max(32.f, node.style->metrics.lineHeight * 3.f);
            Rect visibleBounds = Detail::expandedTextPreparationBounds(ui->viewport.bounds, margin);
            for(size_t ancestorIndex = node.parent; ancestorIndex != 0; ancestorIndex = ui->nodes[ancestorIndex].parent)
            {
                const Context::Node & ancestor = ui->nodes[ancestorIndex];
                bool constrainsText = ancestor.kind == Detail::NodeKind::Window;

                if(ancestor.kind == Detail::NodeKind::Scroll)
                {
                    constrainsText = true;
                }

                if(ancestor.kind == Detail::NodeKind::Table)
                {
                    constrainsText = true;
                }

                if(ancestor.kind == Detail::NodeKind::Clip)
                {
                    constrainsText = true;
                }

                if(constrainsText == false)
                {
                    continue;
                }

                Rect constraint = ancestor.childrenClip;

                if(constraint.empty() == true)
                {
                    return false;
                }

                visibleBounds = Rect::intersection(visibleBounds, Detail::expandedTextPreparationBounds(constraint, margin));

                if(visibleBounds.empty() == true)
                {
                    return false;
                }
            }
            auto returnedValue = Rect::intersection(node.bounds, visibleBounds).empty() == false;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Rect constrainWindowBounds(const Rect & bounds, const Rect & available) noexcept
        {
            if(available.empty() == true)
            {
                return bounds;
            }

            Rect constrained = bounds;
            constrained.width = std::min(std::max(0.f, constrained.width), available.width);
            constrained.height = std::min(std::max(0.f, constrained.height), available.height);
            constrained.x = std::clamp(constrained.x, available.x, std::max(available.x, available.right() - constrained.width));
            constrained.y = std::clamp(constrained.y, available.y, std::max(available.y, available.bottom() - constrained.height));

            return constrained;
        }
        //////////////////////////////////////////////////////////////////////////
        DockModel * dockModel(Context * ui, uint32_t group, bool create) noexcept
        {
            if(group == 0)
            {
                return nullptr;
            }

            if(group == 1)
            {
                return &ui->docking;
            }

            auto found = ui->dockModels.find(group);

            if(found != ui->dockModels.end())
            {
                return &found->second;
            }

            if(create == false)
            {
                return nullptr;
            }

            auto returnedValue = &ui->dockModels.try_emplace(group).first->second;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        const DockModel * dockModel(const Context * ui, uint32_t group) noexcept
        {
            if(group == 0)
            {
                return nullptr;
            }

            if(group == 1)
            {
                return &ui->docking;
            }

            auto found = ui->dockModels.find(group);
            auto returnedValue = found == ui->dockModels.end() ? nullptr : &found->second;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        const DockSpaceOptions * dockSpaceOptions(const Context * ui, uint32_t group) noexcept
        {
            auto found = ui->dockSpaceOptions.find(group);
            auto returnedValue = found == ui->dockSpaceOptions.end() ? nullptr : &found->second;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        void setDockArea(Context * ui, uint32_t group, const Rect & bounds) noexcept
        {
            if(group == 0)
            {
                return;
            }

            if(group == 1)
            {
                ui->dockArea = bounds;

                return;
            }

            ui->dockAreas[group] = bounds;
        }
        //////////////////////////////////////////////////////////////////////////
        bool dockArea(const Context * ui, uint32_t group, Rect * const _out) noexcept
        {
            if(ui == nullptr)
            {
                return false;
            }

            if(_out == nullptr)
            {
                return false;
            }

            if(group == 1)
            {

                *_out = ui->dockArea;

                return true;
            }

            auto found = ui->dockAreas.find(group);

            if(found == ui->dockAreas.end())
            {
                return false;
            }

            *_out = found->second;

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float snapToPixel(float value, float scale) noexcept
        {
            float resolvedScale = std::max(1.f, scale);
            auto returnedValue = std::round(value * resolvedScale) / resolvedScale;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Color mixColor(const Color & from, const Color & to, float amount) noexcept
        {
            float ratio = std::clamp(amount, 0.f, 1.f);
            Color result = {from.r + (to.r - from.r) * ratio, from.g + (to.g - from.g) * ratio, from.b + (to.b - from.b) * ratio, from.a + (to.a - from.a) * ratio};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Color colorWithAlpha(const Color & color, float alpha) noexcept
        {
            float resolvedAlpha = std::clamp(alpha, 0.f, 1.f);
            Color result = {color.r, color.g, color.b, color.a * resolvedAlpha};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Vec2 mixVector(const Vec2 & from, const Vec2 & to, float amount) noexcept
        {
            float ratio = std::clamp(amount, 0.f, 1.f);
            Vec2 result = {from.x + (to.x - from.x) * ratio, from.y + (to.y - from.y) * ratio};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float itemLabelWidth(const Context::Node & node) noexcept
        {
            if(node.label.empty() == true)
            {
                return 0.f;
            }

            if(node.labelPlacement == LabelPlacement::Hidden)
            {
                return 0.f;
            }

            switch(node.kind)
            {
            case NodeKind::Combo:
                return node.textSize.x + node.style->metrics.innerSpacing.x;
            case NodeKind::Slider:
            case NodeKind::ColorEdit:
                return node.textSize.x + node.style->metrics.gap;
            default:
                return 0.f;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] SliderGeometry sliderGeometry(const Context::Node & node, const Rect & bounds) noexcept
        {
            SliderGeometry geometry;
            float labelWidth = std::min(bounds.width, Detail::itemLabelWidth(node));

            if(node.labelPlacement == LabelPlacement::After)
            {
                geometry.control = {bounds.x, bounds.y, std::max(0.f, bounds.width - labelWidth), bounds.height};
                geometry.label = {geometry.control.right() + node.style->metrics.gap, bounds.y, std::max(0.f, labelWidth - node.style->metrics.gap), bounds.height};
            }
            else
            {
                geometry.control = {bounds.x + labelWidth, bounds.y, std::max(0.f, bounds.width - labelWidth), bounds.height};
                geometry.label = {bounds.x, bounds.y, std::max(0.f, labelWidth - node.style->metrics.gap), bounds.height};
            }

            // Like Mosaic's slider_usable_pos_min/max, the value range describes the
            // grab center, not the raw frame edges. At 0 and 1 the whole circular grab stays
            // inside the control instead of extending beyond it or colliding with the label.
            float grabRadius = std::min(geometry.maximumGrabSide * 0.5f, geometry.control.width * 0.5f);
            float trackHeight = 3.f;
            geometry.track = {geometry.control.x + grabRadius, bounds.y + (bounds.height - trackHeight) * 0.5f, std::max(0.f, geometry.control.width - grabRadius * 2.f), trackHeight};

            return geometry;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ComboGeometry comboGeometry(const Context::Node & node, const Rect & bounds) noexcept
        {
            ComboGeometry geometry;
            float labelWidth = std::min(bounds.width, Detail::itemLabelWidth(node));

            if(node.labelPlacement == LabelPlacement::Before)
            {
                geometry.label = {bounds.x, bounds.y, std::max(0.f, labelWidth - node.style->metrics.innerSpacing.x), bounds.height};
                geometry.control = {bounds.x + labelWidth, bounds.y, std::max(0.f, bounds.width - labelWidth), bounds.height};
            }
            else
            {
                geometry.control = {bounds.x, bounds.y, std::max(0.f, bounds.width - labelWidth), bounds.height};
                geometry.label = {geometry.control.right() + node.style->metrics.innerSpacing.x, bounds.y, std::max(0.f, labelWidth - node.style->metrics.innerSpacing.x), bounds.height};
            }

            float arrowWidth = node.comboShowArrow ? std::min(geometry.control.width, node.style->metrics.controlHeight) : 0.f;
            geometry.arrow = {geometry.control.right() - arrowWidth, geometry.control.y, arrowWidth, geometry.control.height};
            geometry.preview = node.comboShowPreview ? Rect{geometry.control.x, geometry.control.y, std::max(0.f, geometry.control.width - arrowWidth), geometry.control.height} : Rect{};

            return geometry;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ColorEditGeometry colorEditGeometry(const Context::Node & node, const Rect & bounds) noexcept
        {
            ColorEditGeometry geometry;
            float labelWidth = std::min(bounds.width, Detail::itemLabelWidth(node));

            if(node.labelPlacement == LabelPlacement::After)
            {
                geometry.control = {bounds.x, bounds.y, std::max(0.f, bounds.width - labelWidth), bounds.height};
                geometry.label = {geometry.control.right() + node.style->metrics.gap, bounds.y, std::max(0.f, labelWidth - node.style->metrics.gap), bounds.height};
            }
            else
            {
                geometry.control = {bounds.x + labelWidth, bounds.y, std::max(0.f, bounds.width - labelWidth), bounds.height};
                geometry.label = {bounds.x, bounds.y, std::max(0.f, labelWidth - node.style->metrics.gap), bounds.height};
            }

            float innerGap = 3.f;
            // with the inputs hidden the swatch is the whole control, so a caller that gives
            // the editor a wider size gets a wider swatch rather than a gap beside it
            float previewWidth = node.colorShowPreview ? (node.colorShowInputs ? std::min(geometry.control.height, geometry.control.width) : geometry.control.width) : 0.f;
            bool previewOnLeft = node.style->metrics.colorButtonPosition == ColorButtonPosition::Left;
            geometry.preview = {previewOnLeft ? geometry.control.x : geometry.control.right() - previewWidth, geometry.control.y, previewWidth, geometry.control.height};
            geometry.channelCount = node.colorShowInputs ? node.colorComponents : 0;

            if(geometry.channelCount != 0)
            {
                float inputWidth = std::max(0.f, geometry.control.width - (previewWidth > 0.f ? previewWidth + innerGap : 0.f));
                float totalGap = innerGap * static_cast<float>(geometry.channelCount - 1U);
                float channelWidth = std::max(1.f, (inputWidth - totalGap) / static_cast<float>(geometry.channelCount));
                float x = geometry.control.x;

                if(previewOnLeft == true && previewWidth > 0.f)
                {
                    x += previewWidth + innerGap;
                }

                for(uint8_t channel = 0; channel != geometry.channelCount; ++channel)
                {
                    geometry.channels[channel] = {x, geometry.control.y, channelWidth, geometry.control.height};
                    x += channelWidth + innerGap;
                }
            }

            return geometry;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float scrollbarScrollAtPointer(const Context::Persistent & state, Orientation orientation, const Vec2 & position) noexcept
        {
            bool vertical = orientation == Orientation::Vertical;
            float trackStart = vertical ? state.scrollData().trackBounds.y : state.scrollData().trackBounds.x;
            float trackExtent = vertical ? state.scrollData().trackBounds.height : state.scrollData().trackBounds.width;
            float thumbExtent = vertical ? state.scrollData().thumbBounds.height : state.scrollData().thumbBounds.width;
            float pointerPosition = vertical ? position.y : position.x;
            float travel = std::max(0.f, trackExtent - thumbExtent);

            if(travel <= 0.f)
            {
                return 0.f;
            }

            if(state.scrollData().extent <= 0.f)
            {
                return 0.f;
            }

            float ratio = std::clamp((pointerPosition - trackStart - state.scrollData().dragOffset) / travel, 0.f, 1.f);

            return ratio * state.scrollData().extent;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float scrollbarScrollAtPointer(const Rect & track, const Rect & thumb, float scrollExtent, bool vertical, float dragOffset, const Vec2 & position) noexcept
        {
            float trackStart = vertical ? track.y : track.x;
            float trackExtent = vertical ? track.height : track.width;
            float thumbExtent = vertical ? thumb.height : thumb.width;
            float pointerPosition = vertical ? position.y : position.x;
            float travel = std::max(0.f, trackExtent - thumbExtent);

            if(travel <= 0.f)
            {
                return 0.f;
            }

            if(scrollExtent <= 0.f)
            {
                return 0.f;
            }

            float ratio = std::clamp((pointerPosition - trackStart - dragOffset) / travel, 0.f, 1.f);

            return ratio * scrollExtent;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float splitterCenter(const Rect & bounds, Orientation orientation) noexcept
        {
            return orientation == Orientation::Horizontal ? bounds.x + bounds.width * 0.5f : bounds.y + bounds.height * 0.5f;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float snapSplitterCenter(const Context * ui, Id splitter, Orientation orientation, uint32_t snapIndex, float center, float distance) noexcept
        {
            if(snapIndex == 0)
            {
                return center;
            }

            float snapped = center;
            float nearest = distance;
            for(const auto & [candidateId, candidate] : ui->persistent)
            {
                if(candidateId == splitter)
                {
                    continue;
                }

                if(candidate.splitterSnapIndex != snapIndex)
                {
                    continue;
                }

                if(candidate.splitterOrientation != orientation)
                {
                    continue;
                }

                if(candidate.splitterBounds.empty() == true)
                {
                    continue;
                }

                if(candidate.lastFrame + 1 < ui->frame.number)
                {
                    continue;
                }

                float candidateCenter = Detail::splitterCenter(candidate.splitterBounds, orientation);
                float separation = std::abs(candidateCenter - center);

                if(separation <= nearest)
                {
                    nearest = separation;
                    snapped = candidateCenter;
                }
            }

            return snapped;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool inputLayerBlocked(const Context * ui, const Context::Node & node) noexcept
        {
            Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;
            auto returnedValue = blockingLayer != InvalidId && node.inputLayer != blockingLayer && Detail::popupOwnerCanInteract(ui, node.id) == false;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        void arrangeFrame(Context * ui, const Rect & rootBounds)
        {
            ui->layoutChildren.clear();
            ui->layoutFloatScratch.clear();
            ui->scrollLayoutNodes.clear();

            if(ui->layoutChildren.capacity() < ui->nodes.size())
            {
                ui->layoutChildren.reserve(ui->nodes.size());
            }

            if(ui->scrollLayoutNodes.capacity() < ui->nodes.size())
            {
                ui->scrollLayoutNodes.reserve(ui->nodes.size());
            }

            for(auto & [group, windows] : ui->visibleDockWindowsScratch)
            {
                (void)group;
                windows.clear();
            }
            for(auto & [group, windows] : ui->collapsedDockWindowsScratch)
            {
                (void)group;
                windows.clear();
            }
            for(Context::Node & candidate : ui->nodes)
            {
                if(candidate.kind != Detail::NodeKind::Window)
                {
                    continue;
                }

                if(candidate.mutableWindowData().dockGroup == 0)
                {
                    continue;
                }

                DockModel * model = Detail::dockModel(ui, candidate.mutableWindowData().dockGroup, false);

                if(model == nullptr)
                {
                    continue;
                }

                if(model->nodeForWindow(candidate.id) == 0)
                {
                    continue;
                }

                candidate.mutableWindowData().docked = true;
                candidate.mutableWindowData().dockNode = model->nodeForWindow(candidate.id);

                if(candidate.visible == true)
                {
                    ui->visibleDockWindowsScratch[candidate.mutableWindowData().dockGroup].push_back(candidate.id);

                    if(candidate.mutableWindowData().collapsed == true)
                    {
                        ui->collapsedDockWindowsScratch[candidate.mutableWindowData().dockGroup].push_back(candidate.id);
                    }
                }
            }
            ui->dockLayout.clear();
            ui->dockSplitters.clear();
            ui->dockLayoutIndices.clear();
            for(const auto & [group, visibleWindows] : ui->visibleDockWindowsScratch)
            {
                DockModel * model = Detail::dockModel(ui, group, false);

                if(model == nullptr)
                {
                    continue;
                }

                if(visibleWindows.empty() == true)
                {
                    continue;
                }

                Rect configuredArea;
                bool hasConfiguredArea = Detail::dockArea(ui, group, &configuredArea);
                Rect resolvedDockArea = hasConfiguredArea == false || configuredArea.empty() == true ? rootBounds : configuredArea;
                auto collapsed = ui->collapsedDockWindowsScratch.find(group);
                IdSpan collapsedWindows = collapsed == ui->collapsedDockWindowsScratch.end() ? IdSpan{} : IdSpan(collapsed->second);
                size_t firstWindow = ui->dockLayout.size();
                size_t firstSplitter = ui->dockSplitters.size();
                model->layout(resolvedDockArea, visibleWindows, collapsedWindows, ui->theme.metrics.windowTitleHeight, ui->theme.metrics.splitterWidth, ui->dockLayout, ui->dockSplitters);
                for(size_t index = firstWindow; index != ui->dockLayout.size(); ++index)
                {
                    DockLayoutEntry & entry = ui->dockLayout[index];
                    entry.group = group;
                    ui->dockLayoutIndices[entry.window] = index;
                }
                for(size_t index = firstSplitter; index != ui->dockSplitters.size(); ++index)
                {
                    DockSplitterLayoutEntry & splitter = ui->dockSplitters[index];
                    splitter.group = group;
                }
                for(Context::Node & candidate : ui->nodes)
                {
                    if(candidate.kind != Detail::NodeKind::Window)
                    {
                        continue;
                    }

                    if(candidate.mutableWindowData().dockNode == 0)
                    {
                        continue;
                    }

                    if(candidate.mutableWindowData().dockGroup != group)
                    {
                        continue;
                    }

                    auto entry = ui->dockLayoutIndices.find(candidate.id);
                    candidate.visible = entry != ui->dockLayoutIndices.end() && ui->dockLayout[entry->second].group == group && ui->dockLayout[entry->second].active;
                }
            }
            ui->arrangeNode(0, rootBounds, ui->viewport.bounds);
            for(const DockLayoutEntry & entry : ui->dockLayout)
            {
                if(entry.active == false)
                {
                    continue;
                }

                Context::Node * window = ui->findFrameNode(entry.window);

                if(window == nullptr)
                {
                    continue;
                }

                if(window->kind != Detail::NodeKind::Window)
                {
                    continue;
                }

                size_t index = static_cast<size_t>(window - ui->nodes.data());
                Rect dockedBounds = entry.bounds;

                if(window->windowData().collapsed == true)
                {
                    dockedBounds.height = window->style->metrics.windowTitleHeight;
                }

                ui->arrangeNode(index, dockedBounds, ui->viewport.bounds);
            }
            for(size_t child = ui->nodes[0].firstChild; child != std::numeric_limits<size_t>::max(); child = ui->nodes[child].nextSibling)
            {
                Context::Node & node = ui->nodes[child];

                if(node.kind == Detail::NodeKind::Window && node.windowData().dockNode == 0)
                {
                    Context::Persistent & persistentState = ui->state(node);

                    if(persistentState.windowData().initialized == false)
                    {
                        persistentState.windowData().bounds = node.bounds;
                        persistentState.windowData().initialized = true;
                    }

                    Rect windowBounds = persistentState.windowData().bounds;

                    if(node.windowData().autoSize == true)
                    {
                        Vec2 popupSize = {std::clamp(node.measured.x, node.windowData().minimumSize.x, node.windowData().maximumSize.x), std::clamp(node.measured.y, node.windowData().minimumSize.y, node.windowData().maximumSize.y)};
                        Rect popupArea = ui->viewport.workArea.empty() == true ? ui->viewport.bounds : ui->viewport.workArea;
                        float horizontalPadding = std::clamp(node.style->metrics.displaySafeAreaPadding.x, 0.f, popupArea.width * 0.5f);
                        float verticalPadding = std::clamp(node.style->metrics.displaySafeAreaPadding.y, 0.f, popupArea.height * 0.5f);
                        popupArea.x += horizontalPadding;
                        popupArea.y += verticalPadding;
                        popupArea.width = std::max(0.f, popupArea.width - horizontalPadding * 2.f);
                        popupArea.height = std::max(0.f, popupArea.height - verticalPadding * 2.f);
                        windowBounds = Detail::placePopup(node, popupSize, popupArea);
                        persistentState.windowData().bounds = windowBounds;
                    }
                    else
                    {
                        bool fitContent = node.windowData().alwaysAutoResize;

                        if(node.windowData().alwaysAutoResize == false && node.windowData().fitContentWidth == true)
                        {
                            if(persistentState.windowData().contentWidthFitted == false)
                            {
                                fitContent = true;
                            }
                        }

                        if(node.windowData().alwaysAutoResize == false && node.windowData().fitContentHeight == true)
                        {
                            if(persistentState.windowData().contentHeightFitted == false)
                            {
                                fitContent = true;
                            }
                        }

                        if(fitContent == true)
                        {
                            bool resizeWidth = node.windowData().fitContentWidth == true && (node.windowData().alwaysAutoResize == true || persistentState.windowData().contentWidthFitted == false);

                            if(resizeWidth == true)
                            {
                                float measuredWidth = std::clamp(node.measured.x, node.windowData().minimumSize.x, node.windowData().maximumSize.x);
                                windowBounds.width = node.windowData().alwaysAutoResize == true ? measuredWidth : std::max(windowBounds.width, measuredWidth);
                                persistentState.windowData().contentWidthFitted = true;
                            }

                            bool resizeHeight = node.windowData().fitContentHeight == true && (node.windowData().alwaysAutoResize == true || persistentState.windowData().contentHeightFitted == false);

                            if(resizeHeight == true)
                            {
                                float measuredHeight = std::clamp(node.measured.y, node.windowData().minimumSize.y, node.windowData().maximumSize.y);
                                windowBounds.height = node.windowData().alwaysAutoResize == true ? measuredHeight : std::max(windowBounds.height, measuredHeight);
                                persistentState.windowData().contentHeightFitted = true;
                            }

                            windowBounds = Detail::constrainWindowBounds(windowBounds, ui->viewport.workArea.empty() == true ? ui->viewport.bounds : ui->viewport.workArea);
                            persistentState.windowData().bounds = windowBounds;
                        }
                    }

                    if(node.windowData().collapsed == true)
                    {
                        windowBounds.height = node.style->metrics.windowTitleHeight;
                    }

                    ui->arrangeNode(child, windowBounds, ui->viewport.bounds);
                }
                else if(node.kind == Detail::NodeKind::Backdrop)
                {
                    ui->arrangeNode(child, rootBounds, ui->viewport.bounds);
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void markScrollLayout(Context * ui, size_t index)
        {
            if(index >= ui->nodes.size())
            {
                return;
            }

            if(ui->nodes[index].scrollLayoutDirty)
            {
                return;
            }

            ui->nodes[index].scrollLayoutDirty = true;
            ui->scrollLayoutNodes.push_back(index);
        }
        //////////////////////////////////////////////////////////////////////////
        void arrangeScrollChanges(Context * ui)
        {
            if(ui->scrollLayoutNodes.empty() == true)
            {
                return;
            }

            ui->layoutChildren.clear();

            if(ui->layoutChildren.capacity() < ui->nodes.size())
            {
                ui->layoutChildren.reserve(ui->nodes.size());
            }

            for(size_t index : ui->scrollLayoutNodes)
            {
                bool ancestorChanged = false;
                for(size_t parent = ui->nodes[index].parent; parent != 0; parent = ui->nodes[parent].parent)
                {
                    if(ui->nodes[parent].scrollLayoutDirty)
                    {
                        ancestorChanged = true;
                        break;
                    }
                }

                if(ancestorChanged == true)
                {
                    continue;
                }

                size_t parent = ui->nodes[index].parent;
                Rect inheritedClip = parent < ui->nodes.size() ? ui->nodes[parent].childrenClip : ui->viewport.bounds;
                ui->arrangeNode(index, ui->nodes[index].bounds, inheritedClip);
            }
            for(size_t index : ui->scrollLayoutNodes)
            {
                ui->nodes[index].scrollLayoutDirty = false;
            }
            ui->scrollLayoutNodes.clear();
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool applyWheelScroll(Context * ui)
        {
            const PointerState * pointer = ui->input.primaryPointer();

            if(ui->input.wheel.x == 0.f && ui->input.wheel.y == 0.f)
            {
                return false;
            }

            if(pointer == nullptr)
            {
                return false;
            }

            if(ui->captured != InvalidId && pointer->isDown() == true)
            {
                return false;
            }

            auto applyToNode = [ui, pointer](size_t nodeIndex)
            {
                Context::Node & node = ui->nodes[nodeIndex];
                bool multilineText = node.kind == Detail::NodeKind::InputMultiline;
                bool scrollContainer = Detail::scrollsContent(node);
                bool scrollTable = node.kind == Detail::NodeKind::Table && (node.tableOptions().scrollHorizontal || node.tableOptions().scrollVertical == true);

                if(multilineText == false && scrollContainer == false && scrollTable == false)
                {
                    return false;
                }

                Context::Persistent & persistentState = ui->state(node);

                if(node.visible == false)
                {
                    return false;
                }

                if(node.disabled == true)
                {
                    return false;
                }

                if(node.inputBlocked == true)
                {
                    return false;
                }

                if(Detail::inputLayerBlocked(ui, node) == true)
                {
                    return false;
                }

                if(node.bounds.empty() == true)
                {
                    return false;
                }

                if(node.clip.empty() == true)
                {
                    return false;
                }

                if(node.bounds.contains(pointer->position) == false)
                {
                    return false;
                }

                if(node.clip.contains(pointer->position) == false)
                {
                    return false;
                }

                // Keep the whole wheel/trackpad gesture routed to the first
                // hovered scroll surface. A nested list therefore owns the
                // gesture even after it reaches an edge instead of leaking
                // subsequent deltas into its parent window.
                ui->wheelOwner = node.id;
                ui->wheelOwnerTimestamp = ui->input.timestamp;

                if(multilineText == true)
                {
                    Detail::TextEditorState & editorState = ui->textEditorState(node.id);
                    float visibleHeight = std::max(0.f, node.bounds.height - node.style->metrics.padding - node.style->metrics.frameBorderSize * 2.f);
                    float maximumY = std::max(0.f, node.textSize.y - visibleHeight);
                    float updated = std::clamp(editorState.textScrollY - ui->input.wheel.y, 0.f, maximumY);

                    if(updated != editorState.textScrollY)
                    {
                        editorState.textScrollY = updated;
                        node.textEditData().scrollY = updated;
                        ui->frame.events.push_back({EventType::Change, node.id, ui->nodePath(node), node.debugData().file, node.debugData().line, ui->input.timestamp});
                    }

                    // Multiline text is an independent scroll surface. Reaching an
                    // edge must not leak this wheel gesture to an enclosing window.
                    return true;
                }

                bool horizontal = scrollTable ? node.tableOptions().scrollHorizontal : node.scrollOptions().axes == ScrollAxes::Horizontal || node.scrollOptions().axes == ScrollAxes::Both;
                bool vertical = scrollTable ? node.tableOptions().scrollVertical : node.scrollOptions().axes == ScrollAxes::Vertical || node.scrollOptions().axes == ScrollAxes::Both;
                float deltaX = horizontal ? ui->input.wheel.x * node.scrollOptions().wheelStep : 0.f;
                float deltaY = vertical ? ui->input.wheel.y * node.scrollOptions().wheelStep : 0.f;

                if(horizontal == true && deltaX == 0.f && (vertical == false || ui->input.modifiers.shift == true))
                {
                    deltaX = ui->input.wheel.y * node.scrollOptions().wheelStep;

                    if(ui->input.modifiers.shift == true)
                    {
                        deltaY = 0.f;
                    }
                }

                if(persistentState.scrollData().targetInitialized == false)
                {
                    persistentState.scrollData().target = persistentState.scrollData().position;
                    persistentState.scrollData().targetInitialized = true;
                }

                Vec2 updated = {std::clamp(persistentState.scrollData().target.x - deltaX, 0.f, persistentState.scrollData().range.x), std::clamp(persistentState.scrollData().target.y - deltaY, 0.f, persistentState.scrollData().range.y)};
                bool wheelHasNoEffect = deltaX == 0.f && deltaY == 0.f;
                bool targetUnchanged = wheelHasNoEffect;

                if(updated == persistentState.scrollData().target)
                {
                    targetUnchanged = true;
                }

                if(targetUnchanged == true)
                {
                    if(node.scrollOptions().nested == false)
                    {
                        return true;
                    }

                    return false;
                }

                persistentState.scrollData().target = updated;

                if(node.scrollOptions().smooth == false || node.scrollOptions().smoothDuration <= 0.f || node.style->behavior.animationsEnabled == false)
                {
                    persistentState.scrollData().position = updated;
                    persistentState.scrollData().velocity = {};
                    persistentState.scrollData().value = node.layout.orientation == Orientation::Vertical ? updated.y : updated.x;
                    Detail::markScrollLayout(ui, nodeIndex);
                }

                ui->frame.events.push_back({EventType::Change, node.id, ui->nodePath(node), node.debugData().file, node.debugData().line, ui->input.timestamp});

                return true;
            };

            constexpr double WheelOwnershipTimeout = 0.35;

            if(ui->wheelOwner != InvalidId && ui->input.timestamp - ui->wheelOwnerTimestamp <= WheelOwnershipTimeout)
            {
                Context::Node * owner = ui->findFrameNode(ui->wheelOwner);

                if(owner != nullptr && applyToNode(static_cast<size_t>(owner - ui->nodes.data())))
                {
                    return true;
                }
            }

            ui->wheelOwner = InvalidId;
            ui->wheelOwnerItem = {};
            for(size_t index = ui->nodes.size(); index > 1; --index)
            {
                if(applyToNode(index - 1))
                {
                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        void popupBackdrop(Context * ui, StringView label, bool * open, bool dismissOnClick, const Color & tint, const Rect & foregroundBounds, const SourceLocation & location)
        {
            LayoutOptions layout;
            layout.width = SizeRule::Fill;
            layout.height = SizeRule::Fill;
            String backdropLabel(label);
            backdropLabel += " backdrop";
            size_t node = ui->addNode(Detail::NodeKind::Backdrop, {}, backdropLabel, layout, location);
            Context::Node & backdrop = ui->nodes[node];
            backdrop.tint = tint;
            Response response;
            response.id = backdrop.id;
            const PointerState * pointer = ui->input.primaryPointer();

            if(pointer == nullptr || foregroundBounds.contains(pointer->position) == false)
            {
                response = ui->interact(node, false);
            }
            else
            {
                backdrop.response = response;
            }

            if(dismissOnClick == true && open != nullptr && response.clicked() == true)
            {
                *open = false;
                ui->frame.events.push_back({EventType::PopupClose, backdrop.id, ui->nodePath(backdrop), backdrop.debugData().file, backdrop.debugData().line, ui->input.timestamp});
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void makePopupOpaque(Context * ui, Context::Node & node)
        {
            Theme & style = ui->mutableStyle(node);
            style.colors.background.a = 1.f;
            style.colors.panel.a = 1.f;
            style.colors.panelHeader.a = 1.f;
            style.colors.input.a = 1.f;
            style.colors.border.a = 1.f;
            style.colors.borderStrong.a = 1.f;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float animateVisual(float current, float target, float deltaTime, float duration, bool enabled) noexcept
        {
            if(enabled == false)
            {
                return target;
            }

            if(duration <= 0.f)
            {
                return target;
            }

            float factor = 1.f - std::exp(-6.f * std::max(0.f, deltaTime) / duration);
            float result = current + (target - current) * std::clamp(factor, 0.f, 1.f);
            auto returnedValue = std::abs(result - target) < 0.001f ? target : result;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float smoothScrollAxis(float current, float target, float & velocity, float deltaTime, float duration) noexcept
        {
            float time = std::max(0.001f, duration);
            float step = std::clamp(deltaTime, 0.f, 0.05f);
            float omega = 2.f / time;
            float scaled = omega * step;
            float decay = 1.f / (1.f + scaled + 0.48f * scaled * scaled + 0.235f * scaled * scaled * scaled);
            float change = current - target;
            float temporary = (velocity + omega * change) * step;
            velocity = (velocity - omega * temporary) * decay;
            float result = target + (change + temporary) * decay;

            if((target - current > 0.f) == (result > target))
            {
                result = target;
                velocity = 0.f;
            }

            if(std::abs(result - target) < 0.05f && std::abs(velocity) < 0.5f)
            {
                result = target;
                velocity = 0.f;
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool updateScrollAnimations(Context * ui) noexcept
        {
            bool changed = false;
            for(size_t index = 0; index != ui->nodes.size(); ++index)
            {
                Context::Node & node = ui->nodes[index];
                bool scrollContainer = Detail::scrollsContent(node);
                bool scrollTable = node.kind == Detail::NodeKind::Table && (node.tableOptions().scrollHorizontal || node.tableOptions().scrollVertical == true);

                if(scrollContainer == false && scrollTable == false)
                {
                    continue;
                }

                Context::Persistent & state = ui->state(node);

                if(state.scrollData().targetInitialized == false)
                {
                    state.scrollData().target = state.scrollData().position;
                    state.scrollData().targetInitialized = true;
                }

                state.scrollData().target = {std::clamp(state.scrollData().target.x, 0.f, state.scrollData().range.x), std::clamp(state.scrollData().target.y, 0.f, state.scrollData().range.y)};

                if(state.scrollData().draggingScrollbar == true)
                {
                    state.scrollData().target = state.scrollData().position;
                    state.scrollData().velocity = {};
                    continue;
                }

                bool smooth = node.scrollOptions().smooth && node.scrollOptions().smoothDuration > 0.f && node.style->behavior.animationsEnabled;
                Vec2 updated = state.scrollData().target;

                if(smooth == true)
                {
                    updated.x = Detail::smoothScrollAxis(state.scrollData().position.x, state.scrollData().target.x, state.scrollData().velocity.x, ui->input.deltaTime, node.scrollOptions().smoothDuration);
                    updated.y = Detail::smoothScrollAxis(state.scrollData().position.y, state.scrollData().target.y, state.scrollData().velocity.y, ui->input.deltaTime, node.scrollOptions().smoothDuration);
                }
                else
                {
                    state.scrollData().velocity = {};
                }

                if(updated == state.scrollData().position)
                {
                    continue;
                }

                state.scrollData().position = updated;
                state.scrollData().value = node.layout.orientation == Orientation::Vertical ? updated.y : updated.x;
                Detail::markScrollLayout(ui, index);
                changed = true;
            }

            return changed;
        }
        //////////////////////////////////////////////////////////////////////////
        void updateVisualState(Context::Persistent & state, const Context::Node & node, float deltaTime) noexcept
        {
            const StyleBehavior & behavior = node.style->behavior;
            float hoverTarget = node.response.hovered() ? 1.f : 0.f;
            float activeTarget = node.response.active() ? 1.f : 0.f;
            float selectionTarget = node.checked || node.selected == true || node.highlighted == true || node.expanded ? 1.f : 0.f;
            float focusTarget = node.response.focused() ? 1.f : 0.f;

            // A brief release confirmation is independent of the held/hovered states.
            // Reduced motion keeps the state changes and removes the transient flash.
            state.activationVisual = behavior.animationsEnabled && node.disabled == false
                ? (node.response.clicked() ? 1.f : std::max(0.f, state.activationVisual - std::max(0.f, deltaTime) / 0.18f))
                : 0.f;

            if(state.visualInitialized == false)
            {
                state.visualInitialized = true;
                state.hoverVisual = hoverTarget;
                state.activeVisual = activeTarget;
                state.selectionVisual = selectionTarget;
                state.focusVisual = focusTarget;
                state.scalarVisual = node.valueData().scalar;

                return;
            }

            state.hoverVisual = Detail::animateVisual(state.hoverVisual, hoverTarget, deltaTime, behavior.hoverAnimationDuration, behavior.animationsEnabled);
            state.activeVisual = Detail::animateVisual(state.activeVisual, activeTarget, deltaTime, behavior.activeAnimationDuration, behavior.animationsEnabled);
            state.selectionVisual = Detail::animateVisual(state.selectionVisual, selectionTarget, deltaTime, behavior.selectionAnimationDuration, behavior.animationsEnabled);
            state.focusVisual = Detail::animateVisual(state.focusVisual, focusTarget, deltaTime, behavior.selectionAnimationDuration, behavior.animationsEnabled);
            state.scalarVisual = node.response.active() ? node.valueData().scalar : Detail::animateVisual(state.scalarVisual, node.valueData().scalar, deltaTime, behavior.valueAnimationDuration, behavior.animationsEnabled);
        }
        //////////////////////////////////////////////////////////////////////////
        void drawFrame(DrawList & drawList, const Rect & bounds, float radius, float borderWidth, const Color & fill, const Color & border, uint64_t renderKey)
        {
            if(bounds.empty() == true)
            {
                return;
            }

            BoxStyle style;
            style.fill = Mosaic::solidFill(fill);
            style.radii = {radius, radius, radius, radius};
            style.borderWidth = std::max(0.f, borderWidth);
            style.borderColor = border;
            style.feather = 0.75f;

            drawList.box(bounds, style, renderKey);
        }
        //////////////////////////////////////////////////////////////////////////
        void drawScrollbar(DrawList & drawList, const Rect & track, const Rect & thumb, bool vertical, const Theme & style, float hover, float active, uint64_t renderKey)
        {
            if(track.empty() == true)
            {
                return;
            }

            if(thumb.empty() == true)
            {
                return;
            }

            Color trackColor = Detail::mixColor(style.colors.scrollbar, style.colors.scrollbarHovered, hover);
            trackColor = Detail::mixColor(trackColor, style.colors.scrollbarActive, active);
            drawList.rect(track, trackColor, renderKey);
            float padding = std::max(0.f, style.metrics.scrollbarPadding);
            Rect insetThumb = thumb.inset(padding);
            float maximumRadius = vertical ? insetThumb.width * 0.5f : insetThumb.height * 0.5f;
            float radius = std::min(maximumRadius, std::max(0.f, style.metrics.scrollbarCornerRadius));
            Color fill = Detail::mixColor(style.colors.scrollbarGrab, style.colors.scrollbarGrabHovered, hover);
            fill = Detail::mixColor(fill, style.colors.scrollbarGrabActive, active);
            Detail::drawFrame(drawList, insetThumb, radius, style.metrics.frameBorderSize, fill, Detail::mixColor(style.colors.borderStrong, style.colors.accent, std::max(hover * 0.45f, active)), renderKey);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t previousUtf8(StringView value, size_t position) noexcept
        {
            if(position == 0)
            {
                return 0;
            }

            --position;
            while(position > 0 && (static_cast<unsigned char>(value[position]) & 0xc0U) == 0x80U)
            {
                --position;
            }

            return position;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t nextUtf8(StringView value, size_t position) noexcept
        {
            if(position >= value.size())
            {
                auto returnedValue = value.size();

                return returnedValue;
            }

            ++position;
            while(position < value.size() && (static_cast<unsigned char>(value[position]) & 0xc0U) == 0x80U)
            {
                ++position;
            }

            return position;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t previousTextPosition(const Context::Node & node, StringView value, size_t position, bool password) noexcept
        {
            if(password == true)
            {
                auto returnedValue = Detail::previousUtf8(value, position);

                return returnedValue;
            }

            if(node.textRun == nullptr)
            {
                auto returnedValue = Detail::previousUtf8(value, position);

                return returnedValue;
            }

            if(node.textRun->clusters.empty() == true)
            {
                auto returnedValue = Detail::previousUtf8(value, position);

                return returnedValue;
            }

            const SizeVector & clusters = node.textRun->clusters;
            auto iterator = std::lower_bound(clusters.begin(), clusters.end(), position);

            if(iterator == clusters.begin())
            {
                return 0;
            }

            auto returnedValue = *(iterator - 1);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t nextTextPosition(const Context::Node & node, StringView value, size_t position, bool password) noexcept
        {
            if(password == true)
            {
                auto returnedValue = Detail::nextUtf8(value, position);

                return returnedValue;
            }

            if(node.textRun == nullptr)
            {
                auto returnedValue = Detail::nextUtf8(value, position);

                return returnedValue;
            }

            if(node.textRun->clusters.empty() == true)
            {
                auto returnedValue = Detail::nextUtf8(value, position);

                return returnedValue;
            }

            const SizeVector & clusters = node.textRun->clusters;
            auto iterator = std::upper_bound(clusters.begin(), clusters.end(), position);
            auto returnedValue = iterator == clusters.end() ? value.size() : *iterator;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] String inputDisplayText(StringView value, bool password)
        {
            if(password == false)
            {
                auto returnedValue = String(value);

                return returnedValue;
            }

            String result;
            for(size_t position = 0; position < value.size(); position = Detail::nextUtf8(value, position))
            {
                result += "\xe2\x80\xa2";
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t inputDisplayOffset(StringView value, size_t position, bool password) noexcept
        {
            size_t clamped = std::min(position, value.size());

            if(password == false)
            {
                return clamped;
            }

            size_t result = 0;
            for(size_t cursor = 0; cursor < clamped; cursor = Detail::nextUtf8(value, cursor))
            {
                result += 3;
            }

            return result;
        }

        enum class TextCharacterClass : uint8_t
        {
            Whitespace,
            Word,
            Punctuation
        };
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] TextCharacterClass textCharacterClass(char value) noexcept
        {
            unsigned char byte = static_cast<unsigned char>(value);

            if(std::isspace(byte) != 0)
            {
                return TextCharacterClass::Whitespace;
            }

            if(byte >= 0x80U)
            {
                return TextCharacterClass::Word;
            }

            if(std::isalnum(byte) != 0)
            {
                return TextCharacterClass::Word;
            }

            if(value == '_')
            {
                return TextCharacterClass::Word;
            }

            return TextCharacterClass::Punctuation;
        }
        //////////////////////////////////////////////////////////////////////////
        void selectWord(StringView value, size_t position, size_t & cursor, size_t & anchor) noexcept
        {
            if(value.empty() == true)
            {
                cursor = 0;
                anchor = 0;

                return;
            }

            size_t probe = std::min(position, value.size());

            if(probe == value.size())
            {
                probe = Detail::previousUtf8(value, probe);
            }
            else if(Detail::textCharacterClass(value[probe]) == TextCharacterClass::Whitespace && probe > 0)
            {
                size_t previous = Detail::previousUtf8(value, probe);

                if(Detail::textCharacterClass(value[previous]) != TextCharacterClass::Whitespace)
                {
                    probe = previous;
                }
            }

            TextCharacterClass characterClass = Detail::textCharacterClass(value[probe]);
            size_t begin = probe;
            while(begin > 0)
            {
                size_t previous = Detail::previousUtf8(value, begin);

                if(Detail::textCharacterClass(value[previous]) != characterClass)
                {
                    break;
                }

                begin = previous;
            }

            size_t end = Detail::nextUtf8(value, probe);
            while(end < value.size() && Detail::textCharacterClass(value[end]) == characterClass)
            {
                end = Detail::nextUtf8(value, end);
            }
            anchor = begin;
            cursor = end;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t previousWord(StringView value, size_t position) noexcept
        {
            size_t cursor = std::min(position, value.size());
            while(cursor > 0)
            {
                size_t previous = Detail::previousUtf8(value, cursor);

                if(Detail::textCharacterClass(value[previous]) != TextCharacterClass::Whitespace)
                {
                    break;
                }

                cursor = previous;
            }
            while(cursor > 0)
            {
                size_t previous = Detail::previousUtf8(value, cursor);

                if(Detail::textCharacterClass(value[previous]) != TextCharacterClass::Punctuation)
                {
                    break;
                }

                cursor = previous;
            }
            while(cursor > 0)
            {
                size_t previous = Detail::previousUtf8(value, cursor);

                if(Detail::textCharacterClass(value[previous]) != TextCharacterClass::Word)
                {
                    break;
                }

                cursor = previous;
            }

            return cursor;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t nextWord(StringView value, size_t position) noexcept
        {
            size_t cursor = std::min(position, value.size());
            TextCharacterClass initialClass = cursor < value.size() ? Detail::textCharacterClass(value[cursor]) : TextCharacterClass::Whitespace;
            while(cursor < value.size() && Detail::textCharacterClass(value[cursor]) == initialClass)
            {
                cursor = Detail::nextUtf8(value, cursor);
            }
            while(cursor < value.size() && Detail::textCharacterClass(value[cursor]) != TextCharacterClass::Word)
            {
                cursor = Detail::nextUtf8(value, cursor);
            }

            return cursor;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t textExtent(const Context::Node & node) noexcept
        {
            // an input node still carries its widget label while the pointer is hit tested,
            // because the label only becomes the display text further down the widget, so
            // the caret range has to come from the shaped run
            if(node.textRun != nullptr)
            {
                if(node.textRun->positions.empty() == false)
                {
                    return node.textRun->positions.size() - 1;
                }

                if(node.textRun->clusters.empty() == false)
                {
                    return node.textRun->clusters.back();
                }
            }

            return node.label.size();
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Vec2 textPosition(const Context::Node & node, size_t position) noexcept
        {
            size_t clamped = std::min(position, Detail::textExtent(node));

            if(node.textRun != nullptr && clamped < node.textRun->positions.size())
            {
                return node.textRun->positions[clamped];
            }

            size_t line = 0;
            for(size_t cursor = 0; cursor < clamped; ++cursor)
            {
                if(node.label[cursor] == '\n')
                {
                    ++line;
                }
            }
            float offset = node.textRun != nullptr && clamped < node.textRun->offsets.size() ? node.textRun->offsets[clamped] : 0.f;
            Vec2 result = {offset, static_cast<float>(line) * node.style->metrics.lineHeight};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t visualLineIndex(const Context::Node & node, size_t position) noexcept
        {
            Vec2 positionValue = Detail::textPosition(node, position);
            auto returnedValue = static_cast<size_t>(std::max(0.f, std::round(positionValue.y / std::max(1.f, node.style->metrics.lineHeight))));

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ShapedTextLine visualLine(const Context::Node & node, StringView value, size_t lineIndex) noexcept
        {
            if(node.textRun != nullptr && node.textRun->lines.empty() == false)
            {
                auto returnedValue = node.textRun->lines[std::min(lineIndex, node.textRun->lines.size() - 1)];

                return returnedValue;
            }

            size_t begin = 0;
            for(size_t line = 0; line < lineIndex; ++line)
            {
                size_t newline = value.find('\n', begin);

                if(newline == StringView::npos)
                {
                    ShapedTextLine result = {value.size(), value.size(), 0.f};

                    return result;
                }

                begin = newline + 1;
            }
            size_t newline = value.find('\n', begin);
            size_t end = newline == StringView::npos ? value.size() : newline;
            ShapedTextLine result = {begin, end, 0.f};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float textOffset(const Context::Node & node, size_t position) noexcept
        {
            auto returnedValue = Detail::textPosition(node, position).x;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t moveVertical(const Context::Node & node, StringView value, size_t position, int lines) noexcept
        {
            float desiredX = Detail::textOffset(node, position);
            size_t currentLine = Detail::visualLineIndex(node, position);
            size_t lineCount = node.textRun == nullptr || node.textRun->lines.empty() == true ? currentLine + 1 : node.textRun->lines.size();
            size_t targetLine = static_cast<size_t>(std::clamp<int64_t>(static_cast<int64_t>(currentLine) + lines, 0, static_cast<int64_t>(lineCount - 1)));
            ShapedTextLine range = Detail::visualLine(node, value, targetLine);
            size_t best = range.begin;
            float bestDistance = std::abs(Detail::textOffset(node, range.begin) - desiredX);
            for(size_t candidate = range.begin; candidate < range.end;)
            {
                candidate = Detail::nextTextPosition(node, value, candidate, false);
                float candidateX = candidate == range.end ? range.width : Detail::textOffset(node, candidate);
                float distance = std::abs(candidateX - desiredX);

                if(distance <= bestDistance)
                {
                    best = candidate;
                    bestDistance = distance;
                }
            }

            return best;
        }
        //////////////////////////////////////////////////////////////////////////
        void selectLine(const Context::Node & node, StringView value, size_t position, size_t & cursor, size_t & anchor) noexcept
        {
            ShapedTextLine line = Detail::visualLine(node, value, Detail::visualLineIndex(node, position));
            anchor = line.begin;
            cursor = line.end;

            if(cursor < value.size() && value[cursor] == '\n')
            {
                ++cursor;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t visualLineBoundary(const Context::Node & node, StringView value, size_t position, bool end) noexcept
        {
            ShapedTextLine line = Detail::visualLine(node, value, Detail::visualLineIndex(node, position));

            return end ? line.end : line.begin;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float inputTextViewportWidth(const Context::Node & node, const Rect & bounds) noexcept
        {
            auto returnedValue = std::max(0.f, bounds.width - node.style->metrics.padding * 2.f - node.style->metrics.frameBorderSize * 2.f);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t inputPositionAtPointer(Context * ui, const Context::Node & node, StringView value, bool password, const Vec2 & pointer)
        {
            const Context::Persistent & persistentState = ui->state(node.id);
            const TextEditorState & editorState = ui->textEditorState(node.id);
            const Rect & bounds = persistentState.lastBounds;
            float pointerX = pointer.x - bounds.x - node.style->metrics.padding;
            float viewportWidth = Detail::inputTextViewportWidth(node, bounds);
            float viewportX = pointer.x >= bounds.x && pointer.x <= bounds.right() ? std::clamp(pointerX, 0.f, viewportWidth) : pointerX;
            float localX = viewportX + editorState.textScrollX;
            float localY = pointer.y - persistentState.lastBounds.y - node.style->metrics.padding * 0.5f + editorState.textScrollY;

            size_t requestedLine = node.textEditData().multiline ? static_cast<size_t>(std::max(0.f, std::floor(localY / std::max(1.f, node.style->metrics.lineHeight)))) : 0;
            ShapedTextLine line = Detail::visualLine(node, value, requestedLine);
            size_t lineStart = line.begin;
            size_t lineEnd = line.end;

            if(localX <= 0.f)
            {
                return lineStart;
            }

            String rendered;
            float previousWidth = 0.f;
            size_t cursor = lineStart;
            while(cursor < lineEnd)
            {
                size_t next = Detail::nextTextPosition(node, value, cursor, password);
                float width = 0.f;

                if(password == true)
                {
                    rendered += "\xe2\x80\xa2";
                    width = ui->measureText(rendered, *node.style).x;
                }
                else
                {
                    width = next == lineEnd ? line.width : Detail::textOffset(node, next);
                }

                if(localX < (previousWidth + width) * 0.5f)
                {
                    return cursor;
                }

                previousWidth = width;
                cursor = next;
            }

            return lineEnd;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Vec2 textCursorPosition(Context * ui, const Context::Node & node, size_t position)
        {
            size_t clamped = std::min(position, Detail::textExtent(node));
            Vec2 local = Detail::textPosition(node, clamped);
            float x = Detail::snapToPixel(node.bounds.x + node.style->metrics.padding - node.textEditData().scrollX + local.x, ui->viewport.dpiScale);
            float y = Detail::snapToPixel(node.bounds.y + node.style->metrics.padding * 0.5f - node.textEditData().scrollY + local.y, ui->viewport.dpiScale);
            Vec2 result = {x, y};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        void drawTextSelection(Context * ui, DrawList & drawList, const Context::Node & node, const Color & color, uint64_t renderKey)
        {
            size_t selectionBegin = std::min(node.textEditData().cursor, node.textEditData().anchor);
            size_t selectionEnd = std::max(node.textEditData().cursor, node.textEditData().anchor);

            if(selectionBegin == selectionEnd)
            {
                return;
            }

            size_t lineCount = node.textRun == nullptr || node.textRun->lines.empty() == true ? 1 : node.textRun->lines.size();
            for(size_t lineIndex = 0; lineIndex != lineCount; ++lineIndex)
            {
                ShapedTextLine line = Detail::visualLine(node, node.label, lineIndex);
                size_t lineStart = line.begin;
                size_t lineEnd = line.end;
                size_t begin = std::clamp(selectionBegin, lineStart, lineEnd);
                size_t end = std::clamp(selectionEnd, lineStart, lineEnd);
                bool selectsNewline = lineEnd < node.label.size() && node.label[lineEnd] == '\n' && selectionBegin <= lineEnd && selectionEnd > lineEnd;

                if(begin < end || selectsNewline == true)
                {
                    float beginX = begin == lineEnd ? line.width : Detail::textOffset(node, begin);
                    float x = Detail::snapToPixel(node.bounds.x + node.style->metrics.padding - node.textEditData().scrollX + beginX, ui->viewport.dpiScale);
                    float endX = end == lineEnd ? line.width : Detail::textOffset(node, end);
                    float width = endX - beginX;

                    if(selectsNewline == true)
                    {
                        width += node.style->metrics.fontSize * 0.42f;
                    }

                    width = Detail::snapToPixel(width, ui->viewport.dpiScale);
                    drawList.roundedRect({x, Detail::snapToPixel(node.bounds.y + node.style->metrics.padding * 0.5f - node.textEditData().scrollY + static_cast<float>(lineIndex) * node.style->metrics.lineHeight, ui->viewport.dpiScale), std::max(2.f, width), node.style->metrics.lineHeight}, 2.f, color, renderKey);
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void updateTextScroll(Context::Node & node, TextEditorState & state, const Rect & bounds, bool focused)
        {
            if(node.textEditData().hint == true)
            {
                state.textScrollX = 0.f;
                state.textScrollY = 0.f;
                node.textEditData().scrollX = 0.f;
                node.textEditData().scrollY = 0.f;

                return;
            }

            if(focused == false)
            {
                state.textScrollX = 0.f;
                node.textEditData().scrollX = 0.f;

                if(node.textEditData().multiline == true)
                {
                    float visibleHeight = std::max(0.f, bounds.height - node.style->metrics.padding - node.style->metrics.frameBorderSize * 2.f);
                    float maximumY = std::max(0.f, node.textSize.y - visibleHeight);
                    state.textScrollY = std::clamp(state.textScrollY, 0.f, maximumY);
                }
                else
                {
                    state.textScrollY = 0.f;
                }

                node.textEditData().scrollY = state.textScrollY;

                return;
            }

            float visibleWidth = Detail::inputTextViewportWidth(node, bounds);
            size_t cursor = std::min(node.textEditData().cursor, Detail::textExtent(node));
            Vec2 cursorPosition = Detail::textPosition(node, cursor);
            float cursorX = cursorPosition.x;

            if(cursorX - state.textScrollX > visibleWidth)
            {
                state.textScrollX = cursorX - visibleWidth;
            }
            else if(cursorX < state.textScrollX)
            {
                state.textScrollX = cursorX;
            }

            float maximumScroll = std::max(0.f, node.textSize.x - visibleWidth);
            state.textScrollX = std::clamp(state.textScrollX, 0.f, maximumScroll);
            node.textEditData().scrollX = state.textScrollX;

            if(node.textEditData().multiline == true)
            {
                float visibleHeight = std::max(0.f, bounds.height - node.style->metrics.padding - node.style->metrics.frameBorderSize * 2.f);
                float cursorTop = cursorPosition.y;

                if(cursorTop < state.textScrollY)
                {
                    state.textScrollY = cursorTop;
                }
                else if(cursorTop + node.style->metrics.lineHeight > state.textScrollY + visibleHeight)
                {
                    state.textScrollY = cursorTop + node.style->metrics.lineHeight - visibleHeight;
                }

                float maximumY = std::max(0.f, node.textSize.y - visibleHeight);
                state.textScrollY = std::clamp(state.textScrollY, 0.f, maximumY);
            }
            else
            {
                state.textScrollY = 0.f;
            }

            node.textEditData().scrollY = state.textScrollY;
        }
        //////////////////////////////////////////////////////////////////////////
        void autoScrollTextSelection(const Context::Node & node, TextEditorState & state, const Rect & bounds, const PointerState & pointer, float deltaTime)
        {
            float speed = std::max(36.f, node.style->metrics.lineHeight * 8.f) * std::max(0.f, deltaTime);

            if(pointer.position.x < bounds.x)
            {
                state.textScrollX = std::max(0.f, state.textScrollX - speed * (1.f + (bounds.x - pointer.position.x) / 24.f));
            }
            else if(pointer.position.x > bounds.right())
            {
                state.textScrollX += speed * (1.f + (pointer.position.x - bounds.right()) / 24.f);
            }

            if(node.textEditData().multiline == true)
            {
                if(pointer.position.y < bounds.y)
                {
                    state.textScrollY = std::max(0.f, state.textScrollY - speed * (1.f + (bounds.y - pointer.position.y) / 24.f));
                }
                else if(pointer.position.y > bounds.bottom())
                {
                    state.textScrollY += speed * (1.f + (pointer.position.y - bounds.bottom()) / 24.f);
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Context::Persistent & Context::state(Id id)
    {
        auto [iterator, inserted] = persistent.try_emplace(id);

        if(inserted == true)
        {
            iterator->second.firstFrame = frame.number;
            iterator->second.allocator = allocator;
        }

        iterator->second.lastFrame = frame.number;

        return iterator->second;
    }
    //////////////////////////////////////////////////////////////////////////
    Context::Persistent & Context::state(Node & node)
    {
        if(node.persistentState == nullptr)
        {
            node.persistentState = &state(node.id);
        }
        else
        {
            node.persistentState->lastFrame = frame.number;
        }

        return *node.persistentState;
    }
    //////////////////////////////////////////////////////////////////////////
    Context::Persistent * Context::findState(Id id) noexcept
    {
        auto iterator = persistent.find(id);
        auto returnedValue = iterator == persistent.end() ? nullptr : &iterator->second;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    const Context::Persistent * Context::findState(Id id) const noexcept
    {
        auto iterator = persistent.find(id);
        auto returnedValue = iterator == persistent.end() ? nullptr : &iterator->second;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Detail::TextEditorState & Context::textEditorState(Id id)
    {
        Detail::TextEditorState & returnedValue = textEditorStates.try_emplace(id).first->second;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    const Detail::TextEditorState * Context::findTextEditorState(Id id) const noexcept
    {
        auto iterator = textEditorStates.find(id);
        auto returnedValue = iterator == textEditorStates.end() ? nullptr : &iterator->second;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Detail::ColorEditorState & Context::colorEditorState(Id id)
    {
        Detail::ColorEditorState & returnedValue = colorEditorStates.try_emplace(id).first->second;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Context::TabState & Context::tabState(Id id)
    {
        TabState & value = tabStates.try_emplace(id).first->second;
        value.lastFrame = frame.number;

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    Detail::NumericState & Context::numericState(Persistent & persistentState)
    {
        if(persistentState.numericStateIndex == std::numeric_limits<size_t>::max())
        {
            if(freeNumericStateIndices.empty() == true)
            {
                persistentState.numericStateIndex = numericStates.size();
                numericStates.emplace_back();
            }
            else
            {
                persistentState.numericStateIndex = freeNumericStateIndices.back();
                freeNumericStateIndices.pop_back();
                numericStates[persistentState.numericStateIndex] = {};
            }
        }

        return numericStates[persistentState.numericStateIndex];
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::collectPersistentStateGarbage()
    {
        constexpr uint64_t TransientRetentionFrames = 3600;
        constexpr uint64_t DurableRetentionFrames = 36000;

        if(frame.number < nextPersistentStateSweep)
        {
            return;
        }

        nextPersistentStateSweep = frame.number + 300;

        for(auto iterator = persistent.begin(); iterator != persistent.end();)
        {
            Id id = iterator->first;
            Persistent & value = iterator->second;
            const Persistent & readOnlyValue = value;
            bool durableWindow = value.window != nullptr && readOnlyValue.windowData().initialized == true;
            bool durableScroll = value.scroll != nullptr && (readOnlyValue.scrollData().position != Vec2{} || readOnlyValue.scrollData().target != Vec2{});
            bool durable = durableWindow || durableScroll || value.expandedInitialized == true || value.table != nullptr;
            uint64_t retention = durable ? DurableRetentionFrames : TransientRetentionFrames;
            bool retain = frame.number - value.lastFrame <= retention;

            if(id == focused)
            {
                retain = true;
            }

            if(id == navigationFocused)
            {
                retain = true;
            }

            if(id == active)
            {
                retain = true;
            }

            if(id == captured)
            {
                retain = true;
            }

            if(retain == true)
            {
                ++iterator;
                continue;
            }

            if(value.numericStateIndex != std::numeric_limits<size_t>::max() && value.numericStateIndex < numericStates.size())
            {
                numericStates[value.numericStateIndex] = {};
                freeNumericStateIndices.push_back(value.numericStateIndex);
            }

            textEditorStates.erase(id);
            colorEditorStates.erase(id);
            iterator = persistent.erase(iterator);
        }

        for(auto iterator = tabStates.begin(); iterator != tabStates.end();)
        {
            if(frame.number - iterator->second.lastFrame <= TransientRetentionFrames)
            {
                ++iterator;
            }
            else
            {
                iterator = tabStates.erase(iterator);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Context::ColorTextData & Context::ensureColorTextData(Node & node)
    {
        if(node.valueData().colorTextDataIndex == std::numeric_limits<size_t>::max())
        {
            node.valueData().colorTextDataIndex = frameColorTextDataCount++;

            if(node.valueData().colorTextDataIndex == frameColorTextData.size())
            {
                frameColorTextData.emplace_back();
            }

            ColorTextData & colorText = frameColorTextData[node.valueData().colorTextDataIndex];
            for(String & text : colorText.text)
            {
                text.clear();
            }
            colorText.runs = {};
            colorText.sizes = {};
            colorText.prepared = {true, true, true, true};
        }

        return frameColorTextData[node.valueData().colorTextDataIndex];
    }
    //////////////////////////////////////////////////////////////////////////
    const Context::ColorTextData * Context::findColorTextData(const Node & node) const noexcept
    {
        auto returnedValue = node.valueData().colorTextDataIndex < frameColorTextData.size() ? &frameColorTextData[node.valueData().colorTextDataIndex] : nullptr;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Context::Node * Context::findFrameNode(Id id) noexcept
    {
        size_t index = findFrameNodeIndex(id);

        if(index >= nodes.size())
        {
            return nullptr;
        }

        return &nodes[index];
    }
    //////////////////////////////////////////////////////////////////////////
    const Context::Node * Context::findFrameNode(Id id) const noexcept
    {
        size_t index = findFrameNodeIndex(id);

        if(index >= nodes.size())
        {
            return nullptr;
        }

        return &nodes[index];
    }
    //////////////////////////////////////////////////////////////////////////
    Context::Node * Context::findFrameNode(const ItemRef & item) noexcept
    {
        size_t index = findFrameNodeIndex(item);

        if(index >= nodes.size())
        {
            return nullptr;
        }

        return &nodes[index];
    }
    //////////////////////////////////////////////////////////////////////////
    const Context::Node * Context::findFrameNode(const ItemRef & item) const noexcept
    {
        size_t index = findFrameNodeIndex(item);

        if(index >= nodes.size())
        {
            return nullptr;
        }

        return &nodes[index];
    }
    //////////////////////////////////////////////////////////////////////////
    size_t Context::findFrameNodeIndex(Id id) const noexcept
    {
        if(frameNodeIndices.empty() == true)
        {
            auto returnedValue = std::numeric_limits<size_t>::max();

            return returnedValue;
        }

        size_t mask = frameNodeIndices.size() - 1;
        size_t slot = std::hash<Id>{}(id)&mask;
        while(frameNodeIndices[slot].frame == frame.number)
        {
            if(frameNodeIndices[slot].id == id)
            {
                return frameNodeIndices[slot].index;
            }

            slot = (slot + 1) & mask;
        }
        auto returnedValue = std::numeric_limits<size_t>::max();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    size_t Context::findFrameNodeIndex(const ItemRef & item) const noexcept
    {
        if(item.valid() == false)
        {
            auto returnedValue = std::numeric_limits<size_t>::max();

            return returnedValue;
        }

        for(size_t index = 0; index != nodes.size(); ++index)
        {
            if(nodes[index].item == item)
            {
                return index;
            }
        }

        auto returnedValue = std::numeric_limits<size_t>::max();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DrawCommandVector & Context::canvasCommands(Node & node)
    {
        DrawCommandVector & commands = canvasCommands(node, node.canvasLayer);

        return commands;
    }
    //////////////////////////////////////////////////////////////////////////
    DrawCommandVector & Context::canvasCommands(Node & node, CanvasLayer layer)
    {
        size_t * commandIndex = &node.canvasCommandIndex;

        if(layer == CanvasLayer::Overlay)
        {
            commandIndex = &node.canvasOverlayCommandIndex;
        }
        else if(layer == CanvasLayer::Background)
        {
            commandIndex = &node.canvasBackgroundCommandIndex;
        }
        else if(layer == CanvasLayer::Foreground)
        {
            commandIndex = &node.canvasForegroundCommandIndex;
        }

        if(*commandIndex == std::numeric_limits<size_t>::max())
        {
            *commandIndex = frameCanvasCommandCount++;

            if(*commandIndex == frameCanvasCommands.size())
            {
                frameCanvasCommands.emplace_back();
            }
            else
            {
                frameCanvasCommands[*commandIndex].clear();
            }
        }

        return frameCanvasCommands[*commandIndex];
    }
    //////////////////////////////////////////////////////////////////////////
    bool Context::hasCanvasCommands(const Node & node, CanvasLayer layer) const noexcept
    {
        size_t commandIndex = node.canvasCommandIndex;

        if(layer == CanvasLayer::Local && node.canvasOverlayCommandIndex != std::numeric_limits<size_t>::max())
        {
            return true;
        }

        if(layer == CanvasLayer::Overlay)
        {
            commandIndex = node.canvasOverlayCommandIndex;
        }
        else if(layer == CanvasLayer::Background)
        {
            commandIndex = node.canvasBackgroundCommandIndex;
        }
        else if(layer == CanvasLayer::Foreground)
        {
            commandIndex = node.canvasForegroundCommandIndex;
        }

        auto returnedValue = commandIndex != std::numeric_limits<size_t>::max();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Context::FrameNodeStrings & Context::ensureNodeStrings(Node & node)
    {
        if(node.frameStringIndex == std::numeric_limits<size_t>::max())
        {
            node.frameStringIndex = frameNodeStringCount++;

            if(node.frameStringIndex == frameNodeStrings.size())
            {
                frameNodeStrings.emplace_back();
            }

            FrameNodeStrings & strings = frameNodeStrings[node.frameStringIndex];
            strings.semanticName.clear();
            strings.semanticDescription.clear();
            strings.semanticValue.clear();
            strings.path.clear();
            strings.identityDebugValue.clear();
        }

        return frameNodeStrings[node.frameStringIndex];
    }
    //////////////////////////////////////////////////////////////////////////
    String & Context::nodeSemanticValue(Node & node)
    {
        String & returnedValue = ensureNodeStrings(node).semanticValue;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    StringView Context::nodeSemanticValue(const Node & node) const noexcept
    {
        auto returnedValue = node.frameStringIndex < frameNodeStringCount ? StringView(frameNodeStrings[node.frameStringIndex].semanticValue) : StringView{};

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    String & Context::nodeSemanticName(Node & node)
    {
        String & returnedValue = ensureNodeStrings(node).semanticName;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    StringView Context::nodeSemanticName(const Node & node) const noexcept
    {
        auto returnedValue = node.frameStringIndex < frameNodeStringCount ? StringView(frameNodeStrings[node.frameStringIndex].semanticName) : StringView{};

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    String & Context::nodeSemanticDescription(Node & node)
    {
        String & returnedValue = ensureNodeStrings(node).semanticDescription;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    StringView Context::nodeSemanticDescription(const Node & node) const noexcept
    {
        auto returnedValue = node.frameStringIndex < frameNodeStringCount ? StringView(frameNodeStrings[node.frameStringIndex].semanticDescription) : StringView{};

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::indexFrameNode(Id id, size_t index)
    {
        constexpr size_t minimumCapacity = 256;

        if(frameNodeIndices.empty() == true || (nodes.size() + 1) * 2 > frameNodeIndices.size())
        {
            size_t capacity = frameNodeIndices.empty() == true ? minimumCapacity : frameNodeIndices.size() * 2;
            FrameNodeIndexVector previous = std::move(frameNodeIndices);
            frameNodeIndices.assign(capacity, {});
            size_t mask = capacity - 1;
            for(const FrameNodeIndexEntry & entry : previous)
            {
                if(entry.frame != frame.number)
                {
                    continue;
                }

                size_t slot = std::hash<Id>{}(entry.id) & mask;
                while(frameNodeIndices[slot].frame == frame.number)
                {
                    slot = (slot + 1) & mask;
                }
                frameNodeIndices[slot] = entry;
            }
        }

        size_t mask = frameNodeIndices.size() - 1;
        size_t slot = std::hash<Id>{}(id)&mask;
        while(frameNodeIndices[slot].frame == frame.number)
        {
            if(frameNodeIndices[slot].id == id)
            {
                return;
            }

            slot = (slot + 1) & mask;
        }
        frameNodeIndices[slot] = {id, index, frame.number};
    }
    //////////////////////////////////////////////////////////////////////////
    const Context::InteractionSnapshotItem * Context::findInteractionSnapshot(const ItemRef & item) const noexcept
    {
        if(item.valid() == false)
        {
            return nullptr;
        }

        if(interactionSnapshotIndices.empty() == true)
        {
            return nullptr;
        }

        Id hash = combineId(combineId(item.id, item.submission), item.occurrence);
        size_t mask = interactionSnapshotIndices.size() - 1;
        size_t slot = std::hash<Id>{}(hash) & mask;
        while(interactionSnapshotIndices[slot].generation == interactionSnapshotGeneration)
        {
            const InteractionSnapshotIndexEntry & entry = interactionSnapshotIndices[slot];

            if(entry.item == item)
            {
                return &interactionSnapshot[entry.index];
            }

            slot = (slot + 1) & mask;
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    ItemRef Context::addWindowFrameInstance(size_t node, uint64_t submission)
    {
        ItemRef item;
        item.id = nodes[node].id;
        item.submission = submission;

        for(const WindowFrameInstance & instance : windowFrameInstances)
        {
            if(instance.item.id == item.id)
            {
                ++item.occurrence;
            }
        }

        windowFrameInstances.push_back({item, node});

        return item;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Context::windowHasMultipleFrameInstances(Id id) const noexcept
    {
        size_t count = 0;

        for(const WindowFrameInstance & instance : windowFrameInstances)
        {
            if(instance.item.id != id)
            {
                continue;
            }

            ++count;

            if(count > 1)
            {
                return true;
            }
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::captureInteractionSnapshot()
    {
        interactionSnapshot.clear();
        interactionScrollTransforms.clear();
        ++interactionSnapshotGeneration;

        if(interactionSnapshotGeneration == 0)
        {
            interactionSnapshotGeneration = 1;
            interactionSnapshotIndices.clear();
        }

        interactionSnapshot.reserve(nodes.size());
        for(size_t nodeIndex = 1; nodeIndex != nodes.size(); ++nodeIndex)
        {
            const Node & node = nodes[nodeIndex];

            if(node.item.valid() == false)
            {
                continue;
            }

            bool queryable = node.persistentState != nullptr || node.focusable || node.kind == Detail::NodeKind::Window;

            if(queryable == false)
            {
                continue;
            }

            InteractionSnapshotItem item;
            item.item = node.item;
            item.kind = node.kind;
            item.identityParent = node.identityParent;
            item.inputLayer = node.inputLayer;
            item.windowOwner = node.windowOwner;
            item.windowSubmission = node.windowSubmission;
            item.capturedBounds = node.persistentState == nullptr ? node.bounds : node.persistentState->lastBounds;
            item.capturedClip = node.persistentState == nullptr ? node.clip : node.persistentState->lastClip;
            item.bounds = item.capturedBounds;
            item.clip = item.capturedClip;
            item.textSize = node.textSize;
            item.scrollOptions = node.scrollOptions();
            item.tableOptions = node.tableOptions();
            item.textVisibleHeight = std::max(0.f, node.bounds.height - node.style->metrics.padding - node.style->metrics.frameBorderSize * 2.f);
            item.animationsEnabled = node.style->behavior.animationsEnabled;
            item.visible = node.visible && (node.response.flags & Detail::CulledNodeFlag) == 0;
            item.disabled = node.disabled;
            item.inputBlocked = node.inputBlocked;
            item.navigationBlocked = node.navigationBlocked;
            item.focusable = node.focusable;
            item.scrollTransformBegin = interactionScrollTransforms.size();

            size_t parent = node.parent;
            while(parent < nodes.size() && parent != 0)
            {
                const Node & ancestor = nodes[parent];

                if(ancestor.kind == Detail::NodeKind::Window && item.capturedWindowBounds.empty() == true)
                {
                    item.capturedWindowBounds = ancestor.bounds;
                }

                bool scrollAncestor = Detail::scrollsContent(ancestor) || ancestor.kind == Detail::NodeKind::Table;

                if(scrollAncestor == true)
                {
                    const Persistent * scrollState = findState(ancestor.id);

                    if(scrollState != nullptr)
                    {
                        InteractionScrollTransform transform;
                        transform.id = ancestor.id;
                        transform.position = scrollState->scrollData().position;
                        transform.clip = ancestor.childrenClip;
                        interactionScrollTransforms.push_back(transform);
                    }
                }

                parent = ancestor.parent;
            }

            item.scrollTransformCount = interactionScrollTransforms.size() - item.scrollTransformBegin;
            const Persistent * windowState = findState(node.kind == Detail::NodeKind::Window ? node.id : node.windowOwner);

            if(windowState != nullptr)
            {
                item.windowZOrder = windowState->windowData().zOrder;
                item.windowPopup = windowState->windowData().popup;
                item.windowAcceptsPointerInput = windowState->windowData().acceptsPointerInput;
            }

            interactionSnapshot.push_back(item);
        }

        for(const WindowFrameInstance & instance : windowFrameInstances)
        {
            const Node & node = nodes[instance.node];

            if(instance.item == node.item)
            {
                continue;
            }

            auto primary = std::find_if(interactionSnapshot.begin(), interactionSnapshot.end(), [&node](const InteractionSnapshotItem & item)
            {
                return item.item == node.item;
            });

            if(primary == interactionSnapshot.end())
            {
                continue;
            }

            InteractionSnapshotItem item = *primary;
            item.item = instance.item;
            item.windowSubmission = instance.item.submission;
            interactionSnapshot.push_back(item);
        }

        size_t capacity = 256;
        while(capacity < interactionSnapshot.size() * 2)
        {
            capacity *= 2;
        }

        if(interactionSnapshotIndices.size() != capacity)
        {
            interactionSnapshotIndices.assign(capacity, {});
        }

        size_t mask = interactionSnapshotIndices.size() - 1;
        for(size_t index = 0; index != interactionSnapshot.size(); ++index)
        {
            const ItemRef & item = interactionSnapshot[index].item;
            Id hash = combineId(combineId(item.id, item.submission), item.occurrence);
            size_t slot = std::hash<Id>{}(hash) & mask;
            while(interactionSnapshotIndices[slot].generation == interactionSnapshotGeneration)
            {
                slot = (slot + 1) & mask;
            }

            interactionSnapshotIndices[slot] = {item, index, interactionSnapshotGeneration};
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::prepareInteractionSnapshot()
    {
        pointerWindow = InvalidId;
        pointerWindowSubmission = 0;
        const PointerState * pointer = input.primaryPointer();
        bool selectedPopup = false;
        uint64_t selectedOrder = 0;

        for(InteractionSnapshotItem & item : interactionSnapshot)
        {
            item.bounds = item.capturedBounds;
            item.clip = item.capturedClip;
            Vec2 windowTranslation;
            Id window = item.kind == Detail::NodeKind::Window ? item.item.id : item.windowOwner;
            const Persistent * windowState = findState(window);

            if(windowState != nullptr && item.capturedWindowBounds.empty() == false)
            {
                windowTranslation = {windowState->windowData().bounds.x - item.capturedWindowBounds.x, windowState->windowData().bounds.y - item.capturedWindowBounds.y};
                item.bounds.x += windowTranslation.x;
                item.bounds.y += windowTranslation.y;
                item.clip.x += windowTranslation.x;
                item.clip.y += windowTranslation.y;
            }

            for(size_t index = 0; index != item.scrollTransformCount; ++index)
            {
                const InteractionScrollTransform & transform = interactionScrollTransforms[item.scrollTransformBegin + index];
                const Persistent * scrollState = findState(transform.id);

                if(scrollState == nullptr)
                {
                    item.visible = false;
                    break;
                }

                Vec2 delta = scrollState->scrollData().position - transform.position;
                item.bounds.x -= delta.x;
                item.bounds.y -= delta.y;
                Rect ancestorClip = transform.clip;
                ancestorClip.x += windowTranslation.x;
                ancestorClip.y += windowTranslation.y;
                item.clip = Rect::intersection(item.clip, ancestorClip);
            }

            if(pointer == nullptr)
            {
                continue;
            }

            if(item.kind != Detail::NodeKind::Window)
            {
                continue;
            }

            if(item.visible == false || item.windowAcceptsPointerInput == false)
            {
                continue;
            }

            if(item.bounds.contains(pointer->position) == false)
            {
                continue;
            }

            bool selectWindow = pointerWindow == InvalidId;

            if(item.windowPopup == true && selectedPopup == false)
            {
                selectWindow = true;
            }

            if(item.windowPopup == selectedPopup && item.windowZOrder >= selectedOrder)
            {
                selectWindow = true;
            }

            if(selectWindow == true)
            {
                pointerWindow = item.item.id;
                pointerWindowSubmission = windowHasMultipleFrameInstances(item.item.id) == true ? 0 : item.item.submission;
                selectedPopup = item.windowPopup;
                selectedOrder = item.windowZOrder;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::updateInteractionScrollAnimations()
    {
        for(const InteractionSnapshotItem & item : interactionSnapshot)
        {
            bool scrollContainer = item.kind == Detail::NodeKind::Scroll;
            bool scrollTable = item.kind == Detail::NodeKind::Table && (item.tableOptions.scrollHorizontal || item.tableOptions.scrollVertical);

            if(scrollContainer == false && scrollTable == false)
            {
                continue;
            }

            Persistent * persistent = findState(item.item.id);

            if(persistent == nullptr)
            {
                continue;
            }

            if(persistent->scrollData().animationFrame == frame.number)
            {
                continue;
            }

            persistent->scrollData().animationFrame = frame.number;

            if(persistent->scrollData().targetInitialized == false)
            {
                persistent->scrollData().target = persistent->scrollData().position;
                persistent->scrollData().targetInitialized = true;
            }

            persistent->scrollData().target.x = std::clamp(persistent->scrollData().target.x, 0.f, persistent->scrollData().range.x);
            persistent->scrollData().target.y = std::clamp(persistent->scrollData().target.y, 0.f, persistent->scrollData().range.y);

            if(persistent->scrollData().draggingScrollbar == true)
            {
                persistent->scrollData().target = persistent->scrollData().position;
                persistent->scrollData().velocity = {};
                continue;
            }

            bool smooth = item.scrollOptions.smooth && item.scrollOptions.smoothDuration > 0.f && item.animationsEnabled;
            Vec2 updated = persistent->scrollData().target;

            if(smooth == true)
            {
                updated.x = Detail::smoothScrollAxis(persistent->scrollData().position.x, persistent->scrollData().target.x, persistent->scrollData().velocity.x, input.deltaTime, item.scrollOptions.smoothDuration);
                updated.y = Detail::smoothScrollAxis(persistent->scrollData().position.y, persistent->scrollData().target.y, persistent->scrollData().velocity.y, input.deltaTime, item.scrollOptions.smoothDuration);
            }
            else
            {
                persistent->scrollData().velocity = {};
            }

            persistent->scrollData().position = updated;
            persistent->scrollData().value = item.scrollOptions.axes == ScrollAxes::Horizontal ? updated.x : updated.y;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::routeInteractionWheel()
    {
        if(input.wheel.x == 0.f && input.wheel.y == 0.f)
        {
            return;
        }

        const PointerState * pointer = input.primaryPointer();

        if(pointer == nullptr)
        {
            return;
        }

        if(captured != InvalidId && pointer->isDown() == true)
        {
            return;
        }

        auto apply = [this, pointer](InteractionSnapshotItem & item)
        {
            bool multilineText = item.kind == Detail::NodeKind::InputMultiline;
            bool scrollContainer = item.kind == Detail::NodeKind::Scroll;
            bool scrollTable = item.kind == Detail::NodeKind::Table && (item.tableOptions.scrollHorizontal || item.tableOptions.scrollVertical);

            if(multilineText == false && scrollContainer == false && scrollTable == false)
            {
                return false;
            }

            if(item.visible == false || item.disabled == true || item.inputBlocked == true)
            {
                return false;
            }

            Id blockedLayer = blockingInputLayer != InvalidId ? blockingInputLayer : previousBlockingInputLayer;

            if(blockedLayer != InvalidId && item.inputLayer != blockedLayer && Detail::popupOwnerCanInteract(this, item.item.id) == false)
            {
                return false;
            }

            bool matchingWindow = pointerWindow == InvalidId || (item.windowOwner == pointerWindow && (pointerWindowSubmission == 0 || item.windowSubmission == pointerWindowSubmission));

            if(matchingWindow == false)
            {
                return false;
            }

            if(item.bounds.empty() == true || item.clip.empty() == true)
            {
                return false;
            }

            if(item.bounds.contains(pointer->position) == false || item.clip.contains(pointer->position) == false)
            {
                return false;
            }

            wheelOwner = item.item.id;
            wheelOwnerItem = item.item;
            wheelOwnerTimestamp = input.timestamp;

            if(multilineText == true)
            {
                Detail::TextEditorState & editorState = textEditorState(item.item.id);
                float maximumY = std::max(0.f, item.textSize.y - item.textVisibleHeight);
                editorState.textScrollY = std::clamp(editorState.textScrollY - input.wheel.y, 0.f, maximumY);
                frame.events.push_back({EventType::Change, item.item.id, {}, {}, 0, input.timestamp});

                return true;
            }

            Persistent & persistent = state(item.item.id);

            if(persistent.scrollData().targetInitialized == false)
            {
                persistent.scrollData().target = persistent.scrollData().position;
                persistent.scrollData().targetInitialized = true;
            }

            bool horizontal = scrollTable ? item.tableOptions.scrollHorizontal : item.scrollOptions.axes == ScrollAxes::Horizontal || item.scrollOptions.axes == ScrollAxes::Both;
            bool vertical = scrollTable ? item.tableOptions.scrollVertical : item.scrollOptions.axes == ScrollAxes::Vertical || item.scrollOptions.axes == ScrollAxes::Both;
            float deltaX = horizontal ? input.wheel.x * item.scrollOptions.wheelStep : 0.f;
            float deltaY = vertical ? input.wheel.y * item.scrollOptions.wheelStep : 0.f;

            if(horizontal == true && deltaX == 0.f && (vertical == false || input.modifiers.shift == true))
            {
                deltaX = input.wheel.y * item.scrollOptions.wheelStep;

                if(input.modifiers.shift == true)
                {
                    deltaY = 0.f;
                }
            }

            Vec2 updated;
            updated.x = std::clamp(persistent.scrollData().target.x - deltaX, 0.f, persistent.scrollData().range.x);
            updated.y = std::clamp(persistent.scrollData().target.y - deltaY, 0.f, persistent.scrollData().range.y);

            if(updated == persistent.scrollData().target)
            {
                return true;
            }

            persistent.scrollData().target = updated;
            bool smooth = item.scrollOptions.smooth && item.scrollOptions.smoothDuration > 0.f && item.animationsEnabled;

            if(smooth == false)
            {
                persistent.scrollData().position = updated;
                persistent.scrollData().velocity = {};
                persistent.scrollData().value = item.scrollOptions.axes == ScrollAxes::Horizontal ? updated.x : updated.y;
            }

            frame.events.push_back({EventType::Change, item.item.id, {}, {}, 0, input.timestamp});

            return true;
        };

        constexpr double wheelOwnershipTimeout = 0.35;

        if(wheelOwnerItem.valid() == true && input.timestamp - wheelOwnerTimestamp <= wheelOwnershipTimeout)
        {
            for(InteractionSnapshotItem & item : interactionSnapshot)
            {
                if(item.item == wheelOwnerItem && apply(item) == true)
                {
                    return;
                }
            }
        }

        wheelOwner = InvalidId;
        wheelOwnerItem = {};
        for(size_t index = interactionSnapshot.size(); index != 0; --index)
        {
            if(apply(interactionSnapshot[index - 1]) == true)
            {
                return;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Context::TableState & Context::tableState(Id id)
    {
        if(const Node * node = findFrameNode(id); node != nullptr && node->kind == Detail::NodeKind::Table && node->tableSettingsId() != InvalidId)
        {
            id = node->tableSettingsId();
        }

        Persistent & persistentState = state(id);

        if(persistentState.table == nullptr)
        {
            persistentState.table = makeUnique<TableState>(*allocator);
        }

        return *persistentState.table;
    }
    //////////////////////////////////////////////////////////////////////////
    const Context::TableState * Context::findTableState(Id id) const noexcept
    {
        if(const Node * node = findFrameNode(id); node != nullptr && node->kind == Detail::NodeKind::Table && node->tableSettingsId() != InvalidId)
        {
            id = node->tableSettingsId();
        }

        const Persistent * persistentState = findState(id);
        auto returnedValue = persistentState == nullptr ? nullptr : persistentState->table.get();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    const Theme * Context::internStyle(const Theme & value)
    {
        if(&value == &frameTheme)
        {
            return &frameTheme;
        }

        if(value == frameTheme)
        {
            return &frameTheme;
        }

        constexpr size_t minimumCapacity = 64;
        ThemeHash hasher;

        if(frameStyleIndices.empty() == true || (frameStyleCount + 1) * 2 > frameStyleIndices.size())
        {
            size_t capacity = frameStyleIndices.empty() == true ? minimumCapacity : frameStyleIndices.size() * 2;
            frameStyleIndices.assign(capacity, {});
            size_t mask = capacity - 1;
            for(size_t index = 0; index != frameStyleCount; ++index)
            {
                size_t styleHash = hasher(*frameStyles[index]);
                size_t slot = styleHash & mask;
                while(frameStyleIndices[slot].frame == frame.number)
                {
                    slot = (slot + 1) & mask;
                }
                frameStyleIndices[slot] = {styleHash, index, frame.number};
            }
        }

        size_t valueHash = hasher(value);
        size_t mask = frameStyleIndices.size() - 1;
        size_t slot = valueHash & mask;
        while(frameStyleIndices[slot].frame == frame.number)
        {
            const ThemeIndexEntry & entry = frameStyleIndices[slot];

            if(entry.hash == valueHash && *frameStyles[entry.index] == value)
            {
                auto returnedValue = frameStyles[entry.index].get();

                return returnedValue;
            }

            slot = (slot + 1) & mask;
        }

        if(frameStyleCount == frameStyles.size())
        {
            frameStyles.push_back(makeUnique<Theme>(*allocator, value));
        }
        else
        {
            *frameStyles[frameStyleCount] = value;
        }

        size_t index = frameStyleCount++;
        frameStyleIndices[slot] = {valueHash, index, frame.number};
        auto returnedValue = frameStyles[index].get();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Theme & Context::mutableStyle(Node & node)
    {
        if(frameStyleCount == frameStyles.size())
        {
            frameStyles.push_back(makeUnique<Theme>(*allocator, *node.style));
        }
        else
        {
            *frameStyles[frameStyleCount] = *node.style;
        }

        Theme * style = frameStyles[frameStyleCount++].get();
        node.style = style;

        return *style;
    }
    //////////////////////////////////////////////////////////////////////////
    float Context::gap(const Node & node) const noexcept
    {
        return node.layout.gap >= 0.f ? node.layout.gap : node.style->metrics.gap;
    }
    //////////////////////////////////////////////////////////////////////////
    Vec2 Context::estimateText(StringView value, const Theme & nodeStyle) const noexcept
    {
        Vec2 result = measureText(value, nodeStyle);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    Vec2 Context::measureText(StringView value, const Theme & nodeStyle) const noexcept
    {
        if(fontProvider != nullptr)
        {
            uint64_t revision = fontProvider->revision();
            uint32_t fontSize = Detail::quantizeFontSize(nodeStyle.metrics.fontSize);
            uint64_t key = hashBytes(value);
            key = combineId(key, reinterpret_cast<uintptr_t>(fontProvider));
            key = combineId(key, revision);
            key = combineId(key, nodeStyle.metrics.font);
            key = combineId(key, fontSize);
            TextMeasureCacheEntry & entry = textMeasureCache[static_cast<size_t>(key) & (textMeasureCache.size() - 1U)];
            bool matches = entry.key == key;
            matches = matches == true && entry.provider == fontProvider;
            matches = matches == true && entry.providerRevision == revision;
            matches = matches == true && entry.font == nodeStyle.metrics.font;
            matches = matches == true && entry.fontSize == fontSize;
            matches = matches == true && entry.text == value;

            if(matches == true)
            {
                ++frameTextMeasureCacheHits;

                return entry.size;
            }

            Vec2 measured;

            if(fontProvider->measure(nodeStyle.metrics.font, nodeStyle.metrics.fontSize, value, &measured) == true)
            {
                entry.text.assign(value);
                entry.size = measured;
                entry.provider = fontProvider;
                entry.providerRevision = revision;
                entry.font = nodeStyle.metrics.font;
                entry.fontSize = fontSize;
                entry.key = key;
                ++frameTextMeasureCacheMisses;

                return measured;
            }
        }

        size_t longest = 0;
        size_t current = 0;
        size_t lines = 1;
        for(char character : value)
        {
            if(character == '\n')
            {
                longest = std::max(longest, current);
                current = 0;
                ++lines;
            }
            else if((static_cast<unsigned char>(character) & 0xc0U) != 0x80U)
            {
                ++current;
            }
        }
        longest = std::max(longest, current);
        float width = static_cast<float>(longest) * nodeStyle.metrics.fontSize * 0.56f;
        float height = static_cast<float>(lines) * nodeStyle.metrics.lineHeight;
        Vec2 result = {width, height};

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::clearTextCache() noexcept
    {
        textCache.clear();
        textCacheOldest = nullptr;
        textCacheNewest = nullptr;
        transientText.clear();
        transientTextCount = 0;
        transientTextIndices.clear();
        transientTextIndexCount = 0;
        textUseHistory.fill({});
        textCacheEntryCount = 0;
        textCacheMemory = 0;
        for(TextMeasureCacheEntry & entry : textMeasureCache)
        {
            entry.text.clear();
            entry.provider = nullptr;
            entry.providerRevision = 0;
            entry.font = DefaultFont;
            entry.fontSize = 0;
            entry.key = 0;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::unlinkCachedText(CachedText & text) noexcept
    {
        if(text.previous != nullptr)
        {
            text.previous->next = text.next;
        }
        else if(textCacheOldest == &text)
        {
            textCacheOldest = text.next;
        }

        if(text.next != nullptr)
        {
            text.next->previous = text.previous;
        }
        else if(textCacheNewest == &text)
        {
            textCacheNewest = text.previous;
        }

        text.previous = nullptr;
        text.next = nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::touchCachedText(CachedText & text) noexcept
    {
        if(textCacheNewest == &text)
        {
            return;
        }

        if(text.previous != nullptr || text.next != nullptr || textCacheOldest == &text)
        {
            unlinkCachedText(text);
        }

        text.previous = textCacheNewest;

        if(textCacheNewest != nullptr)
        {
            textCacheNewest->next = &text;
        }
        else
        {
            textCacheOldest = &text;
        }

        textCacheNewest = &text;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::syncTextCache()
    {
        uint64_t revision = fontProvider == nullptr ? 0 : fontProvider->revision();

        if(textCacheProvider == fontProvider && textCacheProviderRevision == revision)
        {
            return;
        }

        clearTextCache();
        textCacheProvider = fontProvider;
        textCacheProviderRevision = revision;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::trimTextCache(size_t maximumEntries, size_t maximumMemory)
    {
        while(textCacheEntryCount > maximumEntries || textCacheMemory > maximumMemory)
        {
            CachedText * oldest = textCacheOldest;

            if(oldest == nullptr)
            {
                return;
            }

            if(oldest->lastFrame >= frame.number)
            {
                return;
            }

            auto bucket = textCache.find(oldest->key);

            if(bucket == textCache.end())
            {
                return;
            }

            auto entry = std::find_if(bucket->second.begin(), bucket->second.end(),
                                            [oldest](const CachedTextPtr & candidate)
                                            {
                                                auto returnedValue = candidate.get() == oldest;

                                                return returnedValue;
                                            });

            if(entry == bucket->second.end())
            {
                return;
            }

            textCacheMemory -= std::min(textCacheMemory, Detail::cachedTextMemory(**entry));
            unlinkCachedText(**entry);
            bucket->second.erase(entry);
            --textCacheEntryCount;
            ++frameTextCacheEvictions;

            if(bucket->second.empty() == true)
            {
                textCache.erase(bucket);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Context::CachedText * Context::findTransientText(uint64_t key, const TextCacheAttributes & attributes, StringView value) noexcept
    {
        if(transientTextIndices.empty() == true)
        {
            return nullptr;
        }

        size_t mask = transientTextIndices.size() - 1;
        size_t slot = static_cast<size_t>(key) & mask;
        for(size_t probe = 0; probe != transientTextIndices.size(); ++probe)
        {
            const TransientTextIndexEntry & entry = transientTextIndices[slot];

            if(entry.frame != frame.number)
            {
                return nullptr;
            }

            if(entry.key == key && entry.index < transientTextCount)
            {
                CachedText * candidate = transientText[entry.index].get();

                if(candidate->attributes == attributes && candidate->text == value)
                {
                    return candidate;
                }
            }

            slot = (slot + 1) & mask;
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::indexTransientText(uint64_t key, size_t index)
    {
        bool resizeIndex = transientTextIndices.empty() == true;

        if((transientTextIndexCount + 1) * 2 >= transientTextIndices.size())
        {
            resizeIndex = true;
        }

        if(resizeIndex == true)
        {
            size_t capacity = transientTextIndices.empty() == true ? 128 : transientTextIndices.size() * 2;
            transientTextIndices.resize(capacity);
            for(TransientTextIndexEntry & entry : transientTextIndices)
            {
                entry.frame = 0;
            }
            transientTextIndexCount = 0;
            for(size_t transientIndex = 0; transientIndex != transientTextCount; ++transientIndex)
            {
                CachedText * candidate = transientText[transientIndex].get();
                size_t mask = transientTextIndices.size() - 1;
                size_t slot = static_cast<size_t>(candidate->key) & mask;
                while(transientTextIndices[slot].frame == frame.number)
                {
                    slot = (slot + 1) & mask;
                }
                transientTextIndices[slot] = {candidate->key, frame.number, transientIndex};
                ++transientTextIndexCount;
            }

            return;
        }

        size_t mask = transientTextIndices.size() - 1;
        size_t slot = static_cast<size_t>(key) & mask;
        while(transientTextIndices[slot].frame == frame.number)
        {
            slot = (slot + 1) & mask;
        }
        transientTextIndices[slot] = {key, frame.number, index};
        ++transientTextIndexCount;
    }
    //////////////////////////////////////////////////////////////////////////
    const Context::CachedText * Context::findOrCreateText(StringView value, const Theme & nodeStyle, bool editable, bool wordWrap, float wrapWidth)
    {
        if(fontProvider == nullptr)
        {
            return nullptr;
        }

        if(value.empty() == true)
        {
            return nullptr;
        }

        syncTextCache();
        TextCacheAttributes attributes = {fontProvider, textCacheProviderRevision, nodeStyle.metrics.font, Detail::quantizeFontSize(nodeStyle.metrics.fontSize), static_cast<uint32_t>(std::max(0.f, std::round(wrapWidth * 64.f))), editable, wordWrap};
        uint64_t key = hashBytes(value);
        key = combineId(key, reinterpret_cast<uintptr_t>(attributes.provider));
        key = combineId(key, attributes.providerRevision);
        key = combineId(key, attributes.font);
        key = combineId(key, attributes.fontSize);
        key = combineId(key, attributes.wrapWidth);
        key = combineId(key, attributes.editable ? 1U : 0U);
        key = combineId(key, attributes.wordWrap ? 1U : 0U);

        auto cachedBucket = textCache.find(key);

        if(cachedBucket != textCache.end())
        {
            for(const CachedTextPtr & cached : cachedBucket->second)
            {
                if(cached->attributes == attributes && cached->text == value)
                {
                    cached->lastFrame = frame.number;
                    touchCachedText(*cached);
                    ++frameTextCacheHits;
                    auto returnedValue = cached.get();

                    return returnedValue;
                }
            }
        }

        if(CachedText * transient = findTransientText(key, attributes, value); transient != nullptr)
        {
            ++frameTextCacheHits;

            return transient;
        }

        ++frameTextCacheMisses;
        bool composableNumber = Detail::isComposableNumber(value, attributes);
        bool transientOnly = false;

        if(composableNumber == true)
        {
            TextUseHistory & history = textUseHistory[static_cast<size_t>(key) & (textUseHistory.size() - 1)];

            if(history.key != key || history.lastFrame + 120 < frame.number)
            {
                history = {key, 0, 0};
            }

            if(history.lastFrame != frame.number)
            {
                history.uses = static_cast<uint8_t>(std::min<uint32_t>(static_cast<uint32_t>(history.uses) + 1, 255));
            }

            history.lastFrame = frame.number;
            transientOnly = history.uses < 2;
        }

        CachedTextPtr cachedOwner;
        CachedText * cached = nullptr;
        size_t transientIndex = std::numeric_limits<size_t>::max();

        if(transientOnly == true)
        {
            if(transientTextCount == transientText.size())
            {
                transientText.push_back(makeUnique<CachedText>(*allocator));
            }

            transientIndex = transientTextCount++;
            cached = transientText[transientIndex].get();
            Detail::resetTransientText(*cached);
        }
        else
        {
            cachedOwner = makeUnique<CachedText>(*allocator);
            cached = cachedOwner.get();
        }

        cached->attributes = attributes;
        cached->text.assign(value.data(), value.size());
        cached->key = key;
        cached->lastFrame = frame.number;
        auto discardText = [&]() -> const CachedText *
        {
            if(transientOnly == true && transientIndex + 1 == transientTextCount)
            {
                Detail::resetTransientText(*cached);
                --transientTextCount;
            }

            return nullptr;
        };

        bool composed = false;

        if(composableNumber == true)
        {
            float cursor = 0.f;
            float height = 0.f;
            composed = true;
            for(size_t index = 0; index != value.size(); ++index)
            {
                const CachedText * character = findOrCreateText(value.substr(index, 1), nodeStyle, false);

                if(character == nullptr)
                {
                    composed = false;
                    cached->batches.clear();
                    break;
                }

                for(const PreparedTextRunBatch & source : character->batches)
                {
                    bool appendBatch = cached->batches.empty() == true;

                    if(appendBatch == false)
                    {
                        const PreparedTextRunBatch & lastBatch = cached->batches.back();
                        appendBatch = lastBatch.texture != source.texture;
                    }

                    if(appendBatch == true)
                    {
                        PreparedTextRunBatch batch;
                        batch.texture = source.texture;
                        cached->batches.push_back(std::move(batch));
                    }

                    PreparedTextRunBatch & destination = cached->batches.back();

                    if(destination.rectangles.size() > std::numeric_limits<uint32_t>::max() - source.rectangles.size())
                    {
                        auto returnedValue = discardText();

                        return returnedValue;
                    }

                    destination.rectangles.reserve(destination.rectangles.size() + source.rectangles.size());
                    for(const TexturedRectInstance & sourceRectangle : source.rectangles)
                    {
                        TexturedRectInstance translated = sourceRectangle;
                        translated.bounds.x = Detail::snapToPixel(translated.bounds.x + cursor, viewport.dpiScale);
                        translated.bounds.y = Detail::snapToPixel(translated.bounds.y, viewport.dpiScale);
                        destination.rectangles.push_back(translated);
                    }
                }
                cursor += character->size.x;
                height = std::max(height, character->size.y);
            }

            if(composed == true)
            {
                cached->size = {cursor, height};
                cached->clusters.reserve(value.size() + 1);
                for(size_t index = 0; index <= value.size(); ++index)
                {
                    cached->clusters.push_back(index);
                }
            }
        }

        if(composed == false)
        {
            ShapedText shaped;
            TextShapeOptions shapeOptions;
            shapeOptions.caretOffsets = editable;
            shapeOptions.wordWrap = wordWrap;
            shapeOptions.wrapWidth = static_cast<float>(attributes.wrapWidth) / 64.f;

            if(fontProvider->shapeText(nodeStyle.metrics.font, nodeStyle.metrics.fontSize, value, shapeOptions, &shaped) == false)
            {
                auto returnedValue = discardText();

                return returnedValue;
            }

            cached->size = shaped.size;
            cached->offsets = std::move(shaped.offsets);
            cached->clusters = std::move(shaped.clusters);
            cached->positions = std::move(shaped.positions);
            cached->lines = std::move(shaped.lines);

            if(cached->clusters.empty() == true)
            {
                cached->clusters.push_back(0);
                cached->clusters.push_back(value.size());
            }

            for(const ShapedGlyph & placement : shaped.glyphs)
            {
                Glyph glyph;
                if(fontProvider->getGlyph(nodeStyle.metrics.font, nodeStyle.metrics.fontSize, placement, &glyph) == false)
                {
                    auto returnedValue = discardText();

                    return returnedValue;
                }

                bool appendBatch = cached->batches.empty() == true;

                if(appendBatch == false)
                {
                    const PreparedTextRunBatch & lastBatch = cached->batches.back();
                    appendBatch = lastBatch.texture != glyph.texture;
                }

                if(appendBatch == true)
                {
                    PreparedTextRunBatch batch;
                    batch.texture = glyph.texture;
                    cached->batches.push_back(std::move(batch));
                }

                PreparedTextRunBatch & batch = cached->batches.back();

                if(batch.rectangles.size() == std::numeric_limits<uint32_t>::max())
                {
                    auto returnedValue = discardText();

                    return returnedValue;
                }

                float left = Detail::snapToPixel(placement.position.x + glyph.bearing.x, viewport.dpiScale);
                float top = Detail::snapToPixel(placement.position.y + glyph.bearing.y, viewport.dpiScale);
                float right = Detail::snapToPixel(placement.position.x + glyph.bearing.x + glyph.size.x, viewport.dpiScale);
                float bottom = Detail::snapToPixel(placement.position.y + glyph.bearing.y + glyph.size.y, viewport.dpiScale);
                constexpr Color white = {1.f, 1.f, 1.f, 1.f};
                TexturedRectInstance rectangle = {{left, top, right - left, bottom - top}, glyph.uv, white};
                batch.rectangles.push_back(rectangle);
            }
        }

        if(transientOnly == true)
        {
            indexTransientText(key, transientIndex);

            return cached;
        }

        size_t cachedMemory = Detail::cachedTextMemory(*cached);

        if(textCacheEntryCount >= Detail::MaximumTextCacheEntries || textCacheMemory + cachedMemory > Detail::MaximumTextCacheMemory)
        {
            trimTextCache(Detail::MaximumTextCacheEntries - 1, Detail::MaximumTextCacheMemory - std::min(cachedMemory, Detail::MaximumTextCacheMemory));
        }

        auto [bucket, inserted] = textCache.try_emplace(key);
        (void)inserted;
        CachedTextPtrVector & cachedTexts = bucket->second;
        cachedTexts.push_back(std::move(cachedOwner));
        CachedTextPtr & lastCachedText = cachedTexts.back();
        touchCachedText(*lastCachedText);
        ++textCacheEntryCount;
        textCacheMemory += cachedMemory;
        CachedText * returnedValue = lastCachedText.get();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::estimateNodeText(Node & node, StringView value)
    {
        node.textSize = estimateText(value, *node.style);
        node.textRun = nullptr;
        node.textPrepared = value.empty() == true;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::prepareText(Node & node, StringView value)
    {
        node.textPrepared = true;
        bool editable = node.kind == Detail::NodeKind::InputText || node.kind == Detail::NodeKind::InputMultiline;
        float wrapWidth = node.valueData().textWrapWidth;

        if(node.wordWrap == true && wrapWidth <= 0.f)
        {
            const Rect & textBounds = node.bounds.empty() == true ? state(node).lastBounds : node.bounds;
            wrapWidth = std::max(0.f, textBounds.width - node.style->metrics.padding * (node.textEditData().multiline ? 2.f : 0.f) - node.style->metrics.frameBorderSize * (node.textEditData().multiline ? 2.f : 0.f));
        }

        node.textRun = findOrCreateText(value, *node.style, editable, node.wordWrap, wrapWidth);
        node.textSize = node.textRun == nullptr ? measureText(value, *node.style) : node.textRun->size;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::prepareValueText(Node & node, StringView value)
    {
        node.valueText.assign(value);
        Theme valueStyle = *node.style;
        valueStyle.metrics.font = MonospaceFont;
        node.valueData().valueTextSize = estimateText(value, valueStyle);
        node.valueTextRun = nullptr;
        node.valueTextPrepared = value.empty() == true;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::shapeValueText(Node & node)
    {
        node.valueTextPrepared = true;
        Theme valueStyle = *node.style;
        valueStyle.metrics.font = MonospaceFont;
        node.valueTextRun = findOrCreateText(node.valueText, valueStyle, false);
        node.valueData().valueTextSize = node.valueTextRun == nullptr ? measureText(node.valueText, valueStyle) : node.valueTextRun->size;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::prepareColorChannelText(Node & node, size_t channel, StringView value)
    {
        if(channel >= 4)
        {
            return;
        }

        ColorTextData & colorText = ensureColorTextData(node);
        Theme valueStyle = *node.style;
        valueStyle.metrics.font = MonospaceFont;
        colorText.text[channel].assign(value);
        colorText.sizes[channel] = estimateText(value, valueStyle);
        colorText.runs[channel] = nullptr;
        colorText.prepared[channel] = value.empty() == true;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::shapeColorChannelText(Node & node, size_t channel)
    {
        ColorTextData * colorText = node.valueData().colorTextDataIndex < frameColorTextData.size() ? &frameColorTextData[node.valueData().colorTextDataIndex] : nullptr;

        if(colorText == nullptr)
        {
            return;
        }

        if(channel >= colorText->runs.size())
        {
            return;
        }

        colorText->prepared[channel] = true;
        Theme valueStyle = *node.style;
        valueStyle.metrics.font = MonospaceFont;
        colorText->runs[channel] = findOrCreateText(colorText->text[channel], valueStyle, false);
        colorText->sizes[channel] = colorText->runs[channel] == nullptr ? measureText(colorText->text[channel], valueStyle) : colorText->runs[channel]->size;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::prepareVisibleText()
    {
        for(size_t index = 1; index < nodes.size(); ++index)
        {
            Node & node = nodes[index];
            bool intersectsFinalClip = node.visible && node.clip.empty() == false;

            if(intersectsFinalClip == false && Detail::shouldPrepareNodeText(this, index) == false)
            {
                continue;
            }

            if(node.textPrepared == false)
            {
                prepareText(node, node.label);
            }

            if(node.valueTextPrepared == false)
            {
                shapeValueText(node);
            }

            ColorTextData * colorText = node.valueData().colorTextDataIndex < frameColorTextData.size() ? &frameColorTextData[node.valueData().colorTextDataIndex] : nullptr;

            if(colorText == nullptr)
            {
                continue;
            }

            for(size_t channel = 0; channel < colorText->prepared.size(); ++channel)
            {
                if(colorText->prepared[channel])
                {
                    continue;
                }

                shapeColorChannelText(node, channel);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Id Context::localId(Detail::NodeKind kind, const Key & key, const SourceLocation & location, bool anonymous)
    {
        if(anonymous == true)
        {
            auto returnedValue = combineId(hashBytes(Detail::nodeKindName(kind)), anonymousCounter++);

            return returnedValue;
        }

        if(key.isExplicit() == true)
        {
            auto returnedValue = key.value();

            return returnedValue;
        }

        // Display text may be backed by a mutable application buffer. It must never become
        // part of persistent identity. Nodes without an explicit key use their static call
        // site; repeated dynamic siblings must provide a Key or a keyed parent scope.
        Id value = hashBytes(location.file_name());
        value = combineId(value, static_cast<Id>(location.line()));
        value = combineId(value, static_cast<Id>(location.column()));
        value = combineId(value, hashBytes(Detail::nodeKindName(kind)));

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    size_t Context::addNode(Detail::NodeKind kind, const Key & key, StringView label, const LayoutOptions & layout, const SourceLocation & location, SemanticRole semanticRole, bool focusable, bool anonymous)
    {
        Node node = acquireNode();
        node.kind = kind;
        node.debugPayload = frameArena.make<NodeDebugPayload>();

        if(node.debugPayload == nullptr)
        {
            node.debugPayload = &fallbackNodeDebugPayload;
            frame.diagnostics.emplace_back("Frame arena could not allocate node debug payload");
        }

        node.valuePayload = frameArena.make<ValueNodePayload>();

        if(node.valuePayload == nullptr)
        {
            node.valuePayload = &fallbackValueNodePayload;
            frame.diagnostics.emplace_back("Frame arena could not allocate node value payload");
        }

        if(kind == Detail::NodeKind::Window || kind == Detail::NodeKind::Scroll || kind == Detail::NodeKind::Table)
        {
            node.scrollPayload = frameArena.make<ScrollNodePayload>();

            if(node.scrollPayload == nullptr)
            {
                node.scrollPayload = &fallbackScrollNodePayload;
                frame.diagnostics.emplace_back("Frame arena could not allocate scroll node payload");
            }
        }

        if(kind == Detail::NodeKind::Window)
        {
            node.windowPayload = frameArena.make<WindowNodePayload>();

            if(node.windowPayload == nullptr)
            {
                node.windowPayload = &fallbackWindowNodePayload;
                frame.diagnostics.emplace_back("Frame arena could not allocate window node payload");
            }
        }

        if(kind == Detail::NodeKind::IconButton || kind == Detail::NodeKind::Image || kind == Detail::NodeKind::ImageButton)
        {
            node.imagePayload = frameArena.make<ImageNodePayload>();

            if(node.imagePayload == nullptr)
            {
                node.imagePayload = &fallbackImageNodePayload;
                frame.diagnostics.emplace_back("Frame arena could not allocate image node payload");
            }
        }

        if(kind == Detail::NodeKind::Tree)
        {
            node.treePayload = frameArena.make<TreeNodePayload>();

            if(node.treePayload == nullptr)
            {
                node.treePayload = &fallbackTreeNodePayload;
                frame.diagnostics.emplace_back("Frame arena could not allocate tree node payload");
            }
        }

        if(kind == Detail::NodeKind::InputText || kind == Detail::NodeKind::InputMultiline || kind == Detail::NodeKind::Slider || kind == Detail::NodeKind::DragValue)
        {
            node.textEditPayload = frameArena.make<TextEditNodePayload>();

            if(node.textEditPayload == nullptr)
            {
                node.textEditPayload = &fallbackTextEditNodePayload;
                frame.diagnostics.emplace_back("Frame arena could not allocate text edit node payload");
            }
        }

        if(kind == Detail::NodeKind::Table)
        {
            node.tablePayload = frameArena.make<TableNodePayload>();

            if(node.tablePayload == nullptr)
            {
                node.tablePayload = &fallbackTableNodePayload;
                frame.diagnostics.emplace_back("Frame arena could not allocate table node payload");
            }
        }

        if(kind == Detail::NodeKind::NativeSurface)
        {
            node.nativeSurfacePayload = frameArena.make<NativeSurfaceNodePayload>();

            if(node.nativeSurfacePayload == nullptr)
            {
                node.nativeSurfacePayload = &fallbackNativeSurfaceNodePayload;
                frame.diagnostics.emplace_back("Frame arena could not allocate native surface node payload");
            }
        }

        if(nodes[currentParent].kind == Detail::NodeKind::Table)
        {
            node.tableItemPayload = frameArena.make<TableItemNodePayload>();

            if(node.tableItemPayload == nullptr)
            {
                node.tableItemPayload = &fallbackTableItemNodePayload;
                frame.diagnostics.emplace_back("Frame arena could not allocate table item payload");
            }
        }

        node.parent = currentParent;
        node.parentId = nodes[currentParent].id;
        // Windows have a global identity just like Mosaic windows: moving a panel between
        // a split container, a dock node and the floating root must not change its persistent id.
        Id identityParent = kind == Detail::NodeKind::Window ? RootId : nodes[currentParent].identityScope;

        if(kind != Detail::NodeKind::TableRow && nodes[currentParent].kind == Detail::NodeKind::Table)
        {
            const TableState & parentTable = tableState(nodes[currentParent].id);

            if(parentTable.currentRowIdentity != InvalidId)
            {
                identityParent = parentTable.currentRowIdentity;
            }
        }

        Id identityLocal = localId(kind, key, location, anonymous);
        node.id = combineId(identityParent, identityLocal);
        node.identityScope = anonymous == true ? identityParent : node.id;
        node.identityParent = identityParent;
        node.identityLocal = identityLocal;
        node.anonymousIdentity = anonymous;

        if(key.isExplicit() == true)
        {
            if(key.hasIntegralDebug() == true)
            {
                node.mutableDebugData().identityValueKind = IdentityValueKind::Integral;
                node.mutableDebugData().identityIntegral = key.integralDebug();
            }
            else
            {
                if(key.debug().empty() == true)
                {
                    node.mutableDebugData().identityValueKind = IdentityValueKind::Hash;
                }
                else
                {
                    node.mutableDebugData().identityValueKind = IdentityValueKind::String;
                    FrameNodeStrings & strings = ensureNodeStrings(node);
                    strings.identityDebugValue.assign(key.debug());
                }
            }
        }

        node.label.assign(label);
        node.layout = layout;
        Node & parentNode = nodes[currentParent];

        if(parentNode.nextSameLinePending == true)
        {
            node.sameLine = true;
            node.sameLineOffset = std::max(0.f, parentNode.nextSameLine.offset);
            node.sameLineSpacing = parentNode.nextSameLine.spacing;
            parentNode.nextSameLinePending = false;
        }

        if(parentNode.nextTextAlignToFramePadding == true)
        {
            bool textNode = kind == Detail::NodeKind::Text || kind == Detail::NodeKind::Bullet || kind == Detail::NodeKind::BulletText;

            if(textNode == true)
            {
                node.alignTextToFramePadding = true;
                parentNode.nextTextAlignToFramePadding = false;
            }
        }

        if(Detail::usesItemWidth(kind) == true)
        {
            if(nextItemWidthPending == true)
            {
                node.valueData().itemWidth = nextItemWidth;
                node.valueData().itemWidthRequested = true;
                nextItemWidthPending = false;
            }
            else if(itemWidthStack.empty() == false)
            {
                node.valueData().itemWidth = itemWidthStack.back();
                node.valueData().itemWidthRequested = true;
            }
        }

        node.style = currentStyle;
        node.inputLayer = currentInputLayer;
        node.windowOwner = currentWindow;
        node.windowSubmission = currentWindowSubmission;
        node.item.id = node.id;
        node.item.submission = node.windowSubmission;
        for(const Node & existingNode : nodes)
        {
            if(existingNode.item.id == node.item.id && existingNode.item.submission == node.item.submission)
            {
                ++node.item.occurrence;
            }
        }
        estimateNodeText(node, node.label);
        node.disabled = currentDisabled;
        node.inputBlocked = currentInputBlocked;
        node.navigationBlocked = currentNavigationBlocked;
        node.semanticRole = semanticRole;
        node.focusable = focusable && currentDisabled == false && currentInputBlocked == false;
        bool programmaticFocusable = node.focusable || node.semanticRole == SemanticRole::TextField;

        if(programmaticFocusable == true && node.disabled == false && node.inputBlocked == false && focusNextPending == true)
        {
            if(focusNextOffset <= 0)
            {
                focused = node.id;
                navigationFocused = node.id;
                focusNextPending = false;
            }
            else
            {
                --focusNextOffset;
            }
        }

        node.mutableDebugData().file = location.file_name();
        node.mutableDebugData().function = location.function_name();
        node.mutableDebugData().line = location.line();

        if(nodes[currentParent].kind == Detail::NodeKind::Table)
        {
            TableState & tableState = this->tableState(nodes[currentParent].id);
            uint32_t columnCount = std::max(1U, nodes[currentParent].layout.columns);
            TableItemNodePayload & tableItem = node.mutableTableItem();
            tableItem.row = tableState.currentRow;
            tableItem.column = std::min(tableState.currentColumn, columnCount - 1);

            if(tableItem.column < tableState.columns.size())
            {
                tableItem.columnOptions = tableState.columns[tableItem.column].options;

                if(Detail::usesItemWidth(kind) == true && node.valueData().itemWidthRequested == false && tableItem.columnOptions.itemWidth != 0.f)
                {
                    node.valueData().itemWidth = tableItem.columnOptions.itemWidth;
                    node.valueData().itemWidthRequested = true;
                }
            }

            tableItem.rowBackgrounds = tableState.currentRowBackgrounds;
            tableItem.rowBackgroundsEnabled = tableState.currentRowBackgroundsEnabled;
            tableItem.cellBackground = tableState.pendingCellBackground;
            tableItem.cellBackgroundEnabled = tableState.pendingCellBackgroundEnabled;
            tableState.pendingCellBackgroundEnabled = false;
            ++tableState.currentColumn;

            if(tableState.currentColumn >= columnCount)
            {
                tableState.currentColumn = 0;
                ++tableState.currentRow;
                tableState.currentRowIdentity = InvalidId;
            }
        }

        if(Detail::hasInsets(node.layout.padding) == false && Detail::isContainer(kind) == true)
        {
            if(kind == Detail::NodeKind::Window)
            {
                node.layout.padding = EdgeInsets(currentStyle->metrics.padding);
            }
        }

        size_t index = nodes.size();
        size_t existing = findFrameNodeIndex(node.id);

        nodes.emplace_back(std::move(node));

        bool duplicateIdentity = existing < index;

        if(duplicateIdentity == true)
        {
            duplicateIdentity = nodes[existing].anonymousIdentity == false && nodes[index].anonymousIdentity == false;
        }

        if(duplicateIdentity == true)
        {
            duplicateIdentity = nodes[existing].identityParent == nodes[index].identityParent;
        }

        if(duplicateIdentity == true)
        {
            duplicateIdentity = nodes[existing].identityLocal == nodes[index].identityLocal;
        }

        if(duplicateIdentity == true && nodes[existing].kind == Detail::NodeKind::Window && nodes[index].kind == Detail::NodeKind::Window)
        {
            duplicateIdentity = false;
        }

        if(duplicateIdentity == true && nodes[existing].windowOwner != InvalidId && nodes[existing].windowOwner == nodes[index].windowOwner && nodes[existing].windowSubmission != nodes[index].windowSubmission)
        {
            duplicateIdentity = false;
        }

        if(duplicateIdentity == true)
        {
            nodes[existing].idConflict = true;
            nodes[index].idConflict = true;

            // nodePath() may grow frameNodeStrings. Keep owned copies so the
            // second lookup cannot invalidate the first diagnostic path.
            String existingPath(nodePath(existing));
            String addedPath(nodePath(index));
            String identifier;
            if(Detail::formatIntegral(nodes[index].id, "0x%016x", &identifier) == false)
            {
                identifier = "unknown";
            }

            String diagnostic = "Duplicate ID ";
            diagnostic += identifier;
            diagnostic += ": ";
            diagnostic += existingPath;
            diagnostic += " (";
            diagnostic += nodes[existing].debugData().file;
            diagnostic += ':';
            String existingLine = "?";
            (void)Detail::toString(nodes[existing].debugData().line, &existingLine);
            diagnostic += existingLine;
            diagnostic += ") vs ";
            diagnostic += addedPath;
            diagnostic += " (";
            diagnostic += nodes[index].debugData().file;
            diagnostic += ':';
            String addedLine = "?";
            (void)Detail::toString(nodes[index].debugData().line, &addedLine);
            diagnostic += addedLine;
            diagnostic += ')';
            frame.diagnostics.emplace_back(std::move(diagnostic));
        }

        indexFrameNode(nodes[index].id, index);

        if(frameCaptureOptions.debug == true)
        {
            (void)nodePath(index);
        }

        Node & parent = nodes[currentParent];

        if(parent.firstChild == std::numeric_limits<size_t>::max())
        {
            parent.firstChild = index;
        }
        else
        {
            nodes[parent.lastChild].nextSibling = index;
        }

        parent.lastChild = index;

        return index;
    }
    //////////////////////////////////////////////////////////////////////////
    const String & Context::nodePath(size_t index)
    {
        Node & node = nodes[index];

        if(node.frameStringIndex < frameNodeStringCount && frameNodeStrings[node.frameStringIndex].path.empty() == false)
        {
            return frameNodeStrings[node.frameStringIndex].path;
        }

        if(index == 0)
        {
            FrameNodeStrings & strings = ensureNodeStrings(node);
            strings.path = "Root";

            return strings.path;
        }

        String parentPath = nodePath(node.parent);
        FrameNodeStrings & strings = ensureNodeStrings(node);
        strings.path = std::move(parentPath);
        strings.path += '/';
        strings.path += Detail::nodeKindName(node.kind);
        strings.path += '[';

        if(node.label.empty() == false)
        {
            strings.path += node.label;
        }
        else
        {
            String nodeId = "?";
            (void)Detail::toString(node.id, &nodeId);
            strings.path += nodeId;
        }

        strings.path += ']';

        return strings.path;
    }
    //////////////////////////////////////////////////////////////////////////
    const String & Context::nodePath(Node & node)
    {
        const String & returnedValue = nodePath(static_cast<size_t>(&node - nodes.data()));

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Context::Node Context::acquireNode()
    {
        Node node;

        if(recycledNodeStorage.empty() == true)
        {
            return node;
        }

        RecycledNodeStorage & storage = recycledNodeStorage.back();
        node.label = std::move(storage.label);
        node.label.clear();
        node.valueText = std::move(storage.valueText);
        node.valueText.clear();
        recycledNodeStorage.pop_back();

        return node;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::recycleFrameNodes()
    {
        recycledNodeStorage.clear();

        if(recycledNodeStorage.capacity() < nodes.size())
        {
            recycledNodeStorage.reserve(nodes.size());
        }

        for(auto iterator = nodes.rbegin(); iterator != nodes.rend(); ++iterator)
        {
            RecycledNodeStorage storage;
            storage.label = std::move(iterator->label);
            storage.valueText = std::move(iterator->valueText);
            recycledNodeStorage.emplace_back(std::move(storage));
        }
        nodes.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    Response Context::interact(size_t index, bool keyboardActivation, const Rect * interactionBounds)
    {
        Detail::ItemBehaviorOptions options;
        options.keyboardActivation = keyboardActivation;
        auto returnedValue = Detail::itemBehavior(this, index, options, interactionBounds);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response Context::interact(size_t index, const Detail::ItemBehaviorOptions & options, const Rect * interactionBounds)
    {
        auto returnedValue = Detail::itemBehavior(this, index, options, interactionBounds);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    uint64_t Context::pushScope(size_t node, const Theme * previousStyle, bool previousDisabled)
    {
        uint64_t token = nextScopeToken++;
        scopes.push_back({token, node, currentParent, previousDisabled, currentInputBlocked, currentNavigationBlocked, currentLiveEditText, currentLiveEditScalar, currentInputLayer, currentWindow, currentWindowSubmission, previousStyle, currentSelectionModel, currentSelectionOrder, currentSelectionOptions, currentSelectionScope});
        currentParent = node;

        return token;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::closeScope(uint64_t token) noexcept
    {
        if(scopes.empty() == true)
        {
            frame.diagnostics.emplace_back("Mosaic scopes must be destroyed in reverse construction order");

            return;
        }

        const ScopeState & lastScope = scopes.back();

        if(lastScope.token != token)
        {
            frame.diagnostics.emplace_back("Mosaic scopes must be destroyed in reverse construction order");

            return;
        }

        ScopeState previous = lastScope;

        if(previous.node != previous.previousParent && previous.node < nodes.size())
        {
            Node & scopeNode = nodes[previous.node];

            if(scopeNode.nextSameLinePending == true)
            {
                frame.diagnostics.emplace_back("sameLine directive was not consumed before its layout scope closed");
                scopeNode.nextSameLinePending = false;
            }

            if(scopeNode.nextTextAlignToFramePadding == true)
            {
                frame.diagnostics.emplace_back("alignTextToFramePadding directive was not consumed before its layout scope closed");
                scopeNode.nextTextAlignToFramePadding = false;
            }

            bool aggregateGroup = false;

            if(scopeNode.kind == Detail::NodeKind::Scope)
            {
                aggregateGroup = true;
            }

            if(scopeNode.kind == Detail::NodeKind::Row)
            {
                aggregateGroup = true;
            }

            if(scopeNode.kind == Detail::NodeKind::Column)
            {
                aggregateGroup = true;
            }

            if(scopeNode.kind == Detail::NodeKind::Grid)
            {
                aggregateGroup = true;
            }

            if(scopeNode.kind == Detail::NodeKind::Overlay)
            {
                aggregateGroup = true;
            }

            if(scopeNode.semanticRole == SemanticRole::Group && aggregateGroup == true)
            {
                Response aggregate;
                aggregate.id = scopeNode.id;
                size_t child = scopeNode.firstChild;

                while(child != std::numeric_limits<size_t>::max())
                {
                    aggregate.flags |= nodes[child].response.flags & ~(1U << 13U);
                    child = nodes[child].nextSibling;
                }

                if(scopeNode.anonymousIdentity == false)
                {
                    Detail::setFlag(aggregate, 0, Mosaic::itemHovered(this, scopeNode.id));
                }

                scopeNode.response = aggregate;
            }
        }

        scopes.pop_back();
        currentParent = previous.previousParent;
        currentDisabled = previous.previousDisabled;
        currentInputBlocked = previous.previousInputBlocked;
        currentNavigationBlocked = previous.previousNavigationBlocked;
        currentLiveEditText = previous.previousLiveEditText;
        currentLiveEditScalar = previous.previousLiveEditScalar;
        currentInputLayer = previous.previousInputLayer;
        currentWindow = previous.previousWindow;
        currentWindowSubmission = previous.previousWindowSubmission;
        currentStyle = previous.previousStyle;
        currentSelectionModel = previous.previousSelectionModel;
        currentSelectionOrder = previous.previousSelectionOrder;
        currentSelectionOptions = previous.previousSelectionOptions;
        currentSelectionScope = previous.previousSelectionScope;
    }
    //////////////////////////////////////////////////////////////////////////
    void Context::updateShortcuts()
    {
        for(ShortcutRegistry::Binding & binding : shortcuts.m_bindings)
        {
            binding.active = false;
            for(const KeyEvent & event : input.keyboard)
            {
                if(event.pressed == true && event.key == binding.shortcut.key && Detail::modifierMatches(event.modifiers, binding.shortcut.modifiers) == true)
                {
                    binding.active = true;
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Context * newContext(const ContextOptions & options)
    {
        Allocator & backingAllocator = options.allocator == nullptr ? defaultAllocator() : *options.allocator;
        void * memory = backingAllocator.allocate(sizeof(Context), alignof(Context));

        if(memory == nullptr)
        {
            return nullptr;
        }

        void * trackerMemory = backingAllocator.allocate(sizeof(Detail::AllocationTracker), alignof(Detail::AllocationTracker));

        if(trackerMemory == nullptr)
        {
            backingAllocator.deallocate(memory, sizeof(Context), alignof(Context));

            return nullptr;
        }

        Detail::AllocationTracker * tracker = ::new(trackerMemory) Detail::AllocationTracker(&backingAllocator);
        Detail::AllocationTrackerPtr trackerOwner(tracker);
        Detail::ConstructionAllocatorScope allocatorScope(tracker);
        Context * ui = ::new(memory) Context;
        ui->allocationTracker = std::move(trackerOwner);
        ui->allocator = tracker;

        if(options.platform != nullptr)
        {
            ui->platform = options.platform;
        }

        ui->fontProvider = options.fontProvider;
        ui->frameArena.setAllocator(tracker);
        ui->drawList.setStorage(&ui->drawCommandStorage);
        ui->textMeasureCache.resize(4096);

        return ui;
    }
    //////////////////////////////////////////////////////////////////////////
    Context * newContext(PlatformAdapter * platform, FontProvider * fontProvider)
    {
        ContextOptions options;
        options.platform = platform;
        options.fontProvider = fontProvider;
        Context * ui = Mosaic::newContext(options);

        return ui;
    }
    //////////////////////////////////////////////////////////////////////////
    void deleteContext(Context * ui) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        Allocator * backingAllocator = ui->allocationTracker->backingAllocator();
        Detail::AllocatorReference keepBackingAlive(*backingAllocator);

        Detail::destroyNativeSurfaces(ui);

        ui->~Context();
        backingAllocator->deallocate(ui, sizeof(Context), alignof(Context));
    }
    //////////////////////////////////////////////////////////////////////////
    void beginFrame(Context * ui, const Input & input, const Viewport & viewport)
    {
        if(ui->activeFrame == true)
        {
            ui->frame.diagnostics.emplace_back("beginFrame called while a frame is active");

            return;
        }

        Detail::ConstructionAllocatorScope allocatorScope(ui->allocator);
        ui->activeFrame = true;
        ui->allocationTracker->reset();
        ui->frameStarted = ui->platform->monotonicTime();
        bool altPressed = input.modifiers.alt && ui->previousModifiers.alt == false;
        ui->input = input;
        ui->previousModifiers = input.modifiers;
        ui->previousMenuBarItems.swap(ui->menuBarItems);
        ui->menuBarItems.clear();

        if(altPressed == true && ui->previousMenuBarItems.empty() == false)
        {
            ui->menuKeyboardMode = ui->menuKeyboardMode == false;

            if(ui->menuKeyboardMode == true)
            {
                ui->focused = ui->previousMenuBarItems.front();
                ui->navigationFocused = ui->focused;
                ui->navigationCursorVisible = true;
            }
            else if(ui->popupStack.empty() == true)
            {
                ui->navigationFocused = InvalidId;
            }
        }

        if(ui->input.keyPressed(KeyCode::Escape) == true && ui->popupStack.empty() == true)
        {
            ui->menuKeyboardMode = false;
        }

        ui->dragDrop.beginFrame();

        if(ui->configuration.pointerInput == false)
        {
            ui->input.pointers.clear();
            ui->input.wheel = {};
            ui->active = InvalidId;
            ui->activeItem = {};
            ui->captured = InvalidId;
            ui->capturedItem = {};
            ui->capturedPointer = 0;
            ui->wheelOwner = InvalidId;
            ui->wheelOwnerItem = {};
            ui->wheelOwnerTimestamp = 0.0;
            ui->pointerFocused = InvalidId;
            ui->pointerDownDurations.fill(0.f);
        }

        if(ui->configuration.keyboardInput == false)
        {
            ui->input.keyboard.clear();
            ui->input.text.clear();
            ui->input.ime.clear();
            ui->input.modifiers = {};
            ui->keyDownStates.fill(false);
            ui->keyDownDurations.fill(0.f);
        }
        else
        {
            bool clearFocus = ui->input.keyPressed(KeyCode::Escape);

            if(ui->configuration.escapeClearsItemFocus == false)
            {
                if(ui->configuration.escapeClearsWindowFocus == false)
                {
                    clearFocus = false;
                }
            }

            if(ui->blockingInputLayer != InvalidId)
            {
                clearFocus = false;
            }

            // The editor must receive Escape while it still owns focus; clearing
            // it here turns cancellation into an ordinary commit on focus loss.
            const Context::Persistent * focusedState = ui->findState(ui->focused);
            if(focusedState != nullptr && focusedState->editing == true)
            {
                clearFocus = false;
            }

            if(clearFocus == true)
            {
                ui->focused = InvalidId;
                ui->navigationFocused = InvalidId;

                if(ui->configuration.escapeClearsWindowFocus == true)
                {
                    ui->pointerFocused = InvalidId;
                }
            }
        }

        for(size_t index = 0; index != ui->keyDownStates.size(); ++index)
        {
            if(ui->keyDownStates[index] == true)
            {
                ui->keyDownDurations[index] += ui->input.deltaTime;
            }
        }
        for(const KeyEvent & event : ui->input.keyboard)
        {
            size_t index = static_cast<size_t>(event.key);

            if(index >= ui->keyDownStates.size())
            {
                continue;
            }

            if(event.released == true)
            {
                ui->keyDownStates[index] = false;
                ui->keyDownDurations[index] = 0.f;
            }
            else if(event.pressed == true && ui->keyDownStates[index] == false)
            {
                ui->keyDownStates[index] = true;
                ui->keyDownDurations[index] = 0.f;
            }
        }

        if(ui->configuration.navigationCursorVisibleAlways == true)
        {
            ui->navigationCursorVisible = true;
        }
        else if(ui->configuration.navigationCursorVisibleAuto == true)
        {
            for(const PointerState & pointer : ui->input.pointers)
            {
                if(pointer.pressed != 0)
                {
                    ui->navigationCursorVisible = false;
                    break;
                }
            }
            for(const KeyEvent & event : ui->input.keyboard)
            {
                if(event.pressed == false)
                {
                    continue;
                }

                switch(event.key)
                {
                case KeyCode::Tab:
                case KeyCode::Left:
                case KeyCode::Right:
                case KeyCode::Up:
                case KeyCode::Down:
                case KeyCode::Home:
                case KeyCode::End:
                case KeyCode::PageUp:
                case KeyCode::PageDown:
                    ui->navigationCursorVisible = true;
                    break;
                default:
                    break;
                }
            }
        }
        else
        {
            ui->navigationCursorVisible = true;
        }

        const PointerState * primaryPointer = ui->input.primaryPointer();
        for(size_t button = 0; button != ui->pointerDownDurations.size(); ++button)
        {
            bool down = primaryPointer != nullptr && (primaryPointer->down & (1U << button)) != 0;

            if(down == false)
            {
                ui->pointerDownDurations[button] = 0.f;
            }
            else if((primaryPointer->pressed & (1U << button)) == 0)
            {
                ui->pointerDownDurations[button] += ui->input.deltaTime;
            }
        }
        ui->viewport = viewport;
        ui->inputCaptureOverride = {};
        ui->frameCaptureOptions = {};
        ++ui->frame.number;
        ui->debugBeginCall = 0;
        ui->frameTextCacheHits = 0;
        ui->frameTextCacheMisses = 0;
        ui->frameTextCacheEvictions = 0;
        ui->frameTextMeasureCacheHits = 0;
        ui->frameTextMeasureCacheMisses = 0;
        ui->syncTextCache();
        ui->transientTextCount = 0;
        ui->transientTextIndexCount = 0;

        if(ui->frame.viewports.empty() == false)
        {
            FrameViewport & previousViewport = ui->frame.viewports.front();
            DrawCommandVector * previousCommands = Detail::FrameViewportAccess::commands(previousViewport);

            if(previousCommands != nullptr)
            {
                ui->drawList.commands().swap(*previousCommands);
            }

            Detail::FrameRenderData * previousRenderData = Detail::FrameViewportAccess::renderData(previousViewport);

            if(previousRenderData != nullptr)
            {
                ui->drawCommandStorage.swap(previousRenderData->storage);
            }
        }

        ui->drawList.clear();
        ui->drawCommandStorage.clear();
        ui->frame.viewports.clear();
        ui->frameRenderDataCount = 0;
        ui->frame.renderStates.clear();
        ui->frame.textInput.owner = InvalidId;
        ui->frame.textInput.value.clear();
        ui->frame.textInput.composition.clear();
        ui->frame.textInput.cursor = 0;
        ui->frame.textInput.anchor = 0;
        ui->frame.textInput.compositionBegin = 0;
        ui->frame.textInput.compositionEnd = 0;
        ui->frame.textInput.compositionSelectionBegin = 0;
        ui->frame.textInput.compositionSelectionEnd = 0;
        ui->frame.textInput.active = false;
        ui->frame.textInput.password = false;
        ui->frame.textInput.multiline = false;
        ui->previousSelectionItems.swap(ui->selectionItems);
        ui->selectionItems.clear();
        ui->selectionRequests.clear();
        ui->selectionOverlays.clear();
        ui->lastRenderStateKey = 0;
        ui->frame.semantics.clear();
        ui->frame.events.clear();
        ui->frame.diagnostics.clear();
        for(size_t index = 0; index != ui->frameCanvasCommandCount; ++index)
        {
            ui->frameCanvasCommands[index].clear();
        }
        ui->frameCanvasCommandCount = 0;
        ui->canvasInteractionStates.clear();
        for(size_t index = 0; index != ui->frameNodeStringCount; ++index)
        {
            ui->frameNodeStrings[index].semanticName.clear();
            ui->frameNodeStrings[index].semanticDescription.clear();
            ui->frameNodeStrings[index].semanticValue.clear();
            ui->frameNodeStrings[index].path.clear();
            ui->frameNodeStrings[index].identityDebugValue.clear();
        }
        ui->frameNodeStringCount = 0;
        ui->recycleFrameNodes();
        ui->frameArena.reset();
        ui->frameColorTextDataCount = 0;
        ui->frameStyleCount = 0;
        ui->trimTextCache(Detail::MaximumTextCacheEntries, Detail::MaximumTextCacheMemory);
        ui->scopes.clear();
        ui->currentParent = 0;
        ui->currentDisabled = false;
        ui->currentInputBlocked = false;
        ui->currentNavigationBlocked = false;
        ui->currentLiveEditText = true;
        ui->currentLiveEditScalar = true;
        ui->currentInputLayer = InvalidId;
        ui->currentWindow = InvalidId;
        ui->currentWindowSubmission = 0;
        ui->currentSelectionModel = nullptr;
        ui->currentSelectionOrder = {};
        ui->currentSelectionOptions = {};
        ui->currentSelectionScope = InvalidId;
        ui->pointerWindow = InvalidId;
        ui->previousBlockingInputLayer = ui->blockingInputLayer;
        ui->blockingInputLayer = InvalidId;
        ui->popupReplacementAllowed = false;
        ui->popupClosedByOutsidePointer = false;
        ui->popupClosedThisFrame.clear();
        ui->frameTheme = ui->theme;
        ui->frameTheme.metrics.fontSize *= ui->mainFontScale;
        ui->frameTheme.metrics.lineHeight *= ui->mainFontScale;
        ui->currentStyle = &ui->frameTheme;
        ui->anonymousCounter = 1;
        ui->nextWindowSubmission = 1;
        ui->focusOrder.clear();
        ui->focusNextPending = false;
        ui->nextItemShortcutPending = false;
        ui->nextWindow = {};
        ui->mainMenuBarSubmitted = false;
        ui->itemWidthStack.clear();
        ui->nextItemWidthPending = false;
        Detail::beginPopupFrame(ui);
        ui->updateInteractionScrollAnimations();
        ui->prepareInteractionSnapshot();
        ui->routeInteractionWheel();
        ui->prepareInteractionSnapshot();

        if(const PointerState * pointer = ui->input.primaryPointer(); pointer != nullptr)
        {
            for(Id candidateId : ui->visibleWindowIds)
            {
                Context::Persistent & candidate = ui->state(candidateId);

                if(candidateId == ui->pointerWindow && pointer->type != PointerType::Touch)
                {
                    if(candidate.hoverLastFrame == 0 || candidate.hoverLastFrame + 1 != ui->frame.number)
                    {
                        candidate.hoverStartedTimestamp = ui->input.timestamp;
                        candidate.stationaryHoverStartedTimestamp = ui->input.timestamp;
                    }

                    candidate.hoverDuration = std::max(0.0, ui->input.timestamp - candidate.hoverStartedTimestamp);

                    if(std::abs(pointer->delta.x) > 0.25f || std::abs(pointer->delta.y) > 0.25f)
                    {
                        candidate.stationaryHoverStartedTimestamp = ui->input.timestamp;
                    }

                    candidate.stationaryHoverDuration = std::max(0.0, ui->input.timestamp - candidate.stationaryHoverStartedTimestamp);
                    candidate.hoverLastFrame = ui->frame.number;
                }
                else
                {
                    candidate.hoverDuration = 0.0;
                    candidate.stationaryHoverDuration = 0.0;
                    candidate.hoverLastFrame = 0;
                }
            }
        }

        ui->previousVisibleWindowIds = ui->visibleWindowIds;
        ui->visibleWindowIds.clear();
        ui->windowFrameInstances.clear();
        ui->collectPersistentStateGarbage();

        const PointerState * dockPointer = ui->input.primaryPointer();

        if(dockPointer != nullptr)
        {
            const DockSpaceOptions * activeDockSpace = Detail::dockSpaceOptions(ui, ui->activeDockGroup);
            bool activeNoResize = ui->configuration.dockingNoResize || (activeDockSpace != nullptr && activeDockSpace->noResize == true);

            if(activeNoResize == true && ui->activeDockNode != 0)
            {
                Id activeSplitter = ui->activeDockSplitter;
                ui->activeDockNode = 0;
                ui->activeDockGroup = 0;
                ui->activeDockSplitter = InvalidId;

                if(ui->captured == activeSplitter)
                {
                    ui->captured = InvalidId;
                    ui->capturedItem = {};
                    ui->capturedPointer = 0;
                }
            }

            if(ui->configuration.dockingNoResize == false && ui->activeDockNode == 0 && dockPointer->isPressed() == true)
            {
                for(auto splitter = ui->dockSplitters.rbegin(); splitter != ui->dockSplitters.rend(); ++splitter)
                {
                    const DockSpaceOptions * splitterOptions = Detail::dockSpaceOptions(ui, splitter->group);

                    if(splitterOptions != nullptr && splitterOptions->noResize == true)
                    {
                        continue;
                    }

                    Rect hitBounds = splitter->orientation == Orientation::Horizontal ? Rect{splitter->bounds.x - 3.f, splitter->bounds.y, splitter->bounds.width + 6.f, splitter->bounds.height} : Rect{splitter->bounds.x, splitter->bounds.y - 3.f, splitter->bounds.width, splitter->bounds.height + 6.f};

                    if(hitBounds.contains(dockPointer->position) == false)
                    {
                        continue;
                    }

                    ui->activeDockNode = splitter->node;
                    ui->activeDockGroup = splitter->group;
                    ui->activeDockSplitter = combineId(combineId(RootId, splitter->group), splitter->node);
                    ui->captured = ui->activeDockSplitter;
                    ui->capturedPointer = dockPointer->id;
                    ui->dockSplitterDragOffset = splitter->orientation == Orientation::Horizontal ? dockPointer->position.x - splitter->bounds.x : dockPointer->position.y - splitter->bounds.y;
                    break;
                }
            }

            if(activeNoResize == false && ui->activeDockNode != 0 && dockPointer->isDown() == true)
            {
                auto splitter = std::find_if(ui->dockSplitters.begin(), ui->dockSplitters.end(),
                                                   [ui](const DockSplitterLayoutEntry & value)
                                                   {
                                                       return value.group == ui->activeDockGroup && value.node == ui->activeDockNode;
                                                   });

                if(splitter != ui->dockSplitters.end())
                {
                    float primary = splitter->orientation == Orientation::Horizontal ? dockPointer->position.x - splitter->parentBounds.x - ui->dockSplitterDragOffset : dockPointer->position.y - splitter->parentBounds.y - ui->dockSplitterDragOffset;
                    float extent = splitter->orientation == Orientation::Horizontal ? splitter->parentBounds.width : splitter->parentBounds.height;

                    if(extent > 0.f)
                    {
                        float minimumRatio = std::min(0.5f, std::max(96.f, ui->theme.metrics.minimumControlWidth) / extent);
                        DockModel * model = Detail::dockModel(ui, ui->activeDockGroup, false);

                        if(model != nullptr)
                        {
                            (void)model->setSplitRatio(ui->activeDockNode, std::clamp(primary / extent, minimumRatio, 1.f - minimumRatio));
                        }
                    }
                }
            }

            if(ui->activeDockNode != 0 && dockPointer->isReleased() == true)
            {
                ui->activeDockNode = 0;
                ui->activeDockGroup = 0;
                ui->activeDockSplitter = InvalidId;
                ui->captured = InvalidId;
                ui->capturedItem = {};
                ui->capturedPointer = 0;
            }
        }

        Context::Node root = ui->acquireNode();
        root.kind = Detail::NodeKind::Root;
        root.debugPayload = ui->frameArena.make<Context::NodeDebugPayload>();

        if(root.debugPayload == nullptr)
        {
            root.debugPayload = &ui->fallbackNodeDebugPayload;
            ui->frame.diagnostics.emplace_back("Frame arena could not allocate root debug payload");
        }

        root.valuePayload = ui->frameArena.make<Context::ValueNodePayload>();

        if(root.valuePayload == nullptr)
        {
            root.valuePayload = &ui->fallbackValueNodePayload;
            ui->frame.diagnostics.emplace_back("Frame arena could not allocate root value payload");
        }

        root.id = RootId;
        root.identityScope = RootId;
        root.identityParent = RootId;
        root.identityLocal = RootId;
        root.style = &ui->frameTheme;
        root.bounds = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
        root.content = root.bounds;
        root.clip = viewport.bounds;
        root.layout.width = SizeRule::Fill;
        root.layout.height = SizeRule::Fill;
        ui->nodes.emplace_back(std::move(root));

        if(ui->input.windowFocused == false)
        {
            ui->active = InvalidId;
            ui->activeItem = {};
            ui->captured = InvalidId;
            ui->capturedItem = {};
            ui->capturedPointer = 0;
            ui->focused = InvalidId;
            ui->pointerFocused = InvalidId;
            ui->navigationFocused = InvalidId;
            ui->popupStack.clear();
            ui->blockingInputLayer = InvalidId;
            ui->previousBlockingInputLayer = InvalidId;
            ui->popupReplacementAllowed = false;
            ui->activeDockNode = 0;
            ui->activeDockGroup = 0;
            ui->activeDockSplitter = InvalidId;
            ui->dockingDragWindow = InvalidId;
            ui->dockingTabDragWindow = InvalidId;
            ui->dockingTabDragNode = 0;
            ui->dockingPreviewNode = 0;
            ui->dockingPreviewTargetWindow = InvalidId;
            ui->dockingTargetBounds = {};
            ui->dockingPreviewBounds = {};
            ui->activeBoxSelection = InvalidId;
            ui->boxSelectionModel = nullptr;
            ui->boxSelectionOriginal.clear();
            ui->boxSelectionBounds = {};
        }

        if(ui->configuration.keyboardNavigation == true)
        {
            Detail::updateNavigation(ui);
        }

        ui->updateShortcuts();
    }

    //////////////////////////////////////////////////////////////////////////
    void setFrameCaptureOptions(Context * ui, const FrameCaptureOptions & options) noexcept
    {
        if(ui != nullptr)
        {
            ui->frameCaptureOptions = options;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void setConfiguration(Context * ui, const Configuration & configuration) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->configuration = configuration;

        bool assertDisabled = ui->configuration.errorRecoveryEnableAssert == false;
        bool debugLogDisabled = ui->configuration.errorRecoveryEnableDebugLog == false;
        bool tooltipDisabled = ui->configuration.errorRecoveryEnableTooltip == false;

        if(assertDisabled == true && debugLogDisabled == true && tooltipDisabled == true)
        {
            ui->configuration.errorRecoveryEnableAssert = true;
            ui->configuration.errorRecoveryEnableDebugLog = true;
            ui->configuration.errorRecoveryEnableTooltip = true;
        }

        if(configuration.pointerInput == false)
        {
            ui->input.pointers.clear();
            ui->input.wheel = {};
            ui->active = InvalidId;
            ui->activeItem = {};
            ui->captured = InvalidId;
            ui->capturedItem = {};
            ui->capturedPointer = 0;
            ui->wheelOwner = InvalidId;
            ui->wheelOwnerTimestamp = 0.0;
            ui->pointerFocused = InvalidId;
        }

        if(configuration.keyboardInput == false)
        {
            ui->input.keyboard.clear();
            ui->input.text.clear();
            ui->input.ime.clear();
            ui->input.modifiers = {};
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool getConfiguration(const Context * ui, Configuration * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        *_out = ui->configuration;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool debugBreakAvailable(const Context * ui) noexcept
    {
        return ui != nullptr && ui->configuration.debugBreakCallback != nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    bool requestDebugBreak(Context * ui) noexcept
    {
        if(Mosaic::debugBreakAvailable(ui) == false)
        {
            return false;
        }

        DebugBreakCallback callback = ui->configuration.debugBreakCallback;
        void * userData = ui->configuration.debugBreakUserData;
        callback(userData);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    const Frame & endFrame(Context * ui)
    {
        if(ui->activeFrame == false)
        {
            return ui->frame;
        }

        Detail::ConstructionAllocatorScope allocatorScope(ui->allocator);

        if(ui->scopes.empty() == false)
        {
#if !defined(NDEBUG)
            if(ui->configuration.errorRecoveryEnableAssert == true)
            {
                assert(ui->scopes.empty() == true && "endFrame called with live RAII scopes");
            }
#endif

            if(ui->configuration.errorRecoveryEnableDebugLog == true)
            {
                ui->frame.diagnostics.emplace_back("endFrame called with live RAII scopes");
            }

            while(ui->scopes.empty() == false)
            {
                const Context::ScopeState & scope = ui->scopes.back();
                ui->closeScope(scope.token);
            }
        }

        Context::Node & rootNode = ui->nodes.front();

        if(rootNode.nextSameLinePending == true)
        {
            ui->frame.diagnostics.emplace_back("sameLine directive was not consumed before the frame ended");
            rootNode.nextSameLinePending = false;
        }

        if(rootNode.nextTextAlignToFramePadding == true)
        {
            ui->frame.diagnostics.emplace_back("alignTextToFramePadding directive was not consumed before the frame ended");
            rootNode.nextTextAlignToFramePadding = false;
        }

        bool showRecoveryTooltip = ui->configuration.errorRecovery == true;
        showRecoveryTooltip = showRecoveryTooltip == true && ui->configuration.errorRecoveryEnableTooltip == true;
        showRecoveryTooltip = showRecoveryTooltip == true && ui->frame.diagnostics.empty() == false;

        if(showRecoveryTooltip == true)
        {
            auto blocked = Mosaic::interactionScope(ui, false);
            auto diagnosticTooltip = Mosaic::tooltip(ui, Key("Mosaic diagnostics"), "Mosaic diagnostics", {480.f, 0.f});

            if(diagnosticTooltip.visible() == true)
            {
                Mosaic::separatorText(ui, "Programmer error");
                TextOptions diagnosticText;
                diagnosticText.wordWrap = true;
                diagnosticText.layout.width = SizeRule::Fill;

                for(size_t index = 0; index != ui->frame.diagnostics.size(); ++index)
                {
                    auto diagnostic = Mosaic::scope(ui, Key(index));
                    Mosaic::text(ui, ui->frame.diagnostics[index], diagnosticText);
                }
            }
        }

        ui->fontScopeTokens.clear();

        const PointerState * primaryPointer = ui->input.primaryPointer();
        ui->dragDrop.endFrame(primaryPointer != nullptr && primaryPointer->isReleased(PointerButton::Primary));

        double layoutStart = ui->platform->monotonicTime();
        Rect rootBounds = ui->viewport.workArea.empty() == true ? ui->viewport.bounds : ui->viewport.workArea;
        ui->measureNode(0, {rootBounds.width, rootBounds.height});
        Detail::arrangeFrame(ui, rootBounds);
        Detail::arrangeScrollChanges(ui);
        Detail::syncNativeSurfaces(ui);
        // Logical text metrics were resolved before layout. Only glyph placement and cached
        // Graphics geometry are deferred until final visibility is known.
        ui->prepareVisibleText();
        Detail::syncPopupBounds(ui);
        ui->captureInteractionSnapshot();
        for(const Context::Node & node : ui->nodes)
        {
            if(node.id != ui->focused)
            {
                continue;
            }

            if((node.kind != Detail::NodeKind::InputText && node.kind != Detail::NodeKind::InputMultiline))
            {
                continue;
            }

            Vec2 cursor = Detail::textCursorPosition(ui, node, node.textEditData().cursor);
            ui->platform->setImeCandidateRect({cursor.x, cursor.y, 1.f, node.style->metrics.lineHeight});
            const Detail::TextEditorState * editorState = ui->findTextEditorState(node.id);

            if(editorState != nullptr)
            {
                TextInputState & textInput = ui->frame.textInput;
                textInput.owner = node.id;
                textInput.value = node.textEditData().password ? String{} : String(ui->nodeSemanticValue(node));
                textInput.composition = node.textEditData().password ? String{} : editorState->composition;
                textInput.cursor = editorState->cursor;
                textInput.anchor = editorState->anchor;
                textInput.compositionBegin = editorState->compositionBegin;
                textInput.compositionEnd = editorState->compositionEnd;
                textInput.compositionSelectionBegin = editorState->compositionSelectionBegin;
                textInput.compositionSelectionEnd = editorState->compositionSelectionEnd;
                textInput.active = true;
                textInput.password = node.textEditData().password;
                textInput.multiline = node.textEditData().multiline;
            }

            break;
        }
        Detail::finishTextLog(ui);
        Detail::copyFocusedWindowText(ui);
        double layoutEnd = ui->platform->monotonicTime();

        FrameViewport outputViewport;
        outputViewport.id = ui->viewport.id;
        outputViewport.surfaceId = 0;
        outputViewport.bounds = ui->viewport.bounds;
        outputViewport.localBounds = {0.f, 0.f, ui->viewport.bounds.width, ui->viewport.bounds.height};
        outputViewport.screenBounds = ui->viewport.bounds;
        outputViewport.dpiScale = ui->viewport.dpiScale;
        outputViewport.nativeHandle = ui->viewport.nativeHandle;
        outputViewport.renderTarget = ui->viewport.renderTarget;
        ui->frame.viewports.emplace_back(std::move(outputViewport));
        DrawList & drawList = ui->drawList;
        ui->windowRenderOrder.clear();
        for(size_t index = 1; index != ui->nodes.size(); ++index)
        {
            const Context::Node & node = ui->nodes[index];

            if(node.parent == 0 && node.kind == Detail::NodeKind::Window && node.visible == true)
            {
                ui->windowRenderOrder.push_back(index);
            }
        }
        auto windowOrder = [ui](size_t first, size_t second)
        {
            return ui->nodes[first].windowData().zOrder < ui->nodes[second].windowData().zOrder;
        };

        if(std::is_sorted(ui->windowRenderOrder.begin(), ui->windowRenderOrder.end(), windowOrder) == false)
        {
            std::stable_sort(ui->windowRenderOrder.begin(), ui->windowRenderOrder.end(), windowOrder);
        }

        // Debug tools are built during the next frame, so keep the previous completed
        // capture readable until all current nodes have been submitted.
        ui->frame.debug.clear();
        for(size_t index = 1; index != ui->nodes.size(); ++index)
        {
            if(ui->nodes[index].kind == Detail::NodeKind::Canvas && ui->hasCanvasCommands(ui->nodes[index], CanvasLayer::Background) == true)
            {
                ui->emitNode(index, drawList, CanvasLayer::Background);
            }
        }
        ui->emitNode(0, drawList, CanvasLayer::Local);
        for(size_t index = 1; index != ui->nodes.size(); ++index)
        {
            if(ui->nodes[index].kind == Detail::NodeKind::Canvas && ui->hasCanvasCommands(ui->nodes[index], CanvasLayer::Foreground) == true)
            {
                ui->emitNode(index, drawList, CanvasLayer::Foreground);
            }
        }

        ui->itemPickerHovered = InvalidId;
        ui->itemPickerHoveredBounds = {};
        if(ui->itemPickerSelected != InvalidId)
        {
            const Context::Node * selectedNode = ui->findFrameNode(ui->itemPickerSelected);

            if(selectedNode != nullptr)
            {
                ui->itemPickerSelectedBounds = selectedNode->bounds;
            }
        }

        if(ui->itemPickerEnabled == true && primaryPointer != nullptr)
        {
            uint64_t candidateZOrder = 0;
            size_t candidateIndex = 0;
            for(size_t index = 1; index != ui->nodes.size(); ++index)
            {
                const Context::Node & node = ui->nodes[index];
                bool structural = Detail::itemPickerStructuralNode(node.kind);

                if(structural == true)
                {
                    continue;
                }

                if(node.visible == false)
                {
                    continue;
                }

                if(node.inputBlocked == true)
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

                if(node.bounds.contains(primaryPointer->position) == false)
                {
                    continue;
                }

                if(node.clip.contains(primaryPointer->position) == false)
                {
                    continue;
                }

                if(node.windowData().zOrder < candidateZOrder)
                {
                    continue;
                }

                if(node.windowData().zOrder == candidateZOrder && index < candidateIndex)
                {
                    continue;
                }

                candidateZOrder = node.windowData().zOrder;
                candidateIndex = index;
                ui->itemPickerHovered = node.id;
                ui->itemPickerHoveredBounds = node.bounds;
            }

            if(primaryPointer->isPressed(PointerButton::Primary) == true && ui->itemPickerHovered != InvalidId)
            {
                ui->itemPickerSelected = ui->itemPickerHovered;
                ui->itemPickerSelectedBounds = ui->itemPickerHoveredBounds;
                ui->itemPickerEnabled = false;
            }
        }

        auto drawPickerBounds = [ui, &drawList](const Rect & bounds, const Color & color, float thickness)
        {
            if(bounds.empty() == true)
            {
                return;
            }

            RenderState state;
            state.clip = ui->viewport.bounds;
            state.blend = BlendMode::PremultipliedAlpha;
            state.renderTarget = ui->viewport.renderTarget;
            uint64_t renderKey = ui->internRenderState(state);
            drawList.line({bounds.x, bounds.y}, {bounds.right(), bounds.y}, thickness, color, renderKey);
            drawList.line({bounds.right(), bounds.y}, {bounds.right(), bounds.bottom()}, thickness, color, renderKey);
            drawList.line({bounds.right(), bounds.bottom()}, {bounds.x, bounds.bottom()}, thickness, color, renderKey);
            drawList.line({bounds.x, bounds.bottom()}, {bounds.x, bounds.y}, thickness, color, renderKey);
        };

        if(ui->itemPickerSelected != InvalidId)
        {
            drawPickerBounds(ui->itemPickerSelectedBounds, Color::fromBytes(255, 86, 174, 230), 2.f);
        }

        if(ui->itemPickerEnabled == true && ui->itemPickerHovered != InvalidId)
        {
            drawPickerBounds(ui->itemPickerHoveredBounds, Color::fromBytes(255, 218, 72, 240), 2.f);
        }

        for(const Context::SelectionOverlay & overlay : ui->selectionOverlays)
        {
            RenderState overlayState;
            overlayState.clip = overlay.clip;
            overlayState.blend = BlendMode::PremultipliedAlpha;
            overlayState.renderTarget = ui->viewport.renderTarget;
            uint64_t overlayKey = ui->internRenderState(overlayState);
            drawList.rect(overlay.bounds, overlay.fill, overlayKey);
            drawList.line({overlay.bounds.x, overlay.bounds.y}, {overlay.bounds.right(), overlay.bounds.y}, 1.f, overlay.border, overlayKey);
            drawList.line({overlay.bounds.right(), overlay.bounds.y}, {overlay.bounds.right(), overlay.bounds.bottom()}, 1.f, overlay.border, overlayKey);
            drawList.line({overlay.bounds.right(), overlay.bounds.bottom()}, {overlay.bounds.x, overlay.bounds.bottom()}, 1.f, overlay.border, overlayKey);
            drawList.line({overlay.bounds.x, overlay.bounds.bottom()}, {overlay.bounds.x, overlay.bounds.y}, 1.f, overlay.border, overlayKey);
        }

        if(ui->dockSplitters.empty() == false)
        {
            RenderState splitterState;
            splitterState.clip = ui->viewport.bounds;
            splitterState.blend = BlendMode::PremultipliedAlpha;
            splitterState.renderTarget = ui->viewport.renderTarget;
            uint64_t splitterKey = ui->internRenderState(splitterState);
            const PointerState * splitterPointer = ui->input.primaryPointer();
            for(const DockSplitterLayoutEntry & splitter : ui->dockSplitters)
            {
                Rect hitBounds = splitter.orientation == Orientation::Horizontal ? Rect{splitter.bounds.x - 3.f, splitter.bounds.y, splitter.bounds.width + 6.f, splitter.bounds.height} : Rect{splitter.bounds.x, splitter.bounds.y - 3.f, splitter.bounds.width, splitter.bounds.height + 6.f};
                bool hovered = splitterPointer != nullptr && hitBounds.contains(splitterPointer->position);
                Color color = splitter.group == ui->activeDockGroup && splitter.node == ui->activeDockNode ? ui->theme.colors.resizeGripActive : (hovered ? ui->theme.colors.resizeGripHovered : ui->theme.colors.separator);
                drawList.rect(splitter.bounds, color, splitterKey);
            }
        }

        if(ui->dockingTargetBounds.empty() == false || ui->dockingPreviewBounds.empty() == false)
        {
            RenderState previewState;
            previewState.clip = ui->viewport.bounds;
            previewState.blend = BlendMode::PremultipliedAlpha;
            previewState.renderTarget = ui->viewport.renderTarget;
            uint64_t previewKey = ui->internRenderState(previewState);

            if(ui->dockingPreviewBounds.empty() == false)
            {
                drawList.roundedRect(ui->dockingPreviewBounds, ui->theme.metrics.cornerRadius, Detail::colorWithAlpha(ui->theme.colors.dockingPreview, 0.28f), previewKey);
                Rect preview = ui->dockingPreviewBounds;
                drawList.line({preview.x, preview.y}, {preview.right(), preview.y}, 2.f, ui->theme.colors.dockingPreview, previewKey);
                drawList.line({preview.right(), preview.y}, {preview.right(), preview.bottom()}, 2.f, ui->theme.colors.dockingPreview, previewKey);
                drawList.line({preview.right(), preview.bottom()}, {preview.x, preview.bottom()}, 2.f, ui->theme.colors.dockingPreview, previewKey);
                drawList.line({preview.x, preview.bottom()}, {preview.x, preview.y}, 2.f, ui->theme.colors.dockingPreview, previewKey);
            }

            if(ui->dockingTargetBounds.empty() == false)
            {
                Detail::DockTargetRectArray targets = Detail::dockTargetRects(ui->dockingTargetBounds);
                for(size_t index = 0; index != targets.size(); ++index)
                {
                    DockPlacement placement = static_cast<DockPlacement>(index);
                    bool selected = ui->dockingPreviewBounds.empty() == false && ui->dockingPreviewPlacement == placement;
                    Rect target = targets[index];
                    drawList.roundedRect(target, 5.f, Detail::colorWithAlpha(ui->theme.colors.popup, selected ? 0.98f : 0.88f), previewKey);
                    Color border = Detail::colorWithAlpha(ui->theme.colors.dockingPreview, selected ? 1.f : 0.72f);
                    drawList.line({target.x, target.y}, {target.right(), target.y}, 1.f, border, previewKey);
                    drawList.line({target.right(), target.y}, {target.right(), target.bottom()}, 1.f, border, previewKey);
                    drawList.line({target.right(), target.bottom()}, {target.x, target.bottom()}, 1.f, border, previewKey);
                    drawList.line({target.x, target.bottom()}, {target.x, target.y}, 1.f, border, previewKey);
                    Rect glyph = {target.x + 8.f, target.y + 8.f, std::max(0.f, target.width - 16.f), std::max(0.f, target.height - 16.f)};
                    drawList.roundedRect(Detail::dockPreviewBounds(glyph, placement, 0.42f), 1.5f, Detail::colorWithAlpha(ui->theme.colors.dockingPreview, selected ? 0.95f : 0.62f), previewKey);
                }
            }
        }

        FrameViewport & frameViewport = ui->frame.viewports.front();
        if(ui->frameRenderDataCount == ui->frameRenderData.size())
        {
            ui->frameRenderData.emplace_back(makeUnique<Detail::FrameRenderData>(*ui->allocator));
        }
        Detail::FrameRenderData * renderData = ui->frameRenderData[ui->frameRenderDataCount].get();
        ++ui->frameRenderDataCount;
        renderData->sharedStorage = nullptr;
        renderData->commands.swap(drawList.commands());
        Detail::FrameViewportAccess::setRenderData(frameViewport, renderData);

        Viewport mainViewport = ui->viewport;
        for(size_t surfaceNodeIndex : ui->visibleNativeSurfaceNodes)
        {
            Context::Node & surfaceNode = ui->nodes[surfaceNodeIndex];
            auto surfaceIterator = ui->nativeSurfaces.find(surfaceNode.id);

            if(surfaceIterator == ui->nativeSurfaces.end())
            {
                continue;
            }

            Context::NativeSurfaceState & surface = surfaceIterator->second;
            FrameViewport surfaceViewport;
            surfaceViewport.id = surfaceNode.id;
            surfaceViewport.surfaceId = surfaceNode.id;
            surfaceViewport.kind = surface.kind;
            surfaceViewport.bounds = surface.bounds;
            surfaceViewport.localBounds = {0.f, 0.f, surface.bounds.width, surface.bounds.height};
            surfaceViewport.screenBounds = surface.screenBounds;
            surfaceViewport.dpiScale = surface.dpiScale;
            surfaceViewport.nativeHandle = surface.renderHandle;
            surfaceViewport.renderTarget = surface.renderTarget;
            surfaceViewport.generation = surface.generation;
            surfaceViewport.applicationTag = surface.applicationTag;
            surfaceViewport.visible = surface.visible;
            ui->frame.viewports.emplace_back(surfaceViewport);

            Viewport emissionViewport;
            emissionViewport.id = surfaceNode.id;
            emissionViewport.bounds = surface.bounds;
            emissionViewport.workArea = surface.bounds;
            emissionViewport.dpiScale = surface.dpiScale;
            emissionViewport.focused = surface.focused;
            emissionViewport.nativeHandle = surface.renderHandle;
            emissionViewport.renderTarget = surface.renderTarget;
            ui->viewport = emissionViewport;
            ui->emittingNativeSurface = surfaceNode.id;

            if(ui->frameRenderDataCount == ui->frameRenderData.size())
            {
                ui->frameRenderData.emplace_back(makeUnique<Detail::FrameRenderData>(*ui->allocator));
            }

            Detail::FrameRenderData * surfaceRenderData = ui->frameRenderData[ui->frameRenderDataCount].get();
            ++ui->frameRenderDataCount;
            DrawList surfaceDrawList;
            surfaceDrawList.setStorage(&ui->drawCommandStorage);
            surfaceDrawList.commands().swap(surfaceRenderData->commands);
            surfaceDrawList.clear();

            for(size_t child = surfaceNode.firstChild; child != std::numeric_limits<size_t>::max(); child = ui->nodes[child].nextSibling)
            {
                if(ui->nodes[child].kind == Detail::NodeKind::Canvas && ui->hasCanvasCommands(ui->nodes[child], CanvasLayer::Background) == true)
                {
                    ui->emitNode(child, surfaceDrawList, CanvasLayer::Background);
                }
            }

            ui->emitNode(surfaceNodeIndex, surfaceDrawList, CanvasLayer::Local);

            for(size_t child = surfaceNode.firstChild; child != std::numeric_limits<size_t>::max(); child = ui->nodes[child].nextSibling)
            {
                if(ui->nodes[child].kind == Detail::NodeKind::Canvas && ui->hasCanvasCommands(ui->nodes[child], CanvasLayer::Foreground) == true)
                {
                    ui->emitNode(child, surfaceDrawList, CanvasLayer::Foreground);
                }
            }

            for(size_t child = ui->nodes[0].firstChild; child != std::numeric_limits<size_t>::max(); child = ui->nodes[child].nextSibling)
            {
                Context::Node & overlayNode = ui->nodes[child];

                if(overlayNode.kind != Detail::NodeKind::Backdrop)
                {
                    continue;
                }

                Rect overlap = Rect::intersection(overlayNode.bounds, surface.bounds);

                if(overlap.empty() == true)
                {
                    continue;
                }

                ui->emitNode(child, surfaceDrawList, CanvasLayer::Local);
            }

            for(size_t popupIndex : ui->windowRenderOrder)
            {
                Context::Node & popupNode = ui->nodes[popupIndex];

                if(popupNode.windowData().popup == false)
                {
                    continue;
                }

                Rect overlap = Rect::intersection(popupNode.bounds, surface.bounds);

                if(overlap.empty() == true)
                {
                    continue;
                }

                ui->emitNode(popupIndex, surfaceDrawList, CanvasLayer::Local);
            }

            surfaceRenderData->sharedStorage = &renderData->storage;
            surfaceRenderData->commands.swap(surfaceDrawList.commands());
            FrameViewport & addedViewport = ui->frame.viewports.back();
            Detail::FrameViewportAccess::setRenderData(addedViewport, surfaceRenderData);
        }

        ui->emittingNativeSurface = InvalidId;
        ui->viewport = mainViewport;
        renderData->storage.swap(ui->drawCommandStorage);

        for(size_t index = 1; index != ui->frameRenderDataCount; ++index)
        {
            ui->frameRenderData[index]->sharedStorage = &renderData->storage;
        }

        const PointerState * pointer = ui->input.primaryPointer();

        if(pointer != nullptr && pointer->isReleased() == true)
        {
            ui->active = InvalidId;
            ui->activeItem = {};
            ui->captured = InvalidId;
            ui->capturedItem = {};
            ui->capturedPointer = 0;
        }

        CursorShape cursor = CursorShape::Arrow;
        const PointerState * cursorPointer = ui->input.primaryPointer();

        if(ui->configuration.dockingNoResize == false)
        {
            for(const DockSplitterLayoutEntry & splitter : ui->dockSplitters)
            {
                const DockSpaceOptions * splitterOptions = Detail::dockSpaceOptions(ui, splitter.group);

                if(splitterOptions != nullptr && splitterOptions->noResize == true)
                {
                    continue;
                }

                Rect hitBounds = splitter.orientation == Orientation::Horizontal ? Rect{splitter.bounds.x - 3.f, splitter.bounds.y, splitter.bounds.width + 6.f, splitter.bounds.height} : Rect{splitter.bounds.x, splitter.bounds.y - 3.f, splitter.bounds.width, splitter.bounds.height + 6.f};
                bool activeSplitter = splitter.group == ui->activeDockGroup && splitter.node == ui->activeDockNode;
                bool hoveredSplitter = false;

                if(cursorPointer != nullptr)
                {
                    hoveredSplitter = hitBounds.contains(cursorPointer->position);
                }

                if(activeSplitter == true)
                {
                    cursor = splitter.orientation == Orientation::Horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical;
                    break;
                }

                if(hoveredSplitter == true)
                {
                    cursor = splitter.orientation == Orientation::Horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical;
                    break;
                }
            }
        }

        if(cursor == CursorShape::Arrow)
        {
            for(const Context::Node & node : ui->nodes)
            {
                if(node.kind == Detail::NodeKind::Scroll && (node.scrollResizeHovered == true || (ui->captured == node.id && ui->state(node.id).scrollData().resizingArea == true)))
                {
                    uint8_t edges = node.scrollResizeEdges;
                    bool horizontal = (edges & Detail::WindowResizeRight) != 0;
                    bool vertical = (edges & Detail::WindowResizeBottom) != 0;
                    cursor = horizontal && vertical ? CursorShape::ResizeDiagonalNwse : (horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical);
                    break;
                }

                if(node.kind == Detail::NodeKind::Window)
                {
                    bool resizeWindow = node.windowData().resizable;

                    if(node.windowData().resizeHovered == false)
                    {
                        if(ui->captured != node.id)
                        {
                            resizeWindow = false;
                        }
                        else if(ui->state(node.id).windowData().interaction != 6)
                        {
                            resizeWindow = false;
                        }
                    }

                    if(resizeWindow == true)
                    {
                        bool horizontal = (node.windowData().resizeEdges & (Detail::WindowResizeLeft | Detail::WindowResizeRight)) != 0;
                        bool vertical = (node.windowData().resizeEdges & (Detail::WindowResizeTop | Detail::WindowResizeBottom)) != 0;
                        bool northEastSouthWest = ((node.windowData().resizeEdges & Detail::WindowResizeLeft) != 0 && (node.windowData().resizeEdges & Detail::WindowResizeBottom) != 0) || ((node.windowData().resizeEdges & Detail::WindowResizeRight) != 0 && (node.windowData().resizeEdges & Detail::WindowResizeTop) != 0);
                        cursor = horizontal && vertical ? (northEastSouthWest ? CursorShape::ResizeDiagonalNesw : CursorShape::ResizeDiagonalNwse) : (horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical);
                        break;
                    }
                }

                if(node.disabled == false && node.kind == Detail::NodeKind::Split && ui->captured == node.id)
                {
                    cursor = node.layout.orientation == Orientation::Horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical;
                    break;
                }

                if(node.disabled == false && node.kind == Detail::NodeKind::DragValue && ui->captured == node.id)
                {
                    cursor = CursorShape::ResizeHorizontal;
                    break;
                }

                if(node.disabled == false && node.cursorOverride == true && node.response.hovered() == true)
                {
                    cursor = node.cursor;
                }
                else if(node.disabled == false && node.kind == Detail::NodeKind::Split && node.response.hovered() == true)
                {
                    cursor = node.layout.orientation == Orientation::Horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical;
                }
                else if(node.disabled == false && node.focusable == true && node.response.hovered() == true)
                {
                    cursor = node.kind == Detail::NodeKind::InputText || node.kind == Detail::NodeKind::InputMultiline ? CursorShape::Text : (node.kind == Detail::NodeKind::DragValue ? CursorShape::ResizeHorizontal : (node.kind == Detail::NodeKind::Canvas ? CursorShape::Crosshair : CursorShape::Hand));
                }
            }
        }

        if(ui->configuration.cursorChanges == true)
        {
            ui->platform->setCursor(cursor);
        }

        ui->currentCursor = cursor;
        Detail::updateInputCapture(ui);
        double emitEnd = ui->platform->monotonicTime();

        ui->previousFocusOrder = ui->focusOrder;
        ui->frame.metrics = {};

        if(ui->frameCaptureOptions.metrics == true)
        {
            ui->frame.metrics.widgetCount = ui->nodes.size() > 0 ? ui->nodes.size() - 1 : 0;
            ui->frame.metrics.visibleWidgetCount = static_cast<size_t>(std::count_if(ui->nodes.begin() + 1, ui->nodes.end(),
                                                                                     [](const Context::Node & node)
                                                                                     {
                                                                                         auto returnedValue = node.visible && node.clip.empty() == false;

                                                                                         return returnedValue;
                                                                                     }));
            ui->frame.metrics.culledWidgetCount = ui->frame.metrics.widgetCount - ui->frame.metrics.visibleWidgetCount;
            ui->frame.metrics.textCacheEntryCount = ui->textCacheEntryCount;
            ui->frame.metrics.textCacheHitCount = ui->frameTextCacheHits;
            ui->frame.metrics.textCacheMissCount = ui->frameTextCacheMisses;
            ui->frame.metrics.textCacheEvictionCount = ui->frameTextCacheEvictions;
            ui->frame.metrics.textCacheMemory = ui->textCacheMemory;
            ui->frame.metrics.textMeasureCacheHitCount = ui->frameTextMeasureCacheHits;
            ui->frame.metrics.textMeasureCacheMissCount = ui->frameTextMeasureCacheMisses;

            if(ui->fontProvider != nullptr)
            {
                FontCacheMetrics cache = ui->fontProvider->cacheMetrics();
                ui->frame.metrics.fontCount = cache.fontCount;
                ui->frame.metrics.glyphCount = cache.glyphCount;
                ui->frame.metrics.fontAtlasPageCount = cache.atlasPageCount;
                ui->frame.metrics.fontAtlasMemory = cache.atlasMemory;
            }

            for(size_t index = 1; index < ui->nodes.size(); ++index)
            {
                const Context::Node & node = ui->nodes[index];
                auto countTextRun = [&ui](bool prepared)
                {
                    ++ui->frame.metrics.textRunCount;

                    if(prepared == true)
                    {
                        ++ui->frame.metrics.shapedTextRunCount;
                    }
                    else
                    {
                        ++ui->frame.metrics.deferredTextRunCount;
                    }
                };

                if(node.label.empty() == false)
                {
                    countTextRun(node.textPrepared);
                }

                if(node.valueText.empty() == false)
                {
                    countTextRun(node.valueTextPrepared);
                }

                const Context::ColorTextData * colorText = ui->findColorTextData(node);

                if(colorText == nullptr)
                {
                    continue;
                }

                for(size_t channel = 0; channel < colorText->text.size(); ++channel)
                {
                    if(colorText->text[channel].empty() == false)
                    {
                        countTextRun(colorText->prepared[channel]);
                    }
                }
            }
            ui->frame.metrics.persistentStateCount = ui->persistent.size();
            const FrameViewport & metricsViewport = ui->frame.viewports.front();
            const DrawCommandVector * metricsCommands = Detail::FrameViewportAccess::commands(metricsViewport);
            const DrawCommandVector & drawCommands = *metricsCommands;
            ui->frame.metrics.drawCommandCount = drawCommands.size();
            bool hasRenderState = false;
            TextureHandle previousTexture = 0;
            Rect previousClip;
            for(const DrawCommand & command : drawCommands)
            {
                if(command.type == DrawCommandType::PushClip)
                {
                    continue;
                }

                if(command.type == DrawCommandType::PopClip)
                {
                    continue;
                }

                if(command.type == DrawCommandType::PushTransform || command.type == DrawCommandType::PopTransform)
                {
                    continue;
                }

                const RenderState * state = command.renderKey == 0 || command.renderKey > ui->frame.renderStates.size() ? nullptr : &ui->frame.renderStates[static_cast<size_t>(command.renderKey - 1)];
                TextureHandle texture = state == nullptr ? 0 : state->texture;
                Rect clip = state == nullptr || state->clip.empty() == true ? ui->viewport.bounds : state->clip;

                if(hasRenderState == false || texture != previousTexture)
                {
                    ++ui->frame.metrics.textureChanges;
                    previousTexture = texture;
                }

                if(hasRenderState == false || clip != previousClip)
                {
                    ++ui->frame.metrics.clipChanges;
                    previousClip = clip;
                }

                hasRenderState = true;
            }
            ui->frame.metrics.persistentMemory = ui->persistent.size() * sizeof(Context::Persistent);
            for(const auto & [id, persistentState] : ui->persistent)
            {
                (void)id;

                if(persistentState.window != nullptr)
                {
                    ui->frame.metrics.persistentMemory += sizeof(Context::WindowPersistentState);
                }

                if(persistentState.scroll != nullptr)
                {
                    ui->frame.metrics.persistentMemory += sizeof(Context::ScrollPersistentState);
                }
            }
            ui->frame.metrics.persistentMemory += ui->textEditorStates.size() * sizeof(Detail::TextEditorState);
            for(const auto & [id, editorState] : ui->textEditorStates)
            {
                (void)id;
                ui->frame.metrics.persistentMemory += editorState.editOriginal.capacity() + editorState.composition.capacity() + editorState.undoValue.capacity() + editorState.undo.capacity() * sizeof(Detail::TextUndoRecord);
                for(const Detail::TextUndoRecord & undoRecord : editorState.undo)
                {
                    ui->frame.metrics.persistentMemory += undoRecord.removed.capacity() + undoRecord.inserted.capacity();
                }
            }
            ui->frame.metrics.persistentMemory += ui->colorEditorStates.size() * sizeof(Detail::ColorEditorState);
            for(const auto & [id, colorState] : ui->colorEditorStates)
            {
                (void)id;
                ui->frame.metrics.persistentMemory += colorState.hexText.capacity();
                for(const String & channelText : colorState.channelText)
                {
                    ui->frame.metrics.persistentMemory += channelText.capacity();
                }
            }
            ui->frame.metrics.persistentMemory += ui->tabStates.size() * sizeof(Context::TabState);
            for(const auto & [id, tabState] : ui->tabStates)
            {
                (void)id;
                ui->frame.metrics.persistentMemory += tabState.order.capacity() * sizeof(size_t);
            }
            ui->frame.metrics.persistentMemory += ui->numericStates.capacity() * sizeof(Detail::NumericState);
            for(const Detail::NumericState & numericState : ui->numericStates)
            {
                ui->frame.metrics.persistentMemory += numericState.temporaryText.capacity();
            }
            for(const auto & [id, persistentState] : ui->persistent)
            {
                (void)id;

                if(persistentState.table == nullptr)
                {
                    continue;
                }

                ui->frame.metrics.persistentMemory += sizeof(Context::TableState) + persistentState.table->columns.capacity() * sizeof(Context::TableColumnState) + persistentState.table->sortSpecs.capacity() * sizeof(TableSortSpec) + (persistentState.table->intrinsicWidths.capacity() + persistentState.table->rowHeights.capacity() + persistentState.table->rowPositions.capacity() + persistentState.table->columnPositions.capacity()) * sizeof(float);
            }
            ui->frame.metrics.frameMemory =
                ui->nodes.capacity() * sizeof(Context::Node) + ui->frameArena.reservedMemory() + ui->recycledNodeStorage.capacity() * sizeof(Context::RecycledNodeStorage) + ui->frameCanvasCommands.capacity() * sizeof(DrawCommandVector) + ui->frameNodeStrings.capacity() * sizeof(Context::FrameNodeStrings) + ui->frameColorTextData.capacity() * sizeof(Context::ColorTextData) + ui->layoutFloatScratch.capacity() * sizeof(float) + ui->frameStyles.capacity() * sizeof(Context::ThemePtr) + ui->frameStyles.size() * sizeof(Theme) + ui->frameStyleIndices.capacity() * sizeof(Context::ThemeIndexEntry) + ui->renderStateIndices.capacity() * sizeof(Context::RenderStateIndexEntry) + ui->frameNodeIndices.capacity() * sizeof(Context::FrameNodeIndexEntry) + ui->selectionItems.capacity() * sizeof(Context::SelectionItem) + ui->selectionOverlays.capacity() * sizeof(Context::SelectionOverlay) + ui->boxSelectionOriginal.capacity() * sizeof(Id) + ui->frame.textInput.value.capacity() + ui->frame.textInput.composition.capacity();
            ui->frame.metrics.frameMemory += Detail::drawCommandStorageMemory(ui->drawCommandStorage);
            for(const DrawCommandVector & commands : ui->frameCanvasCommands)
            {
                ui->frame.metrics.frameMemory += commands.capacity() * sizeof(DrawCommand);
            }
            for(size_t index = 0; index != ui->frameRenderDataCount; ++index)
            {
                const Detail::FrameRenderData & renderData = *ui->frameRenderData[index];
                ui->frame.metrics.frameMemory += renderData.commands.capacity() * sizeof(DrawCommand);
                ui->frame.metrics.frameMemory += Detail::drawCommandStorageMemory(renderData.storage);
            }
            for(size_t index = 0; index != ui->frameNodeStringCount; ++index)
            {
                ui->frame.metrics.frameMemory += ui->frameNodeStrings[index].semanticName.capacity() + ui->frameNodeStrings[index].semanticDescription.capacity() + ui->frameNodeStrings[index].semanticValue.capacity() + ui->frameNodeStrings[index].path.capacity() + ui->frameNodeStrings[index].identityDebugValue.capacity();
            }
            ui->frame.metrics.frameMemory += ui->transientText.capacity() * sizeof(Context::CachedTextPtr);
            ui->frame.metrics.frameMemory += ui->transientTextIndices.capacity() * sizeof(Context::TransientTextIndexEntry);
            for(const Context::CachedTextPtr & text : ui->transientText)
            {
                ui->frame.metrics.frameMemory += Detail::cachedTextMemory(*text);
            }
            for(const Context::TextMeasureCacheEntry & entry : ui->textMeasureCache)
            {
                ui->frame.metrics.frameMemory += entry.text.capacity();
            }
            ui->frame.metrics.generalAllocationCount = ui->allocationTracker->allocationCount();
            ui->frame.metrics.generalAllocationBytes = ui->allocationTracker->allocationBytes();
            ui->frame.metrics.layoutMilliseconds = (layoutEnd - layoutStart) * 1000.0;
            ui->frame.metrics.emitMilliseconds = (emitEnd - layoutEnd) * 1000.0;
            ui->frame.metrics.buildMilliseconds = (layoutStart - ui->frameStarted) * 1000.0;
        }

        if(ui->frameCaptureOptions.semantics == true)
        {
            ui->platform->publishAccessibilityTree(ui->frame.semantics);
        }

        if(ui->itemWidthStack.empty() == false)
        {
            ui->frame.diagnostics.emplace_back("endFrame called with unbalanced pushItemWidth/popItemWidth");
            ui->itemWidthStack.clear();
        }

        if(ui->nextItemWidthPending == true)
        {
            ui->frame.diagnostics.emplace_back("setNextItemWidth was not consumed by a width-aware item");
            ui->nextItemWidthPending = false;
        }

        ui->activeFrame = false;

        return ui->frame;
    }
    //////////////////////////////////////////////////////////////////////////
    const Frame & getFrame(const Context * ui) noexcept
    {
        return ui->frame;
    }
    //////////////////////////////////////////////////////////////////////////
    bool frameActive(const Context * ui) noexcept
    {
        return ui->activeFrame;
    }
    //////////////////////////////////////////////////////////////////////////
    void beginTextLog(Context * ui, TextLogTarget target, StringView filename)
    {
        Mosaic::beginTextLog(ui, target, TextLogOptions{}, filename);
    }
    //////////////////////////////////////////////////////////////////////////
    void beginTextLog(Context * ui, TextLogTarget target, const TextLogOptions & options, StringView filename)
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->textLogEnabled == true)
        {
            return;
        }

        ui->textLogEnabled = true;
        ui->textLogTarget = target;
        ui->textLogStartNode = ui->nodes.size();
        ui->textLogRootNode = ui->currentParent;
        ui->textLogMaximumDepth = options.maximumDepth;
        ui->textLogWindow = options.currentWindowOnly ? ui->currentWindow : InvalidId;
        ui->textLogAutoExpandTrees = options.autoExpandTrees;
        ui->textLogIndent = options.indent;
        ui->textLogFilename.assign(filename);
        ui->textLogBuffer.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    void logText(Context * ui, StringView text)
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->textLogEnabled == false)
        {
            return;
        }

        if(text.empty() == true)
        {
            return;
        }

        ui->textLogBuffer.append(text);
    }
    //////////////////////////////////////////////////////////////////////////
    void finishTextLog(Context * ui)
    {
        if(ui == nullptr)
        {
            return;
        }

        Detail::finishTextLog(ui);
    }
    //////////////////////////////////////////////////////////////////////////
    bool textLogActive(const Context * ui) noexcept
    {
        return ui != nullptr && ui->textLogEnabled;
    }
    //////////////////////////////////////////////////////////////////////////
    void setInputCaptureOverride(Context * ui, const InputCaptureOverride & overrideValue) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->inputCaptureOverride = overrideValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void setTheme(Context * ui, const Theme & theme)
    {
        ui->theme = theme;

        if(ui->activeFrame == false)
        {
            ui->frameTheme = ui->theme;
            ui->frameTheme.metrics.fontSize *= ui->mainFontScale;
            ui->frameTheme.metrics.lineHeight *= ui->mainFontScale;
            ui->currentStyle = &ui->frameTheme;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    const Theme & getTheme(const Context * ui) noexcept
    {
        return ui->theme;
    }
    //////////////////////////////////////////////////////////////////////////
    void setColorEditDefaults(Context * ui, const ColorEditOptions & options) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->defaultColorEditOptions = options;
    }
    //////////////////////////////////////////////////////////////////////////
    bool colorEditDefaults(const Context * ui, ColorEditOptions * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        *_out = ui->defaultColorEditOptions;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setColorPickerDefaults(Context * ui, const ColorPickerOptions & options) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->defaultColorPickerOptions = options;
    }
    //////////////////////////////////////////////////////////////////////////
    bool colorPickerDefaults(const Context * ui, ColorPickerOptions * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        *_out = ui->defaultColorPickerOptions;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setFontScale(Context * ui, float scale) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->mainFontScale = std::max(0.1f, scale);

        if(ui->activeFrame == false)
        {
            ui->frameTheme = ui->theme;
            ui->frameTheme.metrics.fontSize *= ui->mainFontScale;
            ui->frameTheme.metrics.lineHeight *= ui->mainFontScale;
            ui->currentStyle = &ui->frameTheme;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    float fontScale(const Context * ui) noexcept
    {
        if(ui == nullptr)
        {
            return 1.f;
        }

        return ui->mainFontScale;
    }
    //////////////////////////////////////////////////////////////////////////
    void pushFont(Context * ui, FontHandle font, float size, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->activeFrame == false)
        {
            return;
        }

        (void)location;
        Theme fontTheme = *ui->currentStyle;
        fontTheme.metrics.font = font;

        if(size > 0.f)
        {
            float lineHeightRatio = fontTheme.metrics.fontSize > 0.f ? fontTheme.metrics.lineHeight / fontTheme.metrics.fontSize : 1.3f;
            float resolvedSize = size * ui->mainFontScale;
            fontTheme.metrics.fontSize = resolvedSize;
            fontTheme.metrics.lineHeight = std::ceil(resolvedSize * lineHeightRatio);
        }

        uint64_t token = ui->pushScope(ui->currentParent, ui->currentStyle, ui->currentDisabled);
        ui->currentStyle = ui->internStyle(fontTheme);
        ui->fontScopeTokens.push_back(token);
    }
    //////////////////////////////////////////////////////////////////////////
    void popFont(Context * ui) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->fontScopeTokens.empty() == true)
        {
            ui->frame.diagnostics.emplace_back("popFont called without matching pushFont");

            return;
        }

        uint64_t token = ui->fontScopeTokens.back();

        if(ui->scopes.empty() == true || ui->scopes.back().token != token)
        {
            ui->frame.diagnostics.emplace_back("popFont must close the most recent active scope");

            return;
        }

        ui->fontScopeTokens.pop_back();
        ui->closeScope(token);
    }
    //////////////////////////////////////////////////////////////////////////
    void setPlatformAdapter(Context * ui, PlatformAdapter * platform) noexcept
    {
        PlatformAdapter * replacement = platform == nullptr ? &ui->nullPlatform : platform;
        if(ui->platform == replacement)
        {
            return;
        }

        Detail::destroyNativeSurfaces(ui);
        ui->visibleNativeSurfaceNodes.clear();
        ui->platform = replacement;
    }
    //////////////////////////////////////////////////////////////////////////
    void setFontProvider(Context * ui, FontProvider * fontProvider) noexcept
    {
        if(ui->fontProvider == fontProvider)
        {
            return;
        }

        ui->fontProvider = fontProvider;
        ui->clearTextCache();
        ui->textCacheProvider = fontProvider;
        ui->textCacheProviderRevision = fontProvider == nullptr ? 0 : fontProvider->revision();
        for(Context::Node & node : ui->nodes)
        {
            node.textRun = nullptr;
            node.valueTextRun = nullptr;
            node.textPrepared = node.label.empty() == true;
            node.valueTextPrepared = node.valueText.empty() == true;
            Context::ColorTextData * colorText = node.valueData().colorTextDataIndex < ui->frameColorTextData.size() ? &ui->frameColorTextData[node.valueData().colorTextDataIndex] : nullptr;

            if(colorText == nullptr)
            {
                continue;
            }

            colorText->runs = {};
            for(size_t channel = 0; channel != colorText->prepared.size(); ++channel)
            {
                colorText->prepared[channel] = colorText->text[channel].empty() == true;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Scope scope(Context * ui, const Key & key, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Scope, key, key.debug(), {}, location, SemanticRole::Group);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope row(Context * ui, const LayoutOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::row(ui, Key{}, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope row(Context * ui, const Key & key, const LayoutOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout = options;
        layout.orientation = Orientation::Horizontal;
        bool anonymous = key.isExplicit() == false;
        size_t node = ui->addNode(Detail::NodeKind::Row, key, key.debug(), layout, location, SemanticRole::Group, false, anonymous);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope line(Context * ui, const LineOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::line(ui, Key{}, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope line(Context * ui, const Key & key, const LineOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = options.width;
        layout.height = options.height;
        layout.padding.left = std::max(0.f, options.offset);
        layout.gap = options.spacing;
        layout.crossAxisAlignment = options.alignment;
        auto returnedValue = Mosaic::row(ui, key, layout, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void sameLine(Context * ui, const SameLineOptions & options) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->nodes[ui->currentParent].lastChild == std::numeric_limits<size_t>::max())
        {
            return;
        }

        Context::Node & parentNode = ui->nodes[ui->currentParent];
        parentNode.nextSameLine = options;
        parentNode.nextSameLinePending = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void alignTextToFramePadding(Context * ui) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        Context::Node & parentNode = ui->nodes[ui->currentParent];
        parentNode.nextTextAlignToFramePadding = true;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope column(Context * ui, const LayoutOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::column(ui, Key{}, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope column(Context * ui, const Key & key, const LayoutOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout = options;
        layout.orientation = Orientation::Vertical;
        bool anonymous = key.isExplicit() == false;
        size_t node = ui->addNode(Detail::NodeKind::Column, key, key.debug(), layout, location, SemanticRole::Group, false, anonymous);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope grid(Context * ui, uint32_t columns, const LayoutOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::grid(ui, Key{}, columns, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope grid(Context * ui, const Key & key, uint32_t columns, const LayoutOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout = options;
        layout.columns = std::max(1U, columns);
        bool anonymous = key.isExplicit() == false;
        size_t node = ui->addNode(Detail::NodeKind::Grid, key, key.debug(), layout, location, SemanticRole::Group, false, anonymous);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope overlay(Context * ui, const LayoutOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::overlay(ui, Key{}, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope overlay(Context * ui, const Key & key, const LayoutOptions & options, const SourceLocation & location)
    {
        bool anonymous = key.isExplicit() == false;
        size_t node = ui->addNode(Detail::NodeKind::Overlay, key, key.debug(), options, location, SemanticRole::Group, false, anonymous);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope scrollArea(Context * ui, StringView label, Orientation orientation, const LayoutOptions & options, const SourceLocation & location)
    {
        ScrollOptions scrollOptions;
        scrollOptions.axes = orientation == Orientation::Vertical ? ScrollAxes::Vertical : ScrollAxes::Horizontal;
        scrollOptions.contentOrientation = orientation;
        auto returnedValue = Mosaic::scrollArea(ui, Key{}, label, scrollOptions, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope scrollArea(Context * ui, const Key & key, StringView label, Orientation orientation, const LayoutOptions & options, const SourceLocation & location)
    {
        ScrollOptions scrollOptions;
        scrollOptions.axes = orientation == Orientation::Vertical ? ScrollAxes::Vertical : ScrollAxes::Horizontal;
        scrollOptions.contentOrientation = orientation;
        auto returnedValue = Mosaic::scrollArea(ui, key, label, scrollOptions, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope scrollArea(Context * ui, StringView label, const ScrollOptions & scrollOptions, const LayoutOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::scrollArea(ui, Key{}, label, scrollOptions, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope scrollArea(Context * ui, const Key & key, StringView label, const ScrollOptions & scrollOptions, const LayoutOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout = options;
        layout.orientation = scrollOptions.contentOrientation;
        size_t node = ui->addNode(Detail::NodeKind::Scroll, key, label, layout, location, SemanticRole::Group, true);
        Context::Node & scrollNode = ui->nodes[node];
        scrollNode.mutableScrollOptions() = scrollOptions;
        scrollNode.focusable = scrollOptions.navigationFlattened == false;

        if(scrollOptions.autoResizeY == true || scrollOptions.alwaysAutoResize == true)
        {
            scrollNode.layout.height = SizeRule::Content;
            scrollNode.layout.minimum.y = std::max(scrollNode.layout.minimum.y, scrollOptions.minimumSize.y);
            scrollNode.layout.maximum.y = std::min(scrollNode.layout.maximum.y, scrollOptions.maximumSize.y);
        }

        if(scrollOptions.autoResizeX == true || scrollOptions.alwaysAutoResize == true)
        {
            scrollNode.layout.width = SizeRule::Content;
            scrollNode.layout.minimum.x = std::max(scrollNode.layout.minimum.x, scrollOptions.minimumSize.x);
            scrollNode.layout.maximum.x = std::min(scrollNode.layout.maximum.x, scrollOptions.maximumSize.x);
        }

        Context::Persistent & persistentState = ui->state(scrollNode);
        const PointerState * pointer = ui->input.primaryPointer();

        if((scrollOptions.resizeX == true || scrollOptions.resizeY == true) && persistentState.scrollData().areaSizeInitialized == false)
        {
            persistentState.scrollData().areaSize.x = persistentState.lastBounds.empty() == false ? persistentState.lastBounds.width : (layout.width.rule == SizeRule::Fixed ? layout.width.value : scrollOptions.minimumSize.x);
            persistentState.scrollData().areaSize.y = persistentState.lastBounds.empty() == false ? persistentState.lastBounds.height : (layout.height.rule == SizeRule::Fixed ? layout.height.value : scrollOptions.minimumSize.y);
            persistentState.scrollData().areaSizeInitialized = true;
        }

        if(scrollOptions.resizeX == true)
        {
            scrollNode.layout.width = Dimension::fixed(std::clamp(persistentState.scrollData().areaSize.x, scrollOptions.minimumSize.x, scrollOptions.maximumSize.x));
        }

        if(scrollOptions.resizeY == true)
        {
            scrollNode.layout.height = Dimension::fixed(std::clamp(persistentState.scrollData().areaSize.y, scrollOptions.minimumSize.y, scrollOptions.maximumSize.y));
        }

        Rect hitBounds;
        uint8_t hitAxis = 0;
        uint8_t resizeEdges = 0;
        constexpr float ResizeHitThickness = 6.f;

        if(pointer != nullptr && persistentState.lastBounds.empty() == false)
        {
            const Rect & bounds = persistentState.lastBounds;
            bool resizeXHit = scrollOptions.resizeX;

            if(std::abs(pointer->position.x - bounds.right()) > ResizeHitThickness)
            {
                resizeXHit = false;
            }

            if(pointer->position.y < bounds.y - ResizeHitThickness)
            {
                resizeXHit = false;
            }

            if(pointer->position.y > bounds.bottom() + ResizeHitThickness)
            {
                resizeXHit = false;
            }

            if(resizeXHit == true)
            {
                resizeEdges = static_cast<uint8_t>(resizeEdges | Detail::WindowResizeRight);
            }

            bool resizeYHit = scrollOptions.resizeY;

            if(std::abs(pointer->position.y - bounds.bottom()) > ResizeHitThickness)
            {
                resizeYHit = false;
            }

            if(pointer->position.x < bounds.x - ResizeHitThickness)
            {
                resizeYHit = false;
            }

            if(pointer->position.x > bounds.right() + ResizeHitThickness)
            {
                resizeYHit = false;
            }

            if(resizeYHit == true)
            {
                resizeEdges = static_cast<uint8_t>(resizeEdges | Detail::WindowResizeBottom);
            }

            if((resizeEdges & Detail::WindowResizeRight) != 0 && (resizeEdges & Detail::WindowResizeBottom) != 0)
            {
                hitBounds = {bounds.right() - ResizeHitThickness, bounds.bottom() - ResizeHitThickness, ResizeHitThickness * 2.f, ResizeHitThickness * 2.f};
            }
            else if((resizeEdges & Detail::WindowResizeRight) != 0)
            {
                hitBounds = {bounds.right() - ResizeHitThickness, bounds.y, ResizeHitThickness * 2.f, bounds.height};
            }
            else if((resizeEdges & Detail::WindowResizeBottom) != 0)
            {
                hitBounds = {bounds.x, bounds.bottom() - ResizeHitThickness, bounds.width, ResizeHitThickness * 2.f};
            }
        }

        if(resizeEdges == 0 && pointer != nullptr && persistentState.scrollData().verticalTrack.contains(pointer->position) == true)
        {
            hitBounds = persistentState.scrollData().verticalTrack;
            hitAxis = 2;
        }
        else if(resizeEdges == 0 && pointer != nullptr && persistentState.scrollData().horizontalTrack.contains(pointer->position) == true)
        {
            hitBounds = persistentState.scrollData().horizontalTrack;
            hitAxis = 1;
        }

        Response response = ui->interact(node, false, &hitBounds);

        if(pointer != nullptr && response.pressed() == true && resizeEdges != 0 && pointer->buttonClickCount() >= 2)
        {
            Vec2 content = persistentState.scrollData().contentSize;

            if((resizeEdges & Detail::WindowResizeRight) != 0)
            {
                persistentState.scrollData().areaSize.x = std::clamp(content.x + scrollNode.layout.padding.left + scrollNode.layout.padding.right, scrollOptions.minimumSize.x, scrollOptions.maximumSize.x);
                scrollNode.layout.width = Dimension::fixed(persistentState.scrollData().areaSize.x);
            }

            if((resizeEdges & Detail::WindowResizeBottom) != 0)
            {
                persistentState.scrollData().areaSize.y = std::clamp(content.y + scrollNode.layout.padding.top + scrollNode.layout.padding.bottom, scrollOptions.minimumSize.y, scrollOptions.maximumSize.y);
                scrollNode.layout.height = Dimension::fixed(persistentState.scrollData().areaSize.y);
            }

            persistentState.scrollData().resizingArea = false;
            persistentState.scrollData().resizeEdges = 0;
            Detail::setFlag(response, 6);
        }
        else if(pointer != nullptr && response.pressed() == true && resizeEdges != 0)
        {
            persistentState.scrollData().resizingArea = true;
            persistentState.scrollData().resizeEdges = resizeEdges;
            persistentState.dragStartPosition = pointer->position;
            persistentState.scrollData().resizeStartSize = {persistentState.lastBounds.width, persistentState.lastBounds.height};
        }
        else if(pointer != nullptr && response.pressed() == true && hitAxis != 0)
        {
            bool vertical = hitAxis == 2;
            const Rect & thumb = vertical ? persistentState.scrollData().verticalThumb : persistentState.scrollData().horizontalThumb;
            bool clickedThumb = thumb.contains(pointer->position);

            if(clickedThumb == true)
            {
                float thumbStart = vertical ? thumb.y : thumb.x;
                float pointerPosition = vertical ? pointer->position.y : pointer->position.x;
                persistentState.scrollData().dragOffset = pointerPosition - thumbStart;
                persistentState.scrollData().draggingScrollbar = true;
                persistentState.scrollData().draggingAxis = hitAxis;
            }
            else
            {
                float pointerPosition = vertical ? pointer->position.y : pointer->position.x;
                float thumbStart = vertical ? thumb.y : thumb.x;
                float page = vertical ? scrollNode.bounds.height : scrollNode.bounds.width;
                float & position = vertical ? persistentState.scrollData().position.y : persistentState.scrollData().position.x;
                float extent = vertical ? persistentState.scrollData().range.y : persistentState.scrollData().range.x;

                if(ui->configuration.scrollbarScrollByPage == true)
                {
                    position = std::clamp(position + (pointerPosition < thumbStart ? -page : page), 0.f, extent);
                }
                else
                {
                    const Rect & track = vertical ? persistentState.scrollData().verticalTrack : persistentState.scrollData().horizontalTrack;
                    float trackStart = vertical ? track.y : track.x;
                    float trackExtent = vertical ? track.height : track.width;
                    float thumbExtent = vertical ? thumb.height : thumb.width;
                    float travel = std::max(0.f, trackExtent - thumbExtent);
                    position = travel <= 0.f ? 0.f : std::clamp((pointerPosition - trackStart - thumbExtent * 0.5f) / travel, 0.f, 1.f) * extent;
                }

                persistentState.scrollData().target = persistentState.scrollData().position;
                persistentState.scrollData().velocity = {};
                persistentState.scrollData().targetInitialized = true;
                Detail::setFlag(response, 6);
            }
        }

        if(pointer != nullptr && ui->captured == scrollNode.id && persistentState.scrollData().draggingScrollbar == true && pointer->isDown() == true)
        {
            bool vertical = persistentState.scrollData().draggingAxis == 2;
            const Rect & track = vertical ? persistentState.scrollData().verticalTrack : persistentState.scrollData().horizontalTrack;
            const Rect & thumb = vertical ? persistentState.scrollData().verticalThumb : persistentState.scrollData().horizontalThumb;
            float & position = vertical ? persistentState.scrollData().position.y : persistentState.scrollData().position.x;
            float previous = position;
            position = Detail::scrollbarScrollAtPointer(track, thumb, vertical ? persistentState.scrollData().range.y : persistentState.scrollData().range.x, vertical, persistentState.scrollData().dragOffset, pointer->position);
            persistentState.scrollData().target = persistentState.scrollData().position;
            persistentState.scrollData().velocity = {};
            persistentState.scrollData().targetInitialized = true;

            if(position != previous)
            {
                Detail::setFlag(response, 6);
            }
        }

        if(pointer != nullptr && ui->captured == scrollNode.id && persistentState.scrollData().resizingArea == true && pointer->isDown() == true)
        {
            Vec2 delta = pointer->position - persistentState.dragStartPosition;

            if((persistentState.scrollData().resizeEdges & Detail::WindowResizeRight) != 0)
            {
                persistentState.scrollData().areaSize.x = std::clamp(persistentState.scrollData().resizeStartSize.x + delta.x, scrollOptions.minimumSize.x, scrollOptions.maximumSize.x);
                scrollNode.layout.width = Dimension::fixed(persistentState.scrollData().areaSize.x);
            }

            if((persistentState.scrollData().resizeEdges & Detail::WindowResizeBottom) != 0)
            {
                persistentState.scrollData().areaSize.y = std::clamp(persistentState.scrollData().resizeStartSize.y + delta.y, scrollOptions.minimumSize.y, scrollOptions.maximumSize.y);
                scrollNode.layout.height = Dimension::fixed(persistentState.scrollData().areaSize.y);
            }

            Detail::setFlag(response, 6);
        }

        if(scrollOptions.keyboard == true && ui->focused == scrollNode.id)
        {
            Vec2 delta;
            float pageY = std::max(1.f, persistentState.lastBounds.height);
            float keyStep = std::max(12.f, scrollOptions.wheelStep);

            if(ui->input.keyPressed(KeyCode::Left) == true)
            {
                delta.x = -keyStep;
            }

            if(ui->input.keyPressed(KeyCode::Right) == true)
            {
                delta.x = keyStep;
            }

            if(ui->input.keyPressed(KeyCode::Up) == true)
            {
                delta.y = -keyStep;
            }

            if(ui->input.keyPressed(KeyCode::Down) == true)
            {
                delta.y = keyStep;
            }

            if(ui->input.keyPressed(KeyCode::PageUp) == true)
            {
                delta.y = -pageY;
            }

            if(ui->input.keyPressed(KeyCode::PageDown) == true)
            {
                delta.y = pageY;
            }

            if(ui->input.keyPressed(KeyCode::Home) == true)
            {
                persistentState.scrollData().position = {};
                persistentState.scrollData().target = {};
                persistentState.scrollData().velocity = {};
                persistentState.scrollData().targetInitialized = true;
                Detail::setFlag(response, 6);
            }

            if(ui->input.keyPressed(KeyCode::End) == true)
            {
                persistentState.scrollData().position = persistentState.scrollData().range;
                persistentState.scrollData().target = persistentState.scrollData().position;
                persistentState.scrollData().velocity = {};
                persistentState.scrollData().targetInitialized = true;
                Detail::setFlag(response, 6);
            }

            if(delta != Vec2{})
            {
                Vec2 updated = {std::clamp(persistentState.scrollData().position.x + delta.x, 0.f, persistentState.scrollData().range.x), std::clamp(persistentState.scrollData().position.y + delta.y, 0.f, persistentState.scrollData().range.y)};
                Detail::setFlag(response, 6, updated != persistentState.scrollData().position);
                persistentState.scrollData().position = updated;
                persistentState.scrollData().target = updated;
                persistentState.scrollData().velocity = {};
                persistentState.scrollData().targetInitialized = true;
            }
        }

        if(response.released() == true)
        {
            persistentState.scrollData().draggingScrollbar = false;
            persistentState.scrollData().draggingAxis = 0;
            persistentState.scrollData().resizingArea = false;
            persistentState.scrollData().resizeEdges = 0;
        }

        if(response.changed() == true)
        {
            persistentState.scrollData().value = scrollOptions.axes != ScrollAxes::Horizontal ? persistentState.scrollData().position.y : persistentState.scrollData().position.x;
            ui->frame.events.push_back({EventType::Change, scrollNode.id, ui->nodePath(scrollNode), scrollNode.debugData().file, scrollNode.debugData().line, ui->input.timestamp});
        }

        scrollNode.scrollResizeHovered = resizeEdges != 0;
        scrollNode.scrollResizeEdges = persistentState.scrollData().resizingArea ? persistentState.scrollData().resizeEdges : resizeEdges;
        scrollNode.response = response;
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, scrollNode.id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope split(Context * ui, StringView label, Orientation orientation, float ratio, const LayoutOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::split(ui, Key{}, label, orientation, ratio, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope split(Context * ui, const Key & key, StringView label, Orientation orientation, float ratio, const LayoutOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout = options;
        layout.orientation = orientation;
        layout.splitRatio = std::clamp(ratio, 0.05f, 0.95f);
        size_t node = ui->addNode(Detail::NodeKind::Split, key, label, layout, location, SemanticRole::Group);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope split(Context * ui, StringView label, Orientation orientation, float * ratio, const SplitOptions & splitOptions, const LayoutOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::split(ui, Key{}, label, orientation, ratio, splitOptions, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope split(Context * ui, const Key & key, StringView label, Orientation orientation, float * ratio, const SplitOptions & splitOptions, const LayoutOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout = options;
        layout.orientation = orientation;
        layout.splitRatio = std::clamp(ratio == nullptr ? 0.5f : *ratio, 0.f, 1.f);
        size_t node = ui->addNode(Detail::NodeKind::Split, key, label, layout, location, SemanticRole::Group);
        Context::Node & splitNode = ui->nodes[node];
        splitNode.valueData().splitMinimumFirst = std::max(0.f, splitOptions.minimumFirst);
        splitNode.valueData().splitMinimumSecond = std::max(0.f, splitOptions.minimumSecond);

        Context::Persistent & persistentState = ui->state(splitNode);
        persistentState.splitterOrientation = orientation;
        persistentState.splitterSnapIndex = splitOptions.snapIndex;
        Rect splitterHitBounds = persistentState.splitterBounds;

        if(splitterHitBounds.empty() == false)
        {
            constexpr float splitterHitPadding = 2.f;

            if(orientation == Orientation::Horizontal)
            {
                splitterHitBounds.x -= splitterHitPadding;
                splitterHitBounds.width += splitterHitPadding * 2.f;
            }
            else
            {
                splitterHitBounds.y -= splitterHitPadding;
                splitterHitBounds.height += splitterHitPadding * 2.f;
            }
        }

        Response response = ui->interact(node, false, &splitterHitBounds);
        const PointerState * pointer = ui->input.primaryPointer();

        if(ratio != nullptr && pointer != nullptr && ui->captured == splitNode.id && pointer->isDown() == true)
        {
            float splitterWidth = splitNode.style->metrics.splitterWidth;
            float available = orientation == Orientation::Horizontal ? persistentState.lastBounds.width - splitterWidth : persistentState.lastBounds.height - splitterWidth;

            if(available > 0.f)
            {
                float pointerCenter = orientation == Orientation::Horizontal ? pointer->position.x : pointer->position.y;
                float snapDistance = std::max(8.f, splitterWidth * 2.f);
                float snappedCenter = Detail::snapSplitterCenter(ui, splitNode.id, orientation, splitOptions.snapIndex, pointerCenter, snapDistance);
                float position = orientation == Orientation::Horizontal ? snappedCenter - persistentState.lastBounds.x - splitterWidth * 0.5f : snappedCenter - persistentState.lastBounds.y - splitterWidth * 0.5f;
                float minimumRatio = std::clamp(splitNode.valueData().splitMinimumFirst / available, 0.f, 1.f);
                float maximumRatio = std::clamp(1.f - splitNode.valueData().splitMinimumSecond / available, 0.f, 1.f);
                float updated = std::clamp(position / available, std::min(minimumRatio, maximumRatio), std::max(minimumRatio, maximumRatio));

                if(updated != *ratio)
                {
                    *ratio = updated;
                    splitNode.layout.splitRatio = updated;
                    Detail::setFlag(response, 6);
                    splitNode.response = response;
                    ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(splitNode), splitNode.debugData().file, splitNode.debugData().line, ui->input.timestamp});
                }
            }
        }

        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, splitNode.id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope absolute(Context * ui, const LayoutOptions & options, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Absolute, {}, {}, options, location, SemanticRole::Group, false, true);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope clip(Context * ui, const Rect & rect, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = SizeRule::Fill;
        layout.height = SizeRule::Fill;
        size_t node = ui->addNode(Detail::NodeKind::Clip, Key(ui->anonymousCounter++), "clip", layout, location);
        ui->nodes[node].explicitClip = rect;
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope disabledScope(Context * ui, bool disabled, const SourceLocation & location)
    {
        (void)location;
        bool previousDisabled = ui->currentDisabled;
        size_t parent = ui->currentParent;
        uint64_t token = ui->pushScope(parent, ui->currentStyle, previousDisabled);
        ui->currentDisabled = previousDisabled || disabled;

        return {ui, token, ui->nodes[parent].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope interactionScope(Context * ui, bool enabled, const SourceLocation & location)
    {
        (void)location;
        bool previousInputBlocked = ui->currentInputBlocked;
        size_t parent = ui->currentParent;
        uint64_t token = ui->pushScope(parent, ui->currentStyle, ui->currentDisabled);
        ui->currentInputBlocked = previousInputBlocked || enabled == false;

        return {ui, token, ui->nodes[parent].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope liveEditScope(Context * ui, const LiveEditOptions & options, const SourceLocation & location)
    {
        (void)location;
        size_t parent = ui->currentParent;
        uint64_t token = ui->pushScope(parent, ui->currentStyle, ui->currentDisabled);
        ui->currentLiveEditText = options.text;
        ui->currentLiveEditScalar = options.scalar;

        return {ui, token, ui->nodes[parent].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Scope styleScope(Context * ui, const Theme & theme, const SourceLocation & location)
    {
        (void)location;
        const Theme * previousStyle = ui->currentStyle;
        size_t parent = ui->currentParent;
        uint64_t token = ui->pushScope(parent, previousStyle, ui->currentDisabled);
        Theme resolvedTheme = theme;
        resolvedTheme.metrics.fontSize *= ui->mainFontScale;
        resolvedTheme.metrics.lineHeight *= ui->mainFontScale;
        ui->currentStyle = ui->internStyle(resolvedTheme);

        return {ui, token, ui->nodes[parent].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    void setNextItemWidth(Context * ui, float width) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->activeFrame == false)
        {
            return;
        }

        ui->nextItemWidth = width;
        ui->nextItemWidthPending = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void pushItemWidth(Context * ui, float width)
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->activeFrame == false)
        {
            return;
        }

        ui->itemWidthStack.push_back(width);
    }
    //////////////////////////////////////////////////////////////////////////
    void popItemWidth(Context * ui) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->activeFrame == false)
        {
            return;
        }

        if(ui->itemWidthStack.empty() == true)
        {
            ui->frame.diagnostics.emplace_back("popItemWidth called without a matching pushItemWidth");

            return;
        }

        ui->itemWidthStack.pop_back();
    }
    //////////////////////////////////////////////////////////////////////////
    Response property(Context * ui, StringView name, bool * value, const SourceLocation & location)
    {
        LayoutOptions rowLayout;
        rowLayout.width = SizeRule::Fill;
        auto propertyRow = Mosaic::row(ui, rowLayout, location);
        TextOptions labelOptions;
        labelOptions.layout.width = Dimension::fixed(ui->currentStyle->metrics.propertyLabelWidth);
        Mosaic::text(ui, name, labelOptions, location);
        auto returnedValue = Mosaic::checkbox(ui, Key("value"), {}, value, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response property(Context * ui, StringView name, String * value, const SourceLocation & location)
    {
        LayoutOptions rowLayout;
        rowLayout.width = SizeRule::Fill;
        auto propertyRow = Mosaic::row(ui, rowLayout, location);
        TextOptions labelOptions;
        labelOptions.layout.width = Dimension::fixed(ui->currentStyle->metrics.propertyLabelWidth);
        Mosaic::text(ui, name, labelOptions, location);
        auto returnedValue = Mosaic::inputText(ui, "value", value, {}, location);
        if(Context::Node * node = ui->findFrameNode(returnedValue.item); node != nullptr)
        {
            node->layout.width = SizeRule::Fill;
        }

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool visibleRange(const Context * ui, size_t itemCount, float itemHeight, VisibleRange * const _out, Id scrollArea) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        if(itemCount == 0)
        {

            *_out = {};

            return true;
        }

        if(itemHeight <= 0.f)
        {
            return false;
        }

        float scroll = 0.f;
        float height = ui->viewport.bounds.height;

        if(scrollArea != InvalidId)
        {
            if(const Context::Persistent * persistentState = ui->findState(scrollArea); persistentState != nullptr)
            {
                scroll = persistentState->scrollData().position.y;
                height = persistentState->lastBounds.height;
            }
        }

        size_t begin = std::min(itemCount, static_cast<size_t>(std::max(0.f, std::floor(scroll / itemHeight))));
        size_t count = static_cast<size_t>(std::ceil(std::max(0.f, height) / itemHeight)) + 1;
        size_t end = std::min(itemCount, begin + count);
        VisibleRange result = {begin, end};

        *_out = result;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool beginListClipper(Context * ui, size_t itemCount, float itemExtent, VisibleRange * const _out, Id scrollArea, const ListClipperOptions & options, const SourceLocation & location)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        if(itemCount == 0)
        {

            *_out = {};

            return true;
        }

        if(itemExtent <= 0.f)
        {
            return false;
        }

        if(scrollArea == InvalidId && Detail::scrollsContent(ui->nodes[ui->currentParent]) == true)
        {
            scrollArea = ui->nodes[ui->currentParent].id;
        }

        float scroll = 0.f;
        float viewportExtent = options.orientation == Orientation::Vertical ? ui->viewport.bounds.height : ui->viewport.bounds.width;

        if(scrollArea != InvalidId)
        {
            if(const Context::Persistent * persistentState = ui->findState(scrollArea); persistentState != nullptr)
            {
                scroll = options.orientation == Orientation::Vertical ? persistentState->scrollData().position.y : persistentState->scrollData().position.x;
                float previousExtent = options.orientation == Orientation::Vertical ? persistentState->lastBounds.height : persistentState->lastBounds.width;

                if(previousExtent > 0.f)
                {
                    viewportExtent = previousExtent;
                }
            }
        }

        float spacing = options.spacing >= 0.f ? options.spacing : ui->gap(ui->nodes[ui->currentParent]);
        float stride = std::max(itemExtent + spacing, 0.001f);
        size_t begin = std::min(itemCount, static_cast<size_t>(std::floor(std::max(0.f, scroll) / stride)));
        size_t end = std::min(itemCount, begin + static_cast<size_t>(std::ceil(std::max(0.f, viewportExtent) / stride)) + 1);
        begin = begin > options.overscan ? begin - options.overscan : 0;
        end = std::min(itemCount, end + options.overscan);

        if(begin != 0)
        {
            Detail::addListClipperSpacer(ui, std::max(0.f, static_cast<float>(begin) * stride - spacing), options.orientation, location);
        }

        VisibleRange range = {begin, end};

        *_out = range;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void endListClipper(Context * ui, const VisibleRange & range, size_t itemCount, float itemExtent, const ListClipperOptions & options, const SourceLocation & location)
    {
        if(itemExtent <= 0.f)
        {
            return;
        }

        if(range.end >= itemCount)
        {
            return;
        }

        float spacing = options.spacing >= 0.f ? options.spacing : ui->gap(ui->nodes[ui->currentParent]);
        float stride = std::max(itemExtent + spacing, 0.001f);
        Detail::addListClipperSpacer(ui, std::max(0.f, static_cast<float>(itemCount - range.end) * stride - spacing), options.orientation, location);
    }
    //////////////////////////////////////////////////////////////////////////
    void scrollTo(Context * ui, Id scrollArea, float offset) noexcept
    {
        Context::Persistent & persistentState = ui->state(scrollArea);

        if(persistentState.scrollData().targetInitialized == false)
        {
            persistentState.scrollData().target = persistentState.scrollData().position;
        }

        float target = std::clamp(offset, 0.f, persistentState.scrollData().extent);

        if(persistentState.scrollData().orientation == Orientation::Vertical)
        {
            persistentState.scrollData().target.y = target;
            persistentState.scrollData().toEndY = false;
        }
        else
        {
            persistentState.scrollData().target.x = target;
            persistentState.scrollData().toEndX = false;
        }

        persistentState.scrollData().targetInitialized = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void scrollTo(Context * ui, Id scrollArea, const Vec2 & offset) noexcept
    {
        Context::Persistent & persistentState = ui->state(scrollArea);
        persistentState.scrollData().toEndX = false;
        persistentState.scrollData().toEndY = false;
        persistentState.scrollData().target = {std::clamp(offset.x, 0.f, persistentState.scrollData().range.x), std::clamp(offset.y, 0.f, persistentState.scrollData().range.y)};
        persistentState.scrollData().targetInitialized = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void scrollToEnd(Context * ui, Id scrollArea, ScrollAxes axes) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(scrollArea == InvalidId)
        {
            return;
        }

        Context::Persistent & persistentState = ui->state(scrollArea);
        persistentState.scrollData().toEndX = axes == ScrollAxes::Horizontal || axes == ScrollAxes::Both;
        persistentState.scrollData().toEndY = axes == ScrollAxes::Vertical || axes == ScrollAxes::Both;
    }
    //////////////////////////////////////////////////////////////////////////
    void scrollToItem(Context * ui, Id scrollArea, Id item) noexcept
    {
        Context::Persistent & scroll = ui->state(scrollArea);
        const Context::Persistent * target = ui->findState(item);

        if(target == nullptr)
        {
            return;
        }

        if(scroll.lastBounds.empty() == true)
        {
            return;
        }

        if(target->lastBounds.empty() == true)
        {
            return;
        }

        Vec2 targetPosition = scroll.scrollData().position;

        if(target->lastBounds.x < scroll.lastBounds.x)
        {
            targetPosition.x += target->lastBounds.x - scroll.lastBounds.x;
        }
        else if(target->lastBounds.right() > scroll.lastBounds.right())
        {
            targetPosition.x += target->lastBounds.right() - scroll.lastBounds.right();
        }

        if(target->lastBounds.y < scroll.lastBounds.y)
        {
            targetPosition.y += target->lastBounds.y - scroll.lastBounds.y;
        }
        else if(target->lastBounds.bottom() > scroll.lastBounds.bottom())
        {
            targetPosition.y += target->lastBounds.bottom() - scroll.lastBounds.bottom();
        }

        scroll.scrollData().target = {std::clamp(targetPosition.x, 0.f, scroll.scrollData().range.x), std::clamp(targetPosition.y, 0.f, scroll.scrollData().range.y)};
        scroll.scrollData().targetInitialized = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void scrollToItem(Context * ui, Id scrollArea, Id item, const Vec2 & alignment) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(scrollArea == InvalidId)
        {
            return;
        }

        if(item == InvalidId)
        {
            return;
        }

        Context::Persistent & scroll = ui->state(scrollArea);
        const Context::Persistent * target = ui->findState(item);

        if(target == nullptr)
        {
            return;
        }

        if(scroll.lastBounds.empty() == true)
        {
            return;
        }

        if(target->lastBounds.empty() == true)
        {
            return;
        }

        Vec2 resolvedAlignment = {std::clamp(alignment.x, 0.f, 1.f), std::clamp(alignment.y, 0.f, 1.f)};
        Vec2 targetPosition = {scroll.scrollData().position.x + target->lastBounds.x - scroll.lastBounds.x - resolvedAlignment.x * std::max(0.f, scroll.lastBounds.width - target->lastBounds.width), scroll.scrollData().position.y + target->lastBounds.y - scroll.lastBounds.y - resolvedAlignment.y * std::max(0.f, scroll.lastBounds.height - target->lastBounds.height)};
        scroll.scrollData().target = {std::clamp(targetPosition.x, 0.f, scroll.scrollData().range.x), std::clamp(targetPosition.y, 0.f, scroll.scrollData().range.y)};
        scroll.scrollData().targetInitialized = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void scrollToPosition(Context * ui, Id scrollArea, const Vec2 & position, const Vec2 & alignment) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(scrollArea == InvalidId)
        {
            return;
        }

        Context::Persistent & scroll = ui->state(scrollArea);
        Vec2 resolvedAlignment = {std::clamp(alignment.x, 0.f, 1.f), std::clamp(alignment.y, 0.f, 1.f)};
        scroll.scrollData().target = {std::clamp(position.x - resolvedAlignment.x * scroll.lastBounds.width, 0.f, scroll.scrollData().range.x), std::clamp(position.y - resolvedAlignment.y * scroll.lastBounds.height, 0.f, scroll.scrollData().range.y)};
        scroll.scrollData().targetInitialized = true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool scrollOffset(const Context * ui, Id scrollArea, Vec2 * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::Persistent * persistentState = ui->findState(scrollArea);

        if(persistentState == nullptr)
        {
            return false;
        }

        *_out = persistentState->scrollData().position;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool scrollRange(const Context * ui, Id scrollArea, Vec2 * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::Persistent * persistentState = ui->findState(scrollArea);

        if(persistentState == nullptr)
        {
            return false;
        }

        *_out = persistentState->scrollData().range;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setTreeExpanded(Context * ui, Id tree, bool expanded) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(tree == InvalidId)
        {
            return;
        }

        Context::Persistent & persistentState = ui->state(tree);
        persistentState.expanded = expanded;
        persistentState.expandedInitialized = true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool treeExpanded(const Context * ui, Id tree) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(tree == InvalidId)
        {
            return false;
        }

        const Context::Persistent * persistentState = ui->findState(tree);

        return persistentState != nullptr && persistentState->expanded;
    }
    //////////////////////////////////////////////////////////////////////////
    void setWindowBounds(Context * ui, Id window, const Rect & bounds) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(window == InvalidId)
        {
            return;
        }

        Context::Persistent & persistentState = ui->state(window);
        persistentState.windowData().bounds = bounds;
        persistentState.windowData().initialized = true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool windowBounds(const Context * ui, Id window, Rect * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(window == InvalidId)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::Persistent * persistentState = ui->findState(window);

        if(persistentState == nullptr)
        {
            return false;
        }

        *_out = persistentState->windowData().bounds;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool windowFocused(const Context * ui, Id window) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(window == InvalidId)
        {
            return false;
        }

        if(ui->focused == window)
        {
            return true;
        }

        if(ui->navigationFocused == window)
        {
            return true;
        }

        const Context::Node * focusedNode = ui->findFrameNode(ui->focused);

        if(focusedNode != nullptr && focusedNode->windowOwner == window)
        {
            return true;
        }

        const Context::Node * navigationNode = ui->findFrameNode(ui->navigationFocused);

        return navigationNode != nullptr && navigationNode->windowOwner == window;
    }
    //////////////////////////////////////////////////////////////////////////
    bool windowHovered(const Context * ui, Id window) noexcept
    {
        return ui != nullptr && window != InvalidId && ui->pointerWindow == window;
    }
    //////////////////////////////////////////////////////////////////////////
    void focus(Context * ui, Id id) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->focused = id;
        ui->navigationFocused = id;
    }
    //////////////////////////////////////////////////////////////////////////
    void focusNextItem(Context * ui, int32_t offset) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(offset < 0)
        {
            if(ui->focusOrder.empty() == false)
            {
                ui->focused = ui->focusOrder.back();
                ui->navigationFocused = ui->focusOrder.back();
            }

            ui->focusNextPending = false;

            return;
        }

        ui->focusNextOffset = offset;
        ui->focusNextPending = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setItemDefaultFocus(Context * ui, Id id) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(id == InvalidId)
        {
            return;
        }

        const Context::Node * current = ui->findFrameNode(ui->navigationFocused);

        if(current != nullptr && current->inputLayer == ui->currentInputLayer)
        {
            return;
        }

        const Context::Node * node = ui->findFrameNode(id);

        if(node == nullptr)
        {
            return;
        }

        if(node->disabled == true)
        {
            return;
        }

        if(node->inputBlocked == true)
        {
            return;
        }

        if(node->navigationBlocked == true)
        {
            return;
        }

        ui->navigationFocused = id;

        if(ui->focused == InvalidId)
        {
            ui->focused = id;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Id focused(const Context * ui) noexcept
    {
        return ui->focused;
    }
    //////////////////////////////////////////////////////////////////////////
    Id currentWindow(const Context * ui) noexcept
    {
        return ui == nullptr ? InvalidId : ui->currentWindow;
    }
    //////////////////////////////////////////////////////////////////////////
    Id capturedPointerOwner(const Context * ui) noexcept
    {
        return ui == nullptr ? InvalidId : ui->captured;
    }
    //////////////////////////////////////////////////////////////////////////
    Id navigationFocus(const Context * ui) noexcept
    {
        return ui == nullptr ? InvalidId : ui->navigationFocused;
    }
    //////////////////////////////////////////////////////////////////////////
    void setItemCursor(Context * ui, Id item, CursorShape shape) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(item == InvalidId)
        {
            return;
        }

        Context::Node * node = ui->findFrameNode(item);

        if(node == nullptr)
        {
            return;
        }

        node->cursorOverride = true;
        node->cursor = shape;
    }
    //////////////////////////////////////////////////////////////////////////
    CursorShape cursorShape(const Context * ui) noexcept
    {
        return ui == nullptr ? CursorShape::Arrow : ui->currentCursor;
    }
    //////////////////////////////////////////////////////////////////////////
    bool currentViewport(const Context * ui, Viewport * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        *_out = ui->viewport;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    ShortcutRegistry & shortcuts(Context * ui) noexcept
    {
        return ui->shortcuts;
    }
    //////////////////////////////////////////////////////////////////////////
    const ShortcutRegistry & shortcuts(const Context * ui) noexcept
    {
        return ui->shortcuts;
    }
    //////////////////////////////////////////////////////////////////////////
    PlatformAdapter & platform(Context * ui) noexcept
    {
        return *ui->platform;
    }
    //////////////////////////////////////////////////////////////////////////
    const Input & input(const Context * ui) noexcept
    {
        return ui->input;
    }
    //////////////////////////////////////////////////////////////////////////
    bool keyDown(const Context * ui, KeyCode key) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        size_t index = static_cast<size_t>(key);
        auto returnedValue = index < ui->keyDownStates.size() && ui->keyDownStates[index];

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    float keyDownDuration(const Context * ui, KeyCode key) noexcept
    {
        if(ui == nullptr)
        {
            return 0.f;
        }

        size_t index = static_cast<size_t>(key);
        auto returnedValue = index < ui->keyDownDurations.size() ? ui->keyDownDurations[index] : 0.f;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    float pointerDownDuration(const Context * ui, PointerButton button) noexcept
    {
        if(ui == nullptr)
        {
            return 0.f;
        }

        size_t index = static_cast<size_t>(button);
        auto returnedValue = index < ui->pointerDownDurations.size() ? ui->pointerDownDurations[index] : 0.f;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool pointerDragging(const Context * ui, PointerButton button, float threshold) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        const PointerState * pointer = nullptr;

        if(ui->capturedPointer != 0)
        {
            auto captured = std::find_if(ui->input.pointers.begin(), ui->input.pointers.end(),
                                               [ui](const PointerState & candidate)
                                               {
                                                   return candidate.id == ui->capturedPointer;
                                               });

            if(captured != ui->input.pointers.end())
            {
                pointer = &*captured;
            }
        }

        if(pointer == nullptr)
        {
            pointer = ui->input.primaryPointer();
        }

        if(pointer == nullptr)
        {
            return false;
        }

        if(pointer->isDown(button) == false)
        {
            return false;
        }

        Vec2 origin = pointer->pressPosition(button);
        float minimumDistance = threshold >= 0.f ? threshold : std::max(0.f, ui->currentStyle->behavior.dragThreshold);
        float deltaX = pointer->position.x - origin.x;
        float deltaY = pointer->position.y - origin.y;

        return deltaX * deltaX + deltaY * deltaY >= minimumDistance * minimumDistance;
    }

    //////////////////////////////////////////////////////////////////////////
    bool fontCacheMetrics(const Context * ui, FontCacheMetrics * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->fontProvider == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        *_out = ui->fontProvider->cacheMetrics();

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool fontAtlasPage(const Context * ui, size_t index, FontAtlasPage * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->fontProvider == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        bool returnedValue = ui->fontProvider->atlasPage(index, _out);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool fontAtlasConfiguration(const Context * ui, FontAtlasConfiguration * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->fontProvider == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        bool returnedValue = ui->fontProvider->atlasConfiguration(_out);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool availableFonts(const Context * ui, FontInfoVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->fontProvider == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        FontInfoVector entries;
        if(ui->fontProvider->inspectFonts(&entries) == false)
        {
            return false;
        }

        *_out = std::move(entries);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool fontCacheEntries(const Context * ui, FontCacheEntryVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->fontProvider == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        FontCacheEntryVector entries;
        if(ui->fontProvider->inspectFontCache(&entries) == false)
        {
            return false;
        }

        *_out = std::move(entries);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool glyphCacheEntries(const Context * ui, GlyphCacheEntryVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->fontProvider == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        GlyphCacheEntryVector entries;
        if(ui->fontProvider->inspectGlyphCache(&entries) == false)
        {
            return false;
        }

        *_out = std::move(entries);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool fontAtlasRects(const Context * ui, FontAtlasRectInfoVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->fontProvider == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        FontAtlasRectInfoVector entries;
        if(ui->fontProvider->inspectAtlasRects(&entries) == false)
        {
            return false;
        }

        *_out = std::move(entries);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool fontCacheAction(Context * ui, FontCacheAction action)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->fontProvider == nullptr)
        {
            return false;
        }

        bool returnedValue = ui->fontProvider->cacheAction(action);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DockModel & docking(Context * ui) noexcept
    {
        return ui->docking;
    }
    //////////////////////////////////////////////////////////////////////////
    const DockModel & docking(const Context * ui) noexcept
    {
        return ui->docking;
    }
    //////////////////////////////////////////////////////////////////////////
    DockModel & docking(Context * ui, uint32_t group) noexcept
    {
        DockModel * model = Detail::dockModel(ui, std::max(1U, group), true);

        return *model;
    }
    //////////////////////////////////////////////////////////////////////////
    const DockModel & docking(const Context * ui, uint32_t group) noexcept
    {
        uint32_t resolvedGroup = std::max(1U, group);
        DockModel * model = Detail::dockModel(const_cast<Context *>(ui), resolvedGroup, true);

        return *model;
    }
    //////////////////////////////////////////////////////////////////////////
    void setDockArea(Context * ui, const Rect & bounds) noexcept
    {
        Detail::setDockArea(ui, 1, bounds);
    }
    //////////////////////////////////////////////////////////////////////////
    void setDockArea(Context * ui, uint32_t group, const Rect & bounds) noexcept
    {
        Detail::setDockArea(ui, group, bounds);
    }
    //////////////////////////////////////////////////////////////////////////
    bool dockArea(const Context * ui, Rect * const _out) noexcept
    {
        bool returnedValue = Detail::dockArea(ui, 1, _out);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool dockArea(const Context * ui, uint32_t group, Rect * const _out) noexcept
    {
        bool returnedValue = Detail::dockArea(ui, group, _out);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    DragDrop & dragDrop(Context * ui) noexcept
    {
        return ui->dragDrop;
    }
    //////////////////////////////////////////////////////////////////////////
    const DragDrop & dragDrop(const Context * ui) noexcept
    {
        return ui->dragDrop;
    }
    //////////////////////////////////////////////////////////////////////////
    bool serialize(const Context * ui, Serializer & serializer)
    {
        bool successful = true;
        auto writeNumber = [&serializer, &successful](StringView section, Id owner, StringView key, auto value)
        {
            String output;
            if(Detail::toString(value, &output) == false)
            {
                successful = false;

                return;
            }

            serializer.write(section, owner, key, output);
        };

        serializer.begin(2);
        for(const auto & [id, value] : ui->persistent)
        {
            if(value.windowData().initialized == true && value.windowData().saveSettings == true)
            {
                writeNumber("windows", id, "x", value.windowData().bounds.x);
                writeNumber("windows", id, "y", value.windowData().bounds.y);
                writeNumber("windows", id, "width", value.windowData().bounds.width);
                writeNumber("windows", id, "height", value.windowData().bounds.height);

                if(ui->configuration.settingsSaveLastUsedDate == true)
                {
                    writeNumber("windows", id, "last_used_frame", value.windowData().visibleLastFrame);
                }

                if(ui->configuration.debugIniSettings == true)
                {
                    const Context::Node * node = ui->findFrameNode(id);

                    if(node != nullptr)
                    {
                        serializer.write("debug", id, "label", node->label);
                        serializer.write("debug", id, "file", node->debugData().file);
                        writeNumber("debug", id, "line", node->debugData().line);
                    }
                }
            }

            if(value.scrollData().position.x != 0.f)
            {
                writeNumber("scroll", id, "offset_x", value.scrollData().position.x);
            }

            if(value.scrollData().position.y != 0.f)
            {
                writeNumber("scroll", id, "offset_y", value.scrollData().position.y);
            }
            else if(value.scrollData().value != 0.f)
            {
                writeNumber("scroll", id, "offset", value.scrollData().value);
            }

            if(value.expandedInitialized == true)
            {
                serializer.write("tree", id, "expanded", value.expanded ? "1" : "0");
            }

            if(value.table == nullptr)
            {
                continue;
            }

            if(value.table->options.saveSettings == false)
            {
                continue;
            }

            for(size_t column = 0; column != value.table->columns.size(); ++column)
            {
                const Context::TableColumnState & columnState = value.table->columns[column];

                if(columnState.configured == false && columnState.restored == false)
                {
                    continue;
                }

                String prefix = "column.";
                String columnText;
                if(Detail::toString(column, &columnText) == false)
                {
                    successful = false;
                    continue;
                }

                prefix += columnText;
                prefix += '.';
                writeNumber("table", id, prefix + "sizing", static_cast<unsigned>(columnState.options.sizing));
                writeNumber("table", id, prefix + "width", columnState.options.widthOrWeight);
                serializer.write("table", id, prefix + "visible", columnState.options.visible ? "1" : "0");
                writeNumber("table", id, prefix + "order", columnState.order);
            }
            for(size_t sort = 0; sort != value.table->sortSpecs.size(); ++sort)
            {
                const TableSortSpec & spec = value.table->sortSpecs[sort];
                String prefix = "sort.";
                String sortText;
                if(Detail::toString(sort, &sortText) == false)
                {
                    successful = false;
                    continue;
                }

                prefix += sortText;
                prefix += '.';
                writeNumber("table", id, prefix + "column", spec.column);
                writeNumber("table", id, prefix + "direction", static_cast<unsigned>(spec.direction));
                writeNumber("table", id, prefix + "order", spec.order);
            }
        }
        writeNumber("docking", 0, "root", ui->docking.root());
        for(const DockNode & node : ui->docking.nodes())
        {
            writeNumber("docking", node.id, "type", static_cast<unsigned>(node.type));
            writeNumber("docking", node.id, "parent", node.parent);
            writeNumber("docking", node.id, "orientation", static_cast<unsigned>(node.orientation));
            writeNumber("docking", node.id, "ratio", node.ratio);
            writeNumber("docking", node.id, "child0", node.children[0]);
            writeNumber("docking", node.id, "child1", node.children[1]);
            writeNumber("docking", node.id, "active", node.activeTab);
            String tabs;
            for(size_t index = 0; index != node.tabs.size(); ++index)
            {
                if(index != 0)
                {
                    tabs += ',';
                }

                String tab;
                if(Detail::toString(node.tabs[index], &tab) == false)
                {
                    successful = false;
                    continue;
                }

                tabs += tab;
            }
            serializer.write("docking", node.id, "tabs", tabs);
        }
        serializer.end();

        return successful;
    }
    //////////////////////////////////////////////////////////////////////////
    bool deserialize(Context * ui, const Deserializer & deserializer)
    {
        if(deserializer.version() > 2)
        {
            return false;
        }

        struct Data
        {
            Context * context = nullptr;
            bool valid = true;
            bool hasDocking = false;
            DockNodeId dockRoot = 0;
            DockNodeVector dockNodes;
        } data{ui, true, false, 0, {}};

        deserializer.forEach(
            [](StringView section, Id id, StringView key, StringView value, void * opaque)
            {
                Data & dataValue = *static_cast<Data *>(opaque);
                Context::Persistent & state = dataValue.context->state(id);
                auto parseFloat = [&](float & output)
                {
                    const char * begin = value.data();
                    const char * end = value.data() + value.size();
                    auto result = Detail::fromChars(begin, end, output);

                    if(result.ec != std::errc{} || result.ptr != end || std::isfinite(output) == false)
                    {
                        dataValue.valid = false;
                    }
                };
                auto parseUnsigned = [&](uint64_t & output)
                {
                    const char * begin = value.data();
                    const char * end = value.data() + value.size();
                    auto result = Detail::fromChars(begin, end, output);

                    if(result.ec != std::errc{} || result.ptr != end)
                    {
                        dataValue.valid = false;
                    }
                };

                if(section == "windows")
                {
                    state.windowData().initialized = true;

                    if(key == "x")
                    {
                        parseFloat(state.windowData().bounds.x);
                    }
                    else if(key == "y")
                    {
                        parseFloat(state.windowData().bounds.y);
                    }
                    else if(key == "width")
                    {
                        parseFloat(state.windowData().bounds.width);
                    }
                    else if(key == "height")
                    {
                        parseFloat(state.windowData().bounds.height);
                    }
                }
                else if(section == "scroll" && (key == "offset" || key == "offset_x" || key == "offset_y"))
                {
                    float offset = 0.f;
                    parseFloat(offset);
                    offset = std::max(0.f, offset);

                    if(key == "offset_x")
                    {
                        state.scrollData().position.x = offset;
                    }
                    else if(key == "offset_y")
                    {
                        state.scrollData().position.y = offset;
                    }
                    else
                    {
                        state.scrollData().value = offset;
                        state.scrollData().position.y = offset;
                    }
                }
                else if(section == "tree" && key == "expanded")
                {
                    state.expandedInitialized = true;

                    if(value == "1")
                    {
                        state.expanded = true;
                    }
                    else if(value == "0")
                    {
                        state.expanded = false;
                    }
                    else
                    {
                        dataValue.valid = false;
                    }
                }
                else if(section == "table")
                {
                    Context::TableState & tableState = dataValue.context->tableState(id);
                    size_t firstSeparator = key.find('.');
                    size_t secondSeparator = firstSeparator == StringView::npos ? StringView::npos : key.find('.', firstSeparator + 1);

                    if(firstSeparator == StringView::npos)
                    {
                        dataValue.valid = false;

                        return;
                    }

                    if(secondSeparator == StringView::npos)
                    {
                        dataValue.valid = false;

                        return;
                    }

                    StringView type = key.substr(0, firstSeparator);
                    StringView indexText = key.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1);
                    StringView field = key.substr(secondSeparator + 1);
                    uint64_t index = 0;
                    auto indexResult = Detail::fromChars(indexText.data(), indexText.data() + indexText.size(), index);

                    if(indexResult.ec != std::errc{})
                    {
                        dataValue.valid = false;

                        return;
                    }

                    if(indexResult.ptr != indexText.data() + indexText.size())
                    {
                        dataValue.valid = false;

                        return;
                    }

                    if(index >= 4096)
                    {
                        dataValue.valid = false;

                        return;
                    }

                    uint64_t parsed = 0;

                    if(type == "column")
                    {
                        if(tableState.columns.size() <= index)
                        {
                            tableState.columns.resize(static_cast<size_t>(index + 1));
                        }

                        Context::TableColumnState & column = tableState.columns[static_cast<size_t>(index)];
                        column.restored = true;

                        if(field == "width")
                        {
                            parseFloat(column.options.widthOrWeight);
                            column.options.widthOrWeight = std::max(0.001f, column.options.widthOrWeight);
                        }
                        else if(field == "visible")
                        {
                            if(value == "1")
                            {
                                column.options.visible = true;
                            }
                            else if(value == "0")
                            {
                                column.options.visible = false;
                            }
                            else
                            {
                                dataValue.valid = false;
                            }
                        }
                        else
                        {
                            parseUnsigned(parsed);

                            if(field == "sizing" && parsed <= static_cast<unsigned>(TableSizing::StretchSame))
                            {
                                column.options.sizing = static_cast<TableSizing>(parsed);
                            }
                            else if(field == "order" && parsed < 4096)
                            {
                                column.order = static_cast<uint32_t>(parsed);
                            }
                            else
                            {
                                dataValue.valid = false;
                            }
                        }
                    }
                    else if(type == "sort")
                    {
                        if(tableState.sortSpecs.size() <= index)
                        {
                            tableState.sortSpecs.resize(static_cast<size_t>(index + 1));
                        }

                        TableSortSpec & spec = tableState.sortSpecs[static_cast<size_t>(index)];
                        parseUnsigned(parsed);

                        if(field == "column" && parsed < 4096)
                        {
                            spec.column = static_cast<uint32_t>(parsed);
                        }
                        else if(field == "direction" && parsed <= static_cast<unsigned>(SortDirection::Descending))
                        {
                            spec.direction = static_cast<SortDirection>(parsed);
                        }
                        else if(field == "order" && parsed < 4096)
                        {
                            spec.order = static_cast<uint32_t>(parsed);
                        }
                        else
                        {
                            dataValue.valid = false;
                        }
                    }
                    else
                    {
                        dataValue.valid = false;
                    }
                }
                else if(section == "docking")
                {
                    dataValue.hasDocking = true;

                    if(id == 0 && key == "root")
                    {
                        parseUnsigned(dataValue.dockRoot);

                        return;
                    }

                    auto iterator = std::find_if(dataValue.dockNodes.begin(), dataValue.dockNodes.end(),
                                                 [id](const DockNode & node)
                                                 {
                                                     return node.id == id;
                                                 });

                    if(iterator == dataValue.dockNodes.end())
                    {
                        dataValue.dockNodes.push_back({});
                        iterator = std::prev(dataValue.dockNodes.end());
                        iterator->id = id;
                    }

                    DockNode & dockNode = *iterator;
                    uint64_t parsed = 0;

                    if(key == "ratio")
                    {
                        parseFloat(dockNode.ratio);
                    }
                    else if(key == "tabs")
                    {
                        dockNode.tabs.clear();
                        size_t begin = 0;
                        while(begin < value.size())
                        {
                            size_t separator = value.find(',', begin);
                            size_t end = separator == StringView::npos ? value.size() : separator;
                            StringView item = value.substr(begin, end - begin);
                            uint64_t tab = 0;
                            auto result = Detail::fromChars(item.data(), item.data() + item.size(), tab);

                            if(result.ec != std::errc{} || result.ptr != item.data() + item.size())
                            {
                                dataValue.valid = false;
                            }
                            else
                            {
                                dockNode.tabs.push_back(tab);
                            }

                            begin = end + 1;
                        }
                    }
                    else
                    {
                        parseUnsigned(parsed);

                        if(key == "type" && parsed <= static_cast<unsigned>(DockNodeType::Split))
                        {
                            dockNode.type = static_cast<DockNodeType>(parsed);
                        }
                        else if(key == "parent")
                        {
                            dockNode.parent = parsed;
                        }
                        else if(key == "orientation" && parsed <= static_cast<unsigned>(Orientation::Vertical))
                        {
                            dockNode.orientation = static_cast<Orientation>(parsed);
                        }
                        else if(key == "child0")
                        {
                            dockNode.children[0] = parsed;
                        }
                        else if(key == "child1")
                        {
                            dockNode.children[1] = parsed;
                        }
                        else if(key == "active")
                        {
                            dockNode.activeTab = parsed;
                        }
                        else
                        {
                            dataValue.valid = false;
                        }
                    }
                }
            },
            &data);

        if(data.valid == true && data.hasDocking == true && ui->docking.restore(data.dockNodes, data.dockRoot) == false)
        {
            data.valid = false;
        }

        return data.valid;
    }

    //////////////////////////////////////////////////////////////////////////
    void resetPersistentSection(Context * ui, StringView section)
    {
        for(auto & [id, value] : ui->persistent)
        {
            (void)id;

            if(section == "windows")
            {
                value.windowData().initialized = false;
                value.windowData().contentWidthFitted = false;
                value.windowData().contentHeightFitted = false;
                value.windowData().bounds = {};
            }
            else if(section == "scroll")
            {
                value.scrollData().value = 0.f;
                value.scrollData().extent = 0.f;
                value.scrollData().position = {};
                value.scrollData().target = {};
                value.scrollData().velocity = {};
                value.scrollData().range = {};
                value.scrollData().targetInitialized = false;
                value.scrollData().toEndX = false;
                value.scrollData().toEndY = false;
            }
            else if(section == "tree")
            {
                value.expanded = false;
                value.expandedInitialized = false;
            }
            else if(section == "table")
            {
                value.table.reset();
            }
            else if(section == "editing")
            {
                value.editing = false;
            }
        }

        if(section == "editing")
        {
            ui->textEditorStates.clear();
            for(Detail::NumericState & numericState : ui->numericStates)
            {
                numericState.temporaryInput = false;
                numericState.temporaryDoubleClickBlocked = false;
                numericState.dragThresholdPassed = false;
                numericState.dragAccumulator = 0.L;
            }
        }

        if(section == "tabs")
        {
            ui->tabStates.clear();
        }

        if(section == "docking")
        {
            ui->docking.clear();
            ui->dockModels.clear();
            ui->dockAreas.clear();
            ui->windowDockGroups.clear();
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool debugBounds(const Context * ui, Id id, Rect * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(id == InvalidId)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        if(const Context::Node * node = ui->findFrameNode(id); node != nullptr)
        {
            if(node->content.empty() == false)
            {

                *_out = node->content;

                return true;
            }
        }

        if(const Context::Persistent * state = ui->findState(id); state != nullptr)
        {

            *_out = state->lastBounds;

            return true;
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool debugBounds(const Context * ui, const ItemRef & item, Rect * const _out) noexcept
    {
        if(ui == nullptr || item.valid() == false || _out == nullptr)
        {
            return false;
        }

        const Context::Node * node = ui->findFrameNode(item);

        if(node != nullptr && node->content.empty() == false)
        {
            *_out = node->content;

            return true;
        }

        const Context::InteractionSnapshotItem * snapshot = ui->findInteractionSnapshot(item);

        if(snapshot == nullptr || snapshot->bounds.empty() == true)
        {
            return false;
        }

        *_out = snapshot->bounds;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool windowDebugSnapshots(Context * ui, WindowDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        WindowDebugSnapshotVector output;
        Id focusedWindow = InvalidId;
        const Context::Node * focusedNode = ui->findFrameNode(ui->focused);

        if(focusedNode != nullptr)
        {
            focusedWindow = focusedNode->windowOwner;
        }

        for(Context::Node & node : ui->nodes)
        {
            if(node.kind != Detail::NodeKind::Window)
            {
                continue;
            }

            WindowDebugSnapshot snapshot;
            snapshot.id = node.id;
            snapshot.owner = node.windowOwner;
            snapshot.label = node.label;
            snapshot.bounds = node.bounds;
            snapshot.innerBounds = node.childrenClip;
            snapshot.workBounds = node.content;
            snapshot.content = node.content;
            snapshot.clip = node.clip;
            snapshot.outerRectClipped = Rect::intersection(node.bounds, node.clip);
            snapshot.innerClipRect = node.childrenClip;
            snapshot.contentRegionRect = node.content;
            snapshot.titleBarRect = node.windowData().titleVisible == true ? Rect{node.bounds.x, node.bounds.y, node.bounds.width, node.style->metrics.windowTitleHeight} : Rect{};
            snapshot.submission = node.windowSubmission;
            snapshot.zOrder = node.windowData().zOrder;
            snapshot.beginOrder = static_cast<uint32_t>(node.windowSubmission);
            snapshot.dockGroup = node.windowData().dockGroup;
            snapshot.dockNode = node.windowData().dockNode;
            snapshot.active = node.visible;
            snapshot.writeAccessed = node.firstChild != std::numeric_limits<size_t>::max();
            snapshot.hidden = node.visible == false;
            snapshot.skipped = node.windowData().collapsed;
            snapshot.visible = node.visible;
            snapshot.focused = focusedWindow == node.id;
            snapshot.hovered = ui->pointerWindow == node.id;
            snapshot.collapsed = node.windowData().collapsed;
            snapshot.docked = node.windowData().docked;
            snapshot.popup = node.windowData().popup;
            size_t parentIndex = node.parent;

            while(parentIndex != std::numeric_limits<size_t>::max())
            {
                const Context::Node & parentNode = ui->nodes[parentIndex];

                if(parentNode.kind == Detail::NodeKind::Window)
                {
                    snapshot.parent = parentNode.id;
                    break;
                }

                parentIndex = parentNode.parent;
            }

            for(size_t focusIndex = 0; focusIndex != ui->focusOrder.size(); ++focusIndex)
            {
                const Context::Node * focusNode = ui->findFrameNode(ui->focusOrder[focusIndex]);

                if(focusNode == nullptr)
                {
                    continue;
                }

                if(focusNode->windowOwner != node.id)
                {
                    continue;
                }

                snapshot.focusOrder = static_cast<uint32_t>(focusIndex);
                break;
            }

            for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = ui->nodes[child].nextSibling)
            {
                ++snapshot.childCount;
            }
            const Context::Persistent * persistent = ui->findState(node.id);

            if(persistent != nullptr)
            {
                snapshot.scrollOffset = persistent->scrollData().position;
                snapshot.scrollRange = persistent->scrollData().range;
                snapshot.contentIdeal = {node.content.x, node.content.y, persistent->scrollData().contentSize.x, persistent->scrollData().contentSize.y};
                snapshot.verticalScrollbar = persistent->scrollData().verticalTrack;
                snapshot.horizontalScrollbar = persistent->scrollData().horizontalTrack;
                snapshot.lastFrameActive = persistent->windowData().visibleLastFrame;
                snapshot.scrollbarHorizontal = persistent->scrollData().range.x > 0.f;
                snapshot.scrollbarVertical = persistent->scrollData().range.y > 0.f;
                snapshot.acceptsInput = persistent->windowData().acceptsInput;

                if(snapshot.bounds.empty() == true)
                {
                    snapshot.bounds = persistent->lastBounds;
                }
            }

            output.emplace_back(std::move(snapshot));
        }

        std::sort(output.begin(), output.end(),
                  [](const WindowDebugSnapshot & left, const WindowDebugSnapshot & right)
                  {
                      return left.zOrder < right.zOrder;
                  });
        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool drawListDebugSnapshots(const Context * ui, DrawListDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        DrawListDebugSnapshotVector output;
        output.reserve(ui->frame.viewports.size());

        for(const FrameViewport & viewport : ui->frame.viewports)
        {
            const DrawCommandVector * commands = Detail::FrameViewportAccess::commands(viewport);
            const DrawCommandStorage * storage = Detail::FrameViewportAccess::storage(viewport);

            if(commands == nullptr || storage == nullptr)
            {
                continue;
            }

            DrawListDebugSnapshot snapshot;
            snapshot.viewport = viewport.id;
            snapshot.owner = "Viewport";
            snapshot.bounds = viewport.bounds;
            snapshot.commandCount = commands->size();
            snapshot.stateCount = ui->frame.renderStates.size();

            for(const DrawCommand & command : *commands)
            {
                if(command.type == DrawCommandType::CustomGeometry)
                {
                    snapshot.vertexCount += command.payload.custom.vertices.count;
                    snapshot.indexCount += command.payload.custom.indices.count;
                    continue;
                }

                if(command.type == DrawCommandType::TextGeometry)
                {
                    snapshot.vertexCount += command.payload.textGeometry.rectangles.size() * 4;
                    snapshot.indexCount += command.payload.textGeometry.rectangles.size() * 6;
                }
            }

            snapshot.triangleCount = snapshot.indexCount / 3;
            output.emplace_back(snapshot);
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool drawCommandDebugSnapshots(const Context * ui, DrawCommandDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        DrawCommandDebugSnapshotVector output;
        size_t vertexOffset = 0;
        size_t indexOffset = 0;
        for(const FrameViewport & viewport : ui->frame.viewports)
        {
            const DrawCommandVector * commands = Detail::FrameViewportAccess::commands(viewport);
            const DrawCommandStorage * storage = Detail::FrameViewportAccess::storage(viewport);

            if(commands == nullptr || storage == nullptr)
            {
                continue;
            }

            output.reserve(output.size() + commands->size());
            for(size_t index = 0; index != commands->size(); ++index)
            {
                const DrawCommand & command = (*commands)[index];
                DrawCommandDebugSnapshot snapshot;
                snapshot.viewport = viewport.id;
                snapshot.index = index;
                snapshot.type = Detail::drawCommandName(command.type);
                snapshot.renderKey = command.renderKey;
                snapshot.vertexOffset = vertexOffset;
                snapshot.indexOffset = indexOffset;
                const RenderState * renderState = ui->frame.renderState(command.renderKey);

                if(renderState != nullptr)
                {
                    snapshot.texture = renderState->texture;
                    snapshot.clip = renderState->clip;
                }

                switch(command.type)
                {
                case DrawCommandType::Rect:
                case DrawCommandType::RoundedRect:
                case DrawCommandType::Image:
                    snapshot.elementCount = 1;
                    snapshot.bounds = command.payload.rectangle.bounds;
                    break;
                case DrawCommandType::NineSlice:
                    snapshot.elementCount = 9;
                    snapshot.bounds = command.payload.nineSlice.bounds;
                    break;
                case DrawCommandType::Grid:
                    snapshot.elementCount = 1;
                    snapshot.bounds = command.payload.grid.bounds;
                    break;
                case DrawCommandType::Gradient:
                    snapshot.elementCount = 1;
                    snapshot.bounds = command.payload.gradient.bounds;
                    break;
                case DrawCommandType::Line:
                {
                    snapshot.elementCount = 1;
                    const LineDrawCommand & line = command.payload.line;
                    float minimumX = std::min(line.first.x, line.second.x);
                    float minimumY = std::min(line.first.y, line.second.y);
                    float maximumX = std::max(line.first.x, line.second.x);
                    float maximumY = std::max(line.first.y, line.second.y);
                    snapshot.bounds = {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
                    break;
                }
                case DrawCommandType::RectBatch:
                    snapshot.elementCount = command.payload.rectBatch.instances.count;

                    for(const RectInstance & instance : storage->rectangleSpan(command.payload.rectBatch.instances))
                    {
                        snapshot.bounds = Detail::unionBounds(snapshot.bounds, instance.bounds);
                    }

                    break;
                case DrawCommandType::QuadBatch:
                    snapshot.elementCount = command.payload.quadBatch.instances.count;

                    for(const QuadInstance & instance : storage->quadSpan(command.payload.quadBatch.instances))
                    {
                        for(const Vertex & vertex : instance.vertices)
                        {
                            Rect point = {vertex.position.x, vertex.position.y, 0.001f, 0.001f};
                            snapshot.bounds = Detail::unionBounds(snapshot.bounds, point);
                        }
                    }

                    break;
                case DrawCommandType::Polyline:
                    snapshot.elementCount = command.payload.polyline.points.count;

                    for(const Vec2 & pointValue : storage->pointSpan(command.payload.polyline.points))
                    {
                        Rect point = {pointValue.x, pointValue.y, 0.001f, 0.001f};
                        snapshot.bounds = Detail::unionBounds(snapshot.bounds, point);
                    }

                    break;
                case DrawCommandType::Path:
                    snapshot.elementCount = command.payload.path.points.count;

                    for(const Vec2 & pointValue : storage->pointSpan(command.payload.path.points))
                    {
                        Rect point = {pointValue.x, pointValue.y, 0.001f, 0.001f};
                        snapshot.bounds = Detail::unionBounds(snapshot.bounds, point);
                    }

                    break;
                case DrawCommandType::CustomGeometry:
                    snapshot.elementCount = command.payload.custom.indices.count;
                    snapshot.vertexCount = command.payload.custom.vertices.count;
                    snapshot.indexCount = command.payload.custom.indices.count;

                    for(const Vertex & vertex : storage->vertexSpan(command.payload.custom.vertices))
                    {
                        Rect point = {vertex.position.x, vertex.position.y, 0.001f, 0.001f};
                        snapshot.bounds = Detail::unionBounds(snapshot.bounds, point);
                    }

                    break;
                case DrawCommandType::TextGeometry:
                {
                    const TextGeometryDrawCommand & geometry = command.payload.textGeometry;
                    snapshot.elementCount = geometry.rectangles.size();
                    snapshot.vertexCount = geometry.rectangles.size() * 4;
                    snapshot.indexCount = geometry.rectangles.size() * 6;

                    for(const TexturedRectInstance & rectangle : geometry.rectangles)
                    {
                        Array<Vec2, 4> corners = {{{rectangle.bounds.x, rectangle.bounds.y}, {rectangle.bounds.right(), rectangle.bounds.y}, {rectangle.bounds.right(), rectangle.bounds.bottom()}, {rectangle.bounds.x, rectangle.bounds.bottom()}}};

                        for(const Vec2 & corner : corners)
                        {
                            Vec2 pointValue;
                            pointValue.x = geometry.translation.x + geometry.axisX.x * corner.x + geometry.axisY.x * corner.y;
                            pointValue.y = geometry.translation.y + geometry.axisX.y * corner.x + geometry.axisY.y * corner.y;
                            Rect point = {pointValue.x, pointValue.y, 0.001f, 0.001f};
                            snapshot.bounds = Detail::unionBounds(snapshot.bounds, point);
                        }
                    }

                    break;
                }
                case DrawCommandType::Box:
                    snapshot.elementCount = 1;
                    snapshot.bounds = command.payload.box.bounds;
                    break;
                case DrawCommandType::PushClip:
                case DrawCommandType::PopClip:
                case DrawCommandType::PushTransform:
                case DrawCommandType::PopTransform:
                    snapshot.elementCount = 1;
                    break;
                }

                snapshot.triangleCount = snapshot.indexCount / 3;
                vertexOffset += snapshot.vertexCount;
                indexOffset += snapshot.indexCount;

                output.emplace_back(snapshot);
            }
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool popupDebugSnapshots(const Context * ui, PopupDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        PopupDebugSnapshotVector output;
        output.reserve(ui->popupStack.size());

        for(const Context::PopupState & popup : ui->popupStack)
        {
            PopupDebugSnapshot snapshot;
            snapshot.id = popup.id;
            snapshot.owner = popup.owner;

            if(popup.level != 0)
            {
                for(auto parent = output.rbegin(); parent != output.rend(); ++parent)
                {
                    if(parent->level < popup.level && parent->open == true)
                    {
                        snapshot.parent = parent->id;
                        break;
                    }
                }
            }

            snapshot.restoreFocus = popup.restoreFocus;
            snapshot.window = popup.node;
            snapshot.bounds = popup.bounds;
            snapshot.level = popup.level;
            snapshot.open = popup.open;
            snapshot.modal = popup.options.modal;
            snapshot.focused = ui->focused == popup.node || ui->navigationFocused == popup.node;
            snapshot.closeOnClickOutside = popup.options.closeOnClickOutside;
            snapshot.closeOnSelection = popup.options.closeOnSelection;
            output.emplace_back(snapshot);
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool tabBarDebugSnapshots(const Context * ui, TabBarDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        TabBarDebugSnapshotVector output;
        output.reserve(ui->tabStates.size());

        for(const auto & [id, state] : ui->tabStates)
        {
            TabBarDebugSnapshot snapshot;
            snapshot.id = id;
            snapshot.selected = state.selectedItem;
            snapshot.scrollArea = state.scrollArea;
            snapshot.itemCount = state.itemCount;
            snapshot.visibleCount = state.visibleCount;
            snapshot.draggingIndex = state.dragging;
            snapshot.reorderable = state.options.reorderable;
            snapshot.autoSelectNewTabs = state.options.autoSelectNewTabs;
            snapshot.fittingScroll = state.options.fittingPolicy == TabFittingPolicy::Scroll;
            snapshot.noCloseWithMiddleButton = state.options.closeWithMiddleMouse == false;
            snapshot.order = state.orderIds;
            snapshot.items.reserve(state.orderIds.size());
            for(size_t index = 0; index != state.orderIds.size(); ++index)
            {
                Id itemId = state.orderIds[index];
                TabItemDebugSnapshot item;
                item.id = itemId;
                item.order = static_cast<uint32_t>(index);
                item.selected = itemId == state.selectedItem;
                const Context::Node * node = ui->findFrameNode(itemId);

                if(node != nullptr)
                {
                    item.bounds = node->bounds;
                    item.width = node->bounds.width;
                    item.visible = node->visible;

                    if(index != 0)
                    {
                        const Context::Node * first = ui->findFrameNode(state.orderIds.front());

                        if(first != nullptr)
                        {
                            item.offset = node->bounds.x - first->bounds.x;
                        }
                    }
                }

                snapshot.items.emplace_back(item);
            }

            const Context::Node * host = state.hostNode < ui->nodes.size() ? &ui->nodes[state.hostNode] : nullptr;

            if(host != nullptr)
            {
                snapshot.bounds = host->bounds;
            }

            const Context::Persistent * scrollState = ui->findState(state.scrollArea);

            if(scrollState != nullptr)
            {
                snapshot.scrollOffset = scrollState->scrollData().position;
                snapshot.scrollRange = scrollState->scrollData().range;
            }
            output.emplace_back(snapshot);
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool selectionDebugSnapshots(const Context * ui, SelectionDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        SelectionDebugSnapshotVector output;
        output.reserve(ui->selectionItems.size());

        for(const Context::SelectionItem & item : ui->selectionItems)
        {
            SelectionDebugSnapshot snapshot;
            snapshot.item = item.item;
            snapshot.node = item.node;
            snapshot.scope = item.scope;
            const Context::Persistent * itemState = ui->findState(item.node);

            if(itemState != nullptr)
            {
                snapshot.bounds = itemState->lastBounds;
            }

            snapshot.selected = item.model != nullptr && item.model->selected(item.item);
            snapshot.boxSelecting = ui->activeBoxSelection == item.scope;

            if(snapshot.boxSelecting == true)
            {
                snapshot.boxBounds = ui->boxSelectionBounds;
            }

            output.emplace_back(snapshot);
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool dockDebugSnapshots(const Context * ui, DockDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        DockDebugSnapshotVector output;
        output.reserve(ui->dockModels.size());
        for(const auto & [group, model] : ui->dockModels)
        {
            DockDebugSnapshot snapshot;
            snapshot.group = group;
            snapshot.root = model.root();
            snapshot.central = model.centralNode();
            DockNodeSpan nodes = model.nodes();
            snapshot.nodes.assign(nodes.begin(), nodes.end());
            auto area = ui->dockAreas.find(group);

            if(area != ui->dockAreas.end())
            {
                snapshot.area = area->second;
            }

            for(const auto & [window, windowGroup] : ui->windowDockGroups)
            {
                (void)window;

                if(windowGroup == group)
                {
                    ++snapshot.windowCount;
                }
            }

            output.emplace_back(std::move(snapshot));
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool identityDebugEntries(Context * ui, IdentityDebugEntryVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        IdentityDebugEntryVector output;
        output.reserve(ui->nodes.size());

        for(size_t index = 0; index != ui->nodes.size(); ++index)
        {
            Context::Node & node = ui->nodes[index];

            if(node.kind == Detail::NodeKind::Root)
            {
                continue;
            }

            IdentityDebugEntry entry;
            entry.id = node.id;
            entry.parent = node.identityParent;
            entry.local = node.identityLocal;
            entry.path = ui->nodePath(index);
            entry.valueKind = node.debugData().identityValueKind;

            if(node.debugData().identityValueKind == IdentityValueKind::String)
            {
                if(node.frameStringIndex < ui->frameNodeStringCount)
                {
                    entry.value = ui->frameNodeStrings[node.frameStringIndex].identityDebugValue;
                }
            }
            else if(node.debugData().identityValueKind == IdentityValueKind::Integral)
            {
                (void)Detail::toString(node.debugData().identityIntegral, &entry.value);
            }
            else if(node.debugData().identityValueKind == IdentityValueKind::Hash)
            {
                (void)Detail::formatIntegral(node.identityLocal, "0x%016x", &entry.value);
            }
            else
            {
                entry.value.assign(node.debugData().file.begin(), node.debugData().file.end());
                entry.value += ':';
                String line;
                (void)Detail::toString(node.debugData().line, &line);
                entry.value += line;
            }

            entry.file = node.debugData().file;
            entry.line = node.debugData().line;
            entry.anonymous = node.anonymousIdentity;
            entry.conflict = node.idConflict;
            output.emplace_back(std::move(entry));
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool groupDebugSnapshots(const Context * ui, GroupDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        GroupDebugSnapshotVector output;
        output.reserve(ui->nodes.size());

        for(const Context::Node & node : ui->nodes)
        {
            bool group = node.kind == Detail::NodeKind::Scope;
            group = group || node.kind == Detail::NodeKind::Row;
            group = group || node.kind == Detail::NodeKind::Column;
            group = group || node.kind == Detail::NodeKind::Grid;
            group = group || node.kind == Detail::NodeKind::Overlay;

            if(group == false)
            {
                continue;
            }

            GroupDebugSnapshot snapshot;
            snapshot.id = node.id;
            snapshot.parent = node.parentId;
            snapshot.type = Detail::nodeKindName(node.kind);
            snapshot.bounds = node.bounds;
            snapshot.clip = node.clip;
            snapshot.contentBounds = node.content;
            snapshot.childrenClip = node.childrenClip;
            snapshot.response = node.response;
            snapshot.visible = node.visible;

            const Context::Persistent * persistent = ui->findState(node.id);

            if(persistent != nullptr)
            {
                snapshot.contentIdealBounds = {node.content.x, node.content.y, persistent->scrollData().contentSize.x, persistent->scrollData().contentSize.y};
            }

            for(size_t child = node.firstChild; child != std::numeric_limits<size_t>::max(); child = ui->nodes[child].nextSibling)
            {
                ++snapshot.childCount;
            }

            output.emplace_back(snapshot);
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemDebugSnapshots(Context * ui, ItemDebugSnapshotVector * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        ItemDebugSnapshotVector output;
        output.reserve(ui->nodes.size());
        for(size_t index = 1; index != ui->nodes.size(); ++index)
        {
            Context::Node & node = ui->nodes[index];
            ItemDebugSnapshot snapshot;
            snapshot.item = node.item;
            snapshot.id = node.id;
            snapshot.parent = node.parentId;
            snapshot.identityScope = node.identityScope;
            snapshot.window = node.windowOwner;
            snapshot.label = node.label;
            snapshot.path = ui->nodePath(index);
            snapshot.type = Detail::nodeKindName(node.kind);
            snapshot.file = node.debugData().file;
            snapshot.line = node.debugData().line;
            snapshot.bounds = node.bounds;
            snapshot.clip = node.clip;
            snapshot.response = node.response;
            snapshot.role = node.semanticRole;
            snapshot.submission = node.windowSubmission;
            snapshot.visible = node.visible;
            snapshot.disabled = node.disabled;
            snapshot.readOnly = node.readOnly;
            snapshot.anonymous = node.anonymousIdentity;
            snapshot.conflict = node.idConflict;
            output.emplace_back(std::move(snapshot));
        }

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemDebugSnapshot(Context * ui, Id id, ItemDebugSnapshot * const _out)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(id == InvalidId)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        size_t index = ui->findFrameNodeIndex(id);

        if(index == std::numeric_limits<size_t>::max())
        {
            return false;
        }

        Context::Node & node = ui->nodes[index];
        ItemDebugSnapshot snapshot;
        snapshot.item = node.item;
        snapshot.id = node.id;
        snapshot.parent = node.parentId;
        snapshot.identityScope = node.identityScope;
        snapshot.window = node.windowOwner;
        snapshot.label = node.label;
        snapshot.path = ui->nodePath(index);
        snapshot.type = Detail::nodeKindName(node.kind);
        snapshot.file = node.debugData().file;
        snapshot.line = node.debugData().line;
        snapshot.bounds = node.bounds;
        snapshot.clip = node.clip;
        snapshot.response = node.response;
        snapshot.role = node.semanticRole;
        snapshot.submission = node.windowSubmission;
        snapshot.visible = node.visible;
        snapshot.disabled = node.disabled;
        snapshot.readOnly = node.readOnly;
        snapshot.anonymous = node.anonymousIdentity;
        snapshot.conflict = node.idConflict;
        *_out = std::move(snapshot);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemDebugSnapshot(Context * ui, const ItemRef & item, ItemDebugSnapshot * const _out)
    {
        if(ui == nullptr || item.valid() == false || _out == nullptr)
        {
            return false;
        }

        size_t index = ui->findFrameNodeIndex(item);

        if(index == std::numeric_limits<size_t>::max())
        {
            return false;
        }

        Context::Node & node = ui->nodes[index];
        ItemDebugSnapshot snapshot;
        snapshot.item = node.item;
        snapshot.id = node.id;
        snapshot.parent = node.parentId;
        snapshot.identityScope = node.identityScope;
        snapshot.window = node.windowOwner;
        snapshot.label = node.label;
        snapshot.path = ui->nodePath(index);
        snapshot.type = Detail::nodeKindName(node.kind);
        snapshot.file = node.debugData().file;
        snapshot.line = node.debugData().line;
        snapshot.bounds = node.bounds;
        snapshot.clip = node.clip;
        snapshot.response = node.response;
        snapshot.role = node.semanticRole;
        snapshot.submission = node.windowSubmission;
        snapshot.visible = node.visible;
        snapshot.disabled = node.disabled;
        snapshot.readOnly = node.readOnly;
        snapshot.anonymous = node.anonymousIdentity;
        snapshot.conflict = node.idConflict;
        *_out = std::move(snapshot);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool contextDebugSnapshot(const Context * ui, ContextDebugSnapshot * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        ContextDebugSnapshot snapshot;
        snapshot.frameNumber = ui->frame.number;
        snapshot.metrics = ui->frame.metrics;
        snapshot.configuration = ui->configuration;
        snapshot.activeItem = ui->active;
        snapshot.pointerFocusedItem = ui->pointerFocused;
        snapshot.keyboardFocusedItem = ui->focused;
        snapshot.navigationFocusedItem = ui->navigationFocused;
        snapshot.capturedItem = ui->captured;
        snapshot.hoveredWindow = ui->pointerWindow;
        snapshot.dockingDragWindow = ui->dockingDragWindow;
        snapshot.wheelOwner = ui->wheelOwner;
        snapshot.dragDropSource = ui->dragDrop.source();
        snapshot.dragDropTarget = ui->dragDrop.target();
        snapshot.dragDropPhase = ui->dragDrop.phase();
        snapshot.persistentEntryCount = ui->persistent.size();
        snapshot.dockModelCount = ui->dockModels.size();
        snapshot.tabBarStateCount = ui->tabStates.size();
        snapshot.textEditorStateCount = ui->textEditorStates.size();
        snapshot.colorEditorStateCount = ui->colorEditorStates.size();
        snapshot.selectionItemCount = ui->selectionItems.size();
        snapshot.frameStyleCount = ui->frameStyleCount;
        snapshot.textCacheEntryCount = ui->textCacheEntryCount;
        snapshot.textCacheMemory = ui->textCacheMemory;

        for(const auto & entry : ui->persistent)
        {
            const Context::Persistent & persistent = entry.second;

            if(persistent.windowData().initialized == true)
            {
                ++snapshot.windowSettingCount;
                SettingDebugSnapshot setting;
                setting.id = entry.first;
                setting.type = "Window";
                setting.bounds = persistent.windowData().bounds;
                setting.lastFrame = persistent.windowData().visibleLastFrame;
                setting.memory = sizeof(Context::Persistent);
                setting.active = persistent.windowData().visible;
                snapshot.settings.emplace_back(setting);
                snapshot.settingsMemory += setting.memory;
            }

            if(persistent.windowData().dragging == true && persistent.windowData().interaction == 3)
            {
                snapshot.movingWindow = entry.first;
            }

            if(persistent.windowData().dragging == true && persistent.windowData().interaction == 6)
            {
                snapshot.resizingWindow = entry.first;
            }

            if(persistent.table != nullptr)
            {
                ++snapshot.tableSettingCount;
                SettingDebugSnapshot setting;
                setting.id = entry.first;
                setting.type = "Table";
                setting.bounds = persistent.lastBounds;
                setting.lastFrame = persistent.lastFrame;
                setting.memory = sizeof(Context::TableState) + persistent.table->columns.capacity() * sizeof(Context::TableColumnState) + persistent.table->sortSpecs.capacity() * sizeof(TableSortSpec);
                setting.active = persistent.lastFrame == ui->frame.number;
                snapshot.settings.emplace_back(setting);
                snapshot.settingsMemory += setting.memory;
            }
        }

        *_out = snapshot;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void setItemPickerEnabled(Context * ui, bool enabled) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->itemPickerEnabled = enabled;

        if(enabled == false)
        {
            ui->itemPickerHovered = InvalidId;
            ui->itemPickerHoveredBounds = {};
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void setItemPickerTarget(Context * ui, Id id) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->itemPickerSelected = id;
        ui->itemPickerSelectedBounds = {};

        if(id == InvalidId)
        {
            return;
        }

        const Context::Node * node = ui->findFrameNode(id);

        if(node != nullptr)
        {
            ui->itemPickerSelectedBounds = node->bounds;
            return;
        }

        const Context::Persistent * persistent = ui->findState(id);

        if(persistent != nullptr)
        {
            ui->itemPickerSelectedBounds = persistent->lastBounds;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemPickerState(const Context * ui, ItemPickerState * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        _out->hovered = ui->itemPickerHovered;
        _out->selected = ui->itemPickerSelected;
        _out->hoveredBounds = ui->itemPickerHoveredBounds;
        _out->selectedBounds = ui->itemPickerSelectedBounds;
        _out->enabled = ui->itemPickerEnabled;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void clearItemPicker(Context * ui) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        ui->itemPickerHovered = InvalidId;
        ui->itemPickerSelected = InvalidId;
        ui->itemPickerHoveredBounds = {};
        ui->itemPickerSelectedBounds = {};
    }
    //////////////////////////////////////////////////////////////////////////
    bool contentRegionAvailable(const Context * ui, Vec2 * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        if(ui->currentParent >= ui->nodes.size())
        {
            return false;
        }

        const Context::Node & parent = ui->nodes[ui->currentParent];
        const Context::Persistent * state = ui->findState(parent.id);
        Rect bounds = parent.content;

        if(bounds.empty() == true && state != nullptr)
        {
            bounds = Detail::inset(state->lastBounds, parent.layout.padding);

            if(parent.kind == Detail::NodeKind::Window && parent.windowData().titleVisible == true)
            {
                bounds.y += parent.style->metrics.windowTitleHeight;
                bounds.height = std::max(0.f, bounds.height - parent.style->metrics.windowTitleHeight);
            }
        }

        if(bounds.empty() == true)
        {
            return false;
        }

        float horizontalScrollbar = state != nullptr && state->scrollData().horizontalTrack.empty() == false ? state->scrollData().horizontalTrack.height + ui->gap(parent) : 0.f;
        float verticalScrollbar = state != nullptr && state->scrollData().verticalTrack.empty() == false ? state->scrollData().verticalTrack.width + ui->gap(parent) : 0.f;
        Vec2 available = {std::max(0.f, bounds.width - verticalScrollbar), std::max(0.f, bounds.height - horizontalScrollbar)};
        *_out = available;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool debugClip(const Context * ui, Id id, Rect * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(id == InvalidId)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        if(const Context::Node * node = ui->findFrameNode(id); node != nullptr)
        {
            if(node->clip.empty() == false)
            {

                *_out = node->clip;

                return true;
            }
        }

        if(const Context::Persistent * state = ui->findState(id); state != nullptr)
        {

            *_out = state->lastClip;

            return true;
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool debugClip(const Context * ui, const ItemRef & item, Rect * const _out) noexcept
    {
        if(ui == nullptr || item.valid() == false || _out == nullptr)
        {
            return false;
        }

        const Context::Node * node = ui->findFrameNode(item);

        if(node != nullptr && node->clip.empty() == false)
        {
            *_out = node->clip;

            return true;
        }

        const Context::InteractionSnapshotItem * snapshot = ui->findInteractionSnapshot(item);

        if(snapshot == nullptr || snapshot->clip.empty() == true)
        {
            return false;
        }

        *_out = snapshot->clip;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemResponse(const Context * ui, Id id, Response * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(id == InvalidId)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::Node * node = ui->findFrameNode(id);

        if(node != nullptr)
        {

            *_out = node->response;

            return true;
        }

        Response result;
        result.id = id;

        *_out = result;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemResponse(const Context * ui, const ItemRef & item, Response * const _out) noexcept
    {
        if(ui == nullptr || item.valid() == false || _out == nullptr)
        {
            return false;
        }

        const Context::Node * node = ui->findFrameNode(item);

        if(node == nullptr)
        {
            Response response;
            response.id = item.id;
            response.item = item;
            *_out = response;

            return true;
        }

        *_out = node->response;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemVisible(const Context * ui, Id id) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(id == InvalidId)
        {
            return false;
        }

        if(const Context::Node * node = ui->findFrameNode(id); node != nullptr)
        {
            bool visible = node->visible;

            if(node->bounds.empty() == true)
            {
                visible = false;
            }

            if(node->clip.empty() == true)
            {
                visible = false;
            }

            if(visible == true)
            {
                auto returnedValue = Rect::intersection(node->bounds, node->clip).empty() == false;

                return returnedValue;
            }

            return false;
        }

        const Context::Persistent * state = ui->findState(id);
        auto returnedValue = state != nullptr && state->lastBounds.empty() == false && state->lastClip.empty() == false && Rect::intersection(state->lastBounds, state->lastClip).empty() == false;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemVisible(const Context * ui, const ItemRef & item) noexcept
    {
        if(ui == nullptr || item.valid() == false)
        {
            return false;
        }

        Rect bounds;

        if(Mosaic::debugBounds(ui, item, &bounds) == false)
        {
            return false;
        }

        Rect clip;

        if(Mosaic::debugClip(ui, item, &clip) == false)
        {
            return false;
        }

        bool returnedValue = Rect::intersection(bounds, clip).empty() == false;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemHovered(const Context * ui, Id id, const ItemQueryOptions & options) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(id == InvalidId)
        {
            return false;
        }

        const Context::Node * node = ui->findFrameNode(id);
        const Context::Persistent * state = ui->findState(id);
        const PointerState * pointer = ui->input.primaryPointer();

        if(state == nullptr)
        {
            return false;
        }

        if(pointer == nullptr)
        {
            return false;
        }

        if(pointer->type == PointerType::Touch)
        {
            return false;
        }

        if(state->lastBounds.empty() == true)
        {
            return false;
        }

        if(state->lastClip.empty() == true)
        {
            return false;
        }

        if(state->lastBounds.contains(pointer->position) == false)
        {
            return false;
        }

        if(state->lastClip.contains(pointer->position) == false)
        {
            return false;
        }

        if(options.rectOnly == true)
        {
            return true;
        }

        if(node != nullptr && node->disabled == true && options.allowWhenDisabled == false)
        {
            return false;
        }

        if(options.allowWhenBlockedByPopup == false)
        {
            Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;

            if(blockingLayer != InvalidId && state->lastInputLayer != blockingLayer && Detail::popupOwnerCanInteract(ui, id) == false)
            {
                return false;
            }
        }

        if(options.allowWhenBlockedByActiveItem == false && ui->captured != InvalidId && ui->captured != id)
        {
            return false;
        }

        Id window = node == nullptr ? state->windowOwner : node->windowOwner;

        if(options.allowWhenOverlappedByWindow == false && ui->pointerWindow != InvalidId && window != ui->pointerWindow)
        {
            return false;
        }

        if(options.allowWhenOverlappedByItem == false)
        {
            for(auto iterator = ui->nodes.rbegin(); iterator != ui->nodes.rend(); ++iterator)
            {
                if(iterator->id == id)
                {
                    break;
                }

                if(iterator->visible == false)
                {
                    continue;
                }

                if(iterator->disabled == true)
                {
                    continue;
                }

                if(iterator->focusable == false)
                {
                    continue;
                }

                if(iterator->windowOwner != window)
                {
                    continue;
                }

                const Context::Persistent * candidate = ui->findState(iterator->id);

                if(candidate == nullptr)
                {
                    continue;
                }

                if(candidate->lastBounds.empty() == true)
                {
                    continue;
                }

                if(candidate->lastClip.empty() == true)
                {
                    continue;
                }

                if(candidate->lastBounds.contains(pointer->position) == false)
                {
                    continue;
                }

                if(candidate->lastClip.contains(pointer->position) == true)
                {
                    return false;
                }
            }
        }

        double duration = options.stationary ? state->stationaryHoverDuration : state->hoverDuration;
        auto returnedValue = duration >= static_cast<double>(std::max(0.f, options.delay));

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool itemHovered(const Context * ui, const ItemRef & item, const ItemQueryOptions & options) noexcept
    {
        if(ui == nullptr || item.valid() == false)
        {
            return false;
        }

        const Context::InteractionSnapshotItem * snapshot = ui->findInteractionSnapshot(item);
        const PointerState * pointer = ui->input.primaryPointer();

        if(snapshot == nullptr || pointer == nullptr)
        {
            return false;
        }

        if(pointer->type == PointerType::Touch)
        {
            return false;
        }

        if(snapshot->bounds.empty() == true || snapshot->clip.empty() == true)
        {
            return false;
        }

        if(snapshot->bounds.contains(pointer->position) == false || snapshot->clip.contains(pointer->position) == false)
        {
            return false;
        }

        if(options.rectOnly == true)
        {
            return true;
        }

        if(snapshot->disabled == true && options.allowWhenDisabled == false)
        {
            return false;
        }

        if(options.allowWhenBlockedByPopup == false)
        {
            Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;

            if(blockingLayer != InvalidId && snapshot->inputLayer != blockingLayer && Detail::popupOwnerCanInteract(ui, item.id) == false)
            {
                return false;
            }
        }

        if(options.allowWhenBlockedByActiveItem == false && ui->capturedItem.valid() == true && ui->capturedItem != item)
        {
            return false;
        }

        if(options.allowWhenOverlappedByWindow == false && ui->pointerWindow != InvalidId)
        {
            if(snapshot->windowOwner != ui->pointerWindow || snapshot->windowSubmission != ui->pointerWindowSubmission)
            {
                return false;
            }
        }

        if(options.allowWhenOverlappedByItem == false)
        {
            for(auto iterator = ui->interactionSnapshot.rbegin(); iterator != ui->interactionSnapshot.rend(); ++iterator)
            {
                if(iterator->item == item)
                {
                    break;
                }

                if(iterator->visible == false || iterator->disabled == true || iterator->focusable == false)
                {
                    continue;
                }

                if(iterator->windowOwner != snapshot->windowOwner || iterator->windowSubmission != snapshot->windowSubmission)
                {
                    continue;
                }

                if(iterator->bounds.contains(pointer->position) == true && iterator->clip.contains(pointer->position) == true)
                {
                    return false;
                }
            }
        }

        const Context::Persistent * state = ui->findState(item.id);

        if(state == nullptr)
        {
            return options.delay <= 0.f;
        }

        double duration = options.stationary == true ? state->stationaryHoverDuration : state->hoverDuration;
        bool returnedValue = duration >= static_cast<double>(std::max(0.f, options.delay));

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool scopeFocused(const Context * ui, Id id, bool includeDescendants) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(id == InvalidId)
        {
            return false;
        }

        if(ui->focused == id)
        {
            return true;
        }

        if(ui->navigationFocused == id)
        {
            return true;
        }

        if(includeDescendants == false)
        {
            return false;
        }

        auto descendsFrom = [ui, id](Id focused)
        {
            const Context::Node * node = ui->findFrameNode(focused);
            while(node != nullptr && node->parent < ui->nodes.size())
            {
                const Context::Node & parent = ui->nodes[node->parent];

                if(parent.id == id)
                {
                    return true;
                }

                if(parent.parent == node->parent)
                {
                    return false;
                }

                node = &parent;
            }

            return false;
        };
        auto returnedValue = descendsFrom(ui->focused) || descendsFrom(ui->navigationFocused);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool scopeFocused(const Context * ui, Id id, const ScopeQueryOptions & options) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(options.anyScope == true)
        {
            return ui->focused != InvalidId || ui->navigationFocused != InvalidId;
        }

        if(id == InvalidId)
        {
            return false;
        }

        Id query = id;

        if(options.rootScope == true)
        {
            const Context::Node * node = ui->findFrameNode(query);

            if(node != nullptr && node->windowOwner != InvalidId)
            {
                query = node->windowOwner;
            }
        }

        if(Mosaic::scopeFocused(ui, query, options.includeDescendants) == true)
        {
            return true;
        }

        if(options.includeDockHierarchy == true && (Detail::dockHierarchyMatches(ui, query, ui->focused) == true || Detail::dockHierarchyMatches(ui, query, ui->navigationFocused) == true))
        {
            return true;
        }

        if(options.includePopupHierarchy == true)
        {
            for(const Context::PopupState & popup : ui->popupStack)
            {
                if(popup.open == false)
                {
                    continue;
                }

                if((Detail::nodeDescendsFrom(ui, ui->focused, popup.node) == false && Detail::nodeDescendsFrom(ui, ui->navigationFocused, popup.node) == false))
                {
                    continue;
                }

                if(popup.owner == query)
                {
                    return true;
                }

                if((options.includeDescendants == true && Detail::nodeDescendsFrom(ui, popup.owner, query) == true))
                {
                    return true;
                }
            }
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool scopeHovered(const Context * ui, Id id) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(id == InvalidId)
        {
            return false;
        }

        const PointerState * pointer = ui->input.primaryPointer();

        if(pointer == nullptr)
        {
            return false;
        }

        if(pointer->type == PointerType::Touch)
        {
            return false;
        }

        Rect bounds;
        if(Mosaic::debugBounds(ui, id, &bounds) == false)
        {
            return false;
        }

        Rect clip;
        if(Mosaic::debugClip(ui, id, &clip) == false)
        {
            return false;
        }

        auto returnedValue = bounds.empty() == false && clip.empty() == false && bounds.contains(pointer->position) && clip.contains(pointer->position) && (ui->blockingInputLayer == InvalidId || ui->blockingInputLayer == id || ui->previousBlockingInputLayer == id);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool scopeHovered(const Context * ui, Id id, const ScopeQueryOptions & options) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(options.anyScope == true)
        {
            return ui->pointerWindow != InvalidId;
        }

        if(id == InvalidId)
        {
            return false;
        }

        Id query = id;
        const Context::Node * node = ui->findFrameNode(query);

        if(options.rootScope == true && node != nullptr && node->windowOwner != InvalidId)
        {
            query = node->windowOwner;
            node = ui->findFrameNode(query);
        }

        const PointerState * pointer = ui->input.primaryPointer();
        const Context::Persistent * state = ui->findState(query);

        if(pointer == nullptr)
        {
            return false;
        }

        if(pointer->type == PointerType::Touch)
        {
            return false;
        }

        if(state == nullptr)
        {
            return false;
        }

        if(state->lastBounds.empty() == true)
        {
            return false;
        }

        if(state->lastClip.empty() == true)
        {
            return false;
        }

        if(state->lastBounds.contains(pointer->position) == false)
        {
            return false;
        }

        if(state->lastClip.contains(pointer->position) == false)
        {
            return false;
        }

        if(options.allowWhenBlockedByPopup == false)
        {
            Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;

            if(blockingLayer != InvalidId && state->lastInputLayer != blockingLayer && Detail::popupOwnerCanInteract(ui, query) == false)
            {
                return false;
            }
        }

        if(options.allowWhenBlockedByActiveItem == false && ui->captured != InvalidId && ui->captured != query)
        {
            const Context::Node * captured = ui->findFrameNode(ui->captured);

            if(captured == nullptr)
            {
                return false;
            }

            if(captured->windowOwner != query)
            {
                return false;
            }
        }

        Id window = node == nullptr ? state->windowOwner : node->windowOwner;

        if(options.allowWhenOverlappedByWindow == false && ui->pointerWindow != InvalidId && window != ui->pointerWindow && query != ui->pointerWindow)
        {
            if(options.includeDockHierarchy == false || Detail::dockHierarchyMatches(ui, query, ui->pointerWindow) == false)
            {
                bool popupHierarchy = false;

                if(options.includePopupHierarchy == true)
                {
                    for(const Context::PopupState & popup : ui->popupStack)
                    {
                        if(popup.open == false)
                        {
                            continue;
                        }

                        if(popup.node != ui->pointerWindow)
                        {
                            continue;
                        }

                        bool ownerMatches = popup.owner == query;

                        if(options.includeDescendants == true)
                        {
                            if(Detail::nodeDescendsFrom(ui, popup.owner, query) == true)
                            {
                                ownerMatches = true;
                            }
                        }

                        if(ownerMatches == true)
                        {
                            popupHierarchy = true;
                            break;
                        }
                    }
                }

                if(popupHierarchy == false)
                {
                    return false;
                }
            }
        }

        double duration = options.stationary ? state->stationaryHoverDuration : state->hoverDuration;
        auto returnedValue = duration >= static_cast<double>(std::max(0.f, options.delay));

        return returnedValue;
    }

    double itemHoverDuration(const Context * ui, Id id) noexcept
    {
        if(ui == nullptr)
        {
            return 0.0;
        }

        if(id == InvalidId)
        {
            return 0.0;
        }

        const Context::Persistent * state = ui->findState(id);

        return state == nullptr ? 0.0 : state->hoverDuration;
    }

    double itemStationaryHoverDuration(const Context * ui, Id id) noexcept
    {
        if(ui == nullptr)
        {
            return 0.0;
        }

        if(id == InvalidId)
        {
            return 0.0;
        }

        const Context::Persistent * state = ui->findState(id);

        return state == nullptr ? 0.0 : state->stationaryHoverDuration;
    }
    //////////////////////////////////////////////////////////////////////////
    bool pointerPosition(const Context * ui, Vec2 * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const PointerState * pointer = ui->input.primaryPointer();

        if(pointer == nullptr)
        {
            return false;
        }

        *_out = pointer->position;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool consumeWheel(Context * ui, Id owner, Vec2 * const _out) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(owner == InvalidId)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        if((ui->input.wheel.x == 0.f && ui->input.wheel.y == 0.f))
        {
            return false;
        }

        const PointerState * pointer = ui->input.primaryPointer();

        if(pointer == nullptr)
        {
            return false;
        }

        Rect bounds;
        if(Mosaic::debugBounds(ui, owner, &bounds) == false)
        {
            return false;
        }

        Rect clip;
        if(Mosaic::debugClip(ui, owner, &clip) == false)
        {
            return false;
        }

        if(bounds.empty() == true)
        {
            return false;
        }

        if(clip.empty() == true)
        {
            return false;
        }

        if(bounds.contains(pointer->position) == false)
        {
            return false;
        }

        if(clip.contains(pointer->position) == false)
        {
            return false;
        }

        *_out = ui->input.wheel;
        ui->input.wheel = {};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool isFocused(const Context * ui, Id id) noexcept
    {
        return ui->focused == id;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasRect(Context * ui, Id canvas, const Rect & bounds, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas rectangle target is not a canvas");

            return false;
        }

        if(bounds.empty() == true)
        {
            return true;
        }

        if(color.a <= 0.f)
        {
            return true;
        }

        DrawCommandVector & commands = ui->canvasCommands(*node);
        bool appendCommand = commands.empty() == true;

        if(appendCommand == false)
        {
            const DrawCommand & lastCommand = commands.back();
            appendCommand = lastCommand.type != DrawCommandType::RectBatch;
        }

        if(appendCommand == true)
        {
            DrawCommand command(DrawCommandType::RectBatch);
            command.payload.rectBatch.instances.offset = ui->drawCommandStorage.rectangles.size();
            commands.emplace_back(std::move(command));
        }

        DrawCommand & command = commands.back();
        DrawDataRange & instances = command.payload.rectBatch.instances;
        RectInstance instance = {bounds, color};
        ui->drawCommandStorage.rectangles.push_back(instance);
        ++instances.count;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasPushTransform(Context * ui, Id canvas, const Transform2D & transform, StrokeScale strokeScale)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas transform target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::PushTransform);
        command.payload.transform.transform = transform;
        command.payload.transform.strokeScale = strokeScale;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        Context::CanvasInteractionState & interaction = ui->canvasInteractionStates[canvas];
        interaction.stack.push_back(interaction.transform);
        Transform2D parent = interaction.transform;
        Transform2D combined;
        combined.translation.x = parent.translation.x + parent.axisX.x * transform.translation.x + parent.axisY.x * transform.translation.y;
        combined.translation.y = parent.translation.y + parent.axisX.y * transform.translation.x + parent.axisY.y * transform.translation.y;
        combined.axisX.x = parent.axisX.x * transform.axisX.x + parent.axisY.x * transform.axisX.y;
        combined.axisX.y = parent.axisX.y * transform.axisX.x + parent.axisY.y * transform.axisX.y;
        combined.axisY.x = parent.axisX.x * transform.axisY.x + parent.axisY.x * transform.axisY.y;
        combined.axisY.y = parent.axisX.y * transform.axisY.x + parent.axisY.y * transform.axisY.y;
        interaction.transform = combined;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasPopTransform(Context * ui, Id canvas)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas transform target is not a canvas");

            return false;
        }

        auto iterator = ui->canvasInteractionStates.find(canvas);

        if(iterator == ui->canvasInteractionStates.end())
        {
            ui->frame.diagnostics.emplace_back("Canvas interaction transform stack underflow");

            return false;
        }

        Context::CanvasInteractionState & interaction = iterator->second;

        if(interaction.stack.empty() == true)
        {
            ui->frame.diagnostics.emplace_back("Canvas interaction transform stack underflow");

            return false;
        }

        DrawCommand command(DrawCommandType::PopTransform);
        ui->canvasCommands(*node).emplace_back(std::move(command));
        interaction.transform = interaction.stack.back();
        interaction.stack.pop_back();

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasRects(Context * ui, Id canvas, RectInstanceSpan instances)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(instances.empty() == true)
        {
            return true;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas rectangle batch target is not a canvas");

            return false;
        }

        DrawCommandVector & commands = ui->canvasCommands(*node);
        bool appendCommand = commands.empty() == true;

        if(appendCommand == false)
        {
            const DrawCommand & lastCommand = commands.back();
            appendCommand = lastCommand.type != DrawCommandType::RectBatch;
        }

        if(appendCommand == true)
        {
            DrawCommand command(DrawCommandType::RectBatch);
            command.payload.rectBatch.instances.offset = ui->drawCommandStorage.rectangles.size();
            commands.emplace_back(std::move(command));
        }

        DrawCommand & command = commands.back();
        DrawDataRange & destination = command.payload.rectBatch.instances;
        ui->drawCommandStorage.rectangles.reserve(ui->drawCommandStorage.rectangles.size() + instances.size());
        for(const RectInstance & instance : instances)
        {
            if(instance.bounds.empty() == true)
            {
                continue;
            }

            if(instance.color.a <= 0.f)
            {
                continue;
            }

            ui->drawCommandStorage.rectangles.push_back(instance);
            ++destination.count;
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasQuads(Context * ui, Id canvas, QuadInstanceSpan instances)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(instances.empty() == true)
        {
            return true;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas quad batch target is not a canvas");

            return false;
        }

        DrawCommandVector & commands = ui->canvasCommands(*node);
        bool appendCommand = commands.empty() == true;

        if(appendCommand == false)
        {
            const DrawCommand & lastCommand = commands.back();
            appendCommand = lastCommand.type != DrawCommandType::QuadBatch;
        }

        if(appendCommand == true)
        {
            DrawCommand command(DrawCommandType::QuadBatch);
            command.payload.quadBatch.instances.offset = ui->drawCommandStorage.quads.size();
            commands.emplace_back(std::move(command));
        }

        DrawCommand & command = commands.back();
        DrawDataRange & destination = command.payload.quadBatch.instances;
        ui->drawCommandStorage.quads.insert(ui->drawCommandStorage.quads.end(), instances.begin(), instances.end());
        destination.count += instances.size();

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasBox(Context * ui, Id canvas, const Rect & bounds, const BoxStyle & style)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas box target is not a canvas");

            return false;
        }

        if(bounds.empty() == true)
        {
            return true;
        }

        DrawCommand command(DrawCommandType::Box);
        command.payload.box.bounds = bounds;
        command.payload.box.style = style;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasRoundedRect(Context * ui, Id canvas, const Rect & bounds, float radius, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas rounded rectangle target is not a canvas");

            return false;
        }

        if(bounds.empty() == true)
        {
            return true;
        }

        if(color.a <= 0.f)
        {
            return true;
        }

        DrawCommand command(DrawCommandType::RoundedRect);
        command.payload.rectangle.bounds = bounds;
        command.payload.rectangle.radius = radius;
        command.payload.rectangle.color = color;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasGradient(Context * ui, Id canvas, const Rect & bounds, const Color & topLeft, const Color & topRight, const Color & bottomRight, const Color & bottomLeft)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas gradient target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::Gradient);
        command.payload.gradient.bounds = bounds;
        command.payload.gradient.colors = {topLeft, topRight, bottomRight, bottomLeft};
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasLine(Context * ui, Id canvas, const Vec2 & first, const Vec2 & second, float thickness, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas line target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::Line);
        command.payload.line.first = first;
        command.payload.line.second = second;
        command.payload.line.thickness = thickness;
        command.payload.line.color = color;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasPolyline(Context * ui, Id canvas, Vec2Span points, float thickness, const Color & color, bool closed)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas polyline target is not a canvas");

            return false;
        }

        if(points.size() < 2)
        {
            return true;
        }

        DrawCommand command(DrawCommandType::Polyline);
        PolylineDrawCommand & polyline = command.payload.polyline;
        polyline.points.offset = ui->drawCommandStorage.points.size();
        polyline.points.count = points.size();
        ui->drawCommandStorage.points.insert(ui->drawCommandStorage.points.end(), points.begin(), points.end());
        polyline.thickness = thickness;
        polyline.color = color;
        polyline.closed = closed;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasCircle(Context * ui, Id canvas, const Vec2 & center, float radius, float thickness, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas circle target is not a canvas");

            return false;
        }

        if(radius <= 0.f)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        command.payload.path.shape = PathShape::Circle;
        command.payload.path.center = center;
        command.payload.path.radii = {radius, radius};
        command.payload.path.color = color;
        command.payload.path.thickness = thickness;
        command.payload.path.segments = segments;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasConvexPolygon(Context * ui, Id canvas, Vec2Span points, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas convex polygon target is not a canvas");

            return false;
        }

        if(points.size() < 3)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::Polygon;
        pathCommand.points.offset = ui->drawCommandStorage.points.size();
        pathCommand.points.count = points.size();
        ui->drawCommandStorage.points.insert(ui->drawCommandStorage.points.end(), points.begin(), points.end());
        pathCommand.color = color;
        pathCommand.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasGradientPolygon(Context * ui, Id canvas, ColoredPointSpan points)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas gradient polygon target is not a canvas");

            return false;
        }

        if(points.size() < 3)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::Polygon;
        pathCommand.coloredPoints.offset = ui->drawCommandStorage.coloredPoints.size();
        pathCommand.coloredPoints.count = points.size();
        ui->drawCommandStorage.coloredPoints.insert(ui->drawCommandStorage.coloredPoints.end(), points.begin(), points.end());
        pathCommand.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasCircleFilled(Context * ui, Id canvas, const Vec2 & center, float radius, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas filled circle target is not a canvas");

            return false;
        }

        if(radius <= 0.f)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        command.payload.path.shape = PathShape::Circle;
        command.payload.path.center = center;
        command.payload.path.radii = {radius, radius};
        command.payload.path.color = color;
        command.payload.path.segments = segments;
        command.payload.path.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasArc(Context * ui, Id canvas, const Vec2 & center, float radius, float startAngle, float endAngle, float thickness, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas arc target is not a canvas");

            return false;
        }

        if(radius <= 0.f)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        command.payload.path.shape = PathShape::Arc;
        command.payload.path.center = center;
        command.payload.path.radii = {radius, radius};
        command.payload.path.startAngle = startAngle;
        command.payload.path.endAngle = endAngle;
        command.payload.path.color = color;
        command.payload.path.thickness = thickness;
        command.payload.path.segments = segments;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasArcFilled(Context * ui, Id canvas, const Vec2 & center, float radius, float startAngle, float endAngle, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas filled arc target is not a canvas");

            return false;
        }

        if(radius <= 0.f)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        command.payload.path.shape = PathShape::Arc;
        command.payload.path.center = center;
        command.payload.path.radii = {radius, radius};
        command.payload.path.startAngle = startAngle;
        command.payload.path.endAngle = endAngle;
        command.payload.path.color = color;
        command.payload.path.segments = segments;
        command.payload.path.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasEllipse(Context * ui, Id canvas, const Vec2 & center, const Vec2 & radii, float rotation, float thickness, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas ellipse target is not a canvas");

            return false;
        }

        if(radii.x <= 0.f)
        {
            return false;
        }

        if(radii.y <= 0.f)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        command.payload.path.shape = PathShape::Ellipse;
        command.payload.path.center = center;
        command.payload.path.radii = radii;
        command.payload.path.rotation = rotation;
        command.payload.path.color = color;
        command.payload.path.thickness = thickness;
        command.payload.path.segments = segments;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasEllipseFilled(Context * ui, Id canvas, const Vec2 & center, const Vec2 & radii, float rotation, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas filled ellipse target is not a canvas");

            return false;
        }

        if(radii.x <= 0.f)
        {
            return false;
        }

        if(radii.y <= 0.f)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        command.payload.path.shape = PathShape::Ellipse;
        command.payload.path.center = center;
        command.payload.path.radii = radii;
        command.payload.path.rotation = rotation;
        command.payload.path.color = color;
        command.payload.path.segments = segments;
        command.payload.path.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasRegularPolygon(Context * ui, Id canvas, const Vec2 & center, float radius, uint32_t sideCount, float rotation, float thickness, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas regular polygon target is not a canvas");

            return false;
        }

        if(radius <= 0.f)
        {
            return false;
        }

        if(sideCount < 3)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        command.payload.path.shape = PathShape::RegularPolygon;
        command.payload.path.center = center;
        command.payload.path.radii = {radius, radius};
        command.payload.path.rotation = rotation;
        command.payload.path.color = color;
        command.payload.path.thickness = thickness;
        command.payload.path.segments = sideCount;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasRegularPolygonFilled(Context * ui, Id canvas, const Vec2 & center, float radius, uint32_t sideCount, float rotation, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas filled regular polygon target is not a canvas");

            return false;
        }

        if(radius <= 0.f)
        {
            return false;
        }

        if(sideCount < 3)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        command.payload.path.shape = PathShape::RegularPolygon;
        command.payload.path.center = center;
        command.payload.path.radii = {radius, radius};
        command.payload.path.rotation = rotation;
        command.payload.path.color = color;
        command.payload.path.segments = sideCount;
        command.payload.path.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasGradientRing(Context * ui, Id canvas, const Vec2 & center, float innerRadius, float outerRadius, float startAngle, ColorSpan colors)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas gradient ring target is not a canvas");

            return false;
        }

        if(innerRadius < 0.f)
        {
            return false;
        }

        if(outerRadius <= innerRadius)
        {
            return false;
        }

        if(colors.empty() == true)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::GradientRing;
        pathCommand.center = center;
        pathCommand.radii = {innerRadius, outerRadius};
        pathCommand.rotation = startAngle;
        pathCommand.colors.offset = ui->drawCommandStorage.colors.size();
        pathCommand.colors.count = colors.size();
        ui->drawCommandStorage.colors.insert(ui->drawCommandStorage.colors.end(), colors.begin(), colors.end());
        pathCommand.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasConcavePolygon(Context * ui, Id canvas, Vec2Span points, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas concave polygon target is not a canvas");

            return false;
        }

        if(points.size() < 3)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::Polygon;
        pathCommand.points.offset = ui->drawCommandStorage.points.size();
        pathCommand.points.count = points.size();
        ui->drawCommandStorage.points.insert(ui->drawCommandStorage.points.end(), points.begin(), points.end());
        pathCommand.color = color;
        pathCommand.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasTriangle(Context * ui, Id canvas, const Vec2 & first, const Vec2 & second, const Vec2 & third, float thickness, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas triangle target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::Polygon;
        pathCommand.points.offset = ui->drawCommandStorage.points.size();
        pathCommand.points.count = 3;
        ui->drawCommandStorage.points.push_back(first);
        ui->drawCommandStorage.points.push_back(second);
        ui->drawCommandStorage.points.push_back(third);
        pathCommand.color = color;
        pathCommand.thickness = thickness;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasTriangleFilled(Context * ui, Id canvas, const Vec2 & first, const Vec2 & second, const Vec2 & third, const Color & color)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas filled triangle target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::Polygon;
        pathCommand.points.offset = ui->drawCommandStorage.points.size();
        pathCommand.points.count = 3;
        ui->drawCommandStorage.points.push_back(first);
        ui->drawCommandStorage.points.push_back(second);
        ui->drawCommandStorage.points.push_back(third);
        pathCommand.color = color;
        pathCommand.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasBezierCubic(Context * ui, Id canvas, const Vec2 & first, const Vec2 & firstControl, const Vec2 & secondControl, const Vec2 & second, float thickness, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas cubic bezier target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::CubicBezier;
        pathCommand.points.offset = ui->drawCommandStorage.points.size();
        pathCommand.points.count = 4;
        ui->drawCommandStorage.points.push_back(first);
        ui->drawCommandStorage.points.push_back(firstControl);
        ui->drawCommandStorage.points.push_back(secondControl);
        ui->drawCommandStorage.points.push_back(second);
        pathCommand.color = color;
        pathCommand.thickness = thickness;
        pathCommand.segments = segments;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasBezierQuadratic(Context * ui, Id canvas, const Vec2 & first, const Vec2 & control, const Vec2 & second, float thickness, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas quadratic bezier target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::QuadraticBezier;
        pathCommand.points.offset = ui->drawCommandStorage.points.size();
        pathCommand.points.count = 3;
        ui->drawCommandStorage.points.push_back(first);
        ui->drawCommandStorage.points.push_back(control);
        ui->drawCommandStorage.points.push_back(second);
        pathCommand.color = color;
        pathCommand.thickness = thickness;
        pathCommand.segments = segments;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasBezierQuadraticFilled(Context * ui, Id canvas, const Vec2 & first, const Vec2 & control, const Vec2 & second, const Color & color, uint32_t segments)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas filled quadratic bezier target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::Path);
        PathDrawCommand & pathCommand = command.payload.path;
        pathCommand.shape = PathShape::QuadraticBezier;
        pathCommand.points.offset = ui->drawCommandStorage.points.size();
        pathCommand.points.count = 3;
        ui->drawCommandStorage.points.push_back(first);
        ui->drawCommandStorage.points.push_back(control);
        ui->drawCommandStorage.points.push_back(second);
        pathCommand.color = color;
        pathCommand.segments = segments;
        pathCommand.filled = true;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasText(Context * ui, Id canvas, const Vec2 & position, StringView value, const Color & color, Vec2 * const _out)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            return false;
        }

        if(node->style == nullptr)
        {
            return false;
        }

        if(value.empty() == true)
        {
            return false;
        }

        if(_out == nullptr)
        {
            return false;
        }

        const Context::CachedText * text = ui->findOrCreateText(value, *node->style, false);

        if(text == nullptr)
        {
            return false;
        }

        uint64_t batchIndex = 0;
        for(const Context::PreparedTextRunBatch & batch : text->batches)
        {
            RenderState state;
            state.texture = batch.texture;
            state.renderTarget = ui->viewport.renderTarget;
            DrawCommand command(DrawCommandType::TextGeometry);
            command.renderKey = ui->internRenderState(state);
            TextGeometryDrawCommand & geometry = command.payload.textGeometry;
            uint64_t batchKey = combineId(text->key, batchIndex);
            geometry.cacheKey = combineId(batchKey, batch.texture);
            geometry.rectangles = batch.rectangles;
            geometry.translation = position;
            geometry.tint = color;
            ui->canvasCommands(*node).emplace_back(std::move(command));
            ++batchIndex;
        }

        *_out = text->size;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasPushClip(Context * ui, Id canvas, const Rect & bounds)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas clip target is not a canvas");

            return false;
        }

        if(bounds.empty() == true)
        {
            return false;
        }

        DrawCommand command(DrawCommandType::PushClip);
        command.payload.rectangle.bounds = bounds;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasPopClip(Context * ui, Id canvas)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas clip target is not a canvas");

            return false;
        }

        DrawCommand command(DrawCommandType::PopClip);
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasImage(Context * ui, Id canvas, TextureHandle texture, const Rect & bounds, const Rect & uv, const Color & tint, SamplerFilter sampler)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas image target is not a canvas");

            return false;
        }

        if(bounds.empty() == true)
        {
            return false;
        }

        RenderState state;
        state.texture = texture;
        state.sampler = sampler;
        state.renderTarget = ui->viewport.renderTarget;
        DrawCommand command(DrawCommandType::Image);
        command.renderKey = ui->internRenderState(state);
        command.payload.rectangle.bounds = bounds;
        command.payload.rectangle.uv = uv;
        command.payload.rectangle.color = tint;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasNineSlice(Context * ui, Id canvas, TextureHandle texture, const Rect & bounds, const NineSliceOptions & options, SamplerFilter sampler)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas nine-slice target is not a canvas");

            return false;
        }

        if(bounds.empty() == true)
        {
            return true;
        }

        if(options.sourceSize.x <= 0.f || options.sourceSize.y <= 0.f)
        {
            ui->frame.diagnostics.emplace_back("Canvas nine-slice source size must be positive");

            return false;
        }

        RenderState state;
        state.texture = texture;
        state.sampler = sampler;
        state.renderTarget = ui->viewport.renderTarget;
        DrawCommand command(DrawCommandType::NineSlice);
        command.renderKey = ui->internRenderState(state);
        command.payload.nineSlice.bounds = bounds;
        command.payload.nineSlice.options = options;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasGrid(Context * ui, Id canvas, const Rect & bounds, const GridStyle & style)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas grid target is not a canvas");

            return false;
        }

        if(bounds.empty() == true)
        {
            return true;
        }

        if(style.minorSpacing <= 0.f || style.majorInterval == 0)
        {
            ui->frame.diagnostics.emplace_back("Canvas grid spacing and major interval must be positive");

            return false;
        }

        if(style.bounded == true && style.range.empty() == true)
        {
            ui->frame.diagnostics.emplace_back("Canvas bounded grid range must be positive");

            return false;
        }

        RenderState state;
        state.renderTarget = ui->viewport.renderTarget;
        DrawCommand command(DrawCommandType::Grid);
        command.renderKey = ui->internRenderState(state);
        command.payload.grid.bounds = bounds;
        command.payload.grid.style = style;
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasCustom(Context * ui, Id canvas, VertexSpan vertices, IndexSpan indices, const RenderState & state)
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            ui->frame.diagnostics.emplace_back("Canvas custom geometry target is not a canvas");

            return false;
        }

        if(vertices.empty() == true)
        {
            return false;
        }

        if(indices.empty() == true)
        {
            return false;
        }

        RenderState canvasState = state;
        canvasState.renderTarget = ui->viewport.renderTarget;
        DrawCommand command(DrawCommandType::CustomGeometry);
        command.renderKey = ui->internRenderState(canvasState);
        CustomGeometryDrawCommand & custom = command.payload.custom;
        custom.vertices.offset = ui->drawCommandStorage.vertices.size();
        custom.vertices.count = vertices.size();
        ui->drawCommandStorage.vertices.insert(ui->drawCommandStorage.vertices.end(), vertices.begin(), vertices.end());
        custom.indices.offset = ui->drawCommandStorage.indices.size();
        custom.indices.count = indices.size();
        ui->drawCommandStorage.indices.insert(ui->drawCommandStorage.indices.end(), indices.begin(), indices.end());
        ui->canvasCommands(*node).emplace_back(std::move(command));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool canvasSetLayer(Context * ui, Id canvas, CanvasLayer layer) noexcept
    {
        Context::Node * node = Detail::findCanvasNode(ui, canvas);

        if(node == nullptr)
        {
            return false;
        }

        if(node->kind != Detail::NodeKind::Canvas)
        {
            return false;
        }

        node->canvasLayer = layer;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void Detail::closeScope(Context * ui, uint64_t token) noexcept
    {
        ui->closeScope(token);
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
