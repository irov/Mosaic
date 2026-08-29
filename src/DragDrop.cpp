#include "Mosaic/Mosaic.hpp"

#include "Context.hpp"
#include "Popup.hpp"

#include <algorithm>

namespace Mosaic
{
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::begin(Id source, TypeId type, ByteSpan data)
    {
        ItemRef item;
        item.id = source;
        bool result = DragDrop::begin(item, type, data);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::begin(const ItemRef & source, TypeId type, ByteSpan data)
    {
        if(source.valid() == false)
        {
            return false;
        }

        if(type == 0)
        {
            return false;
        }

        if(m_phase != DragPhase::None)
        {
            return false;
        }

        m_source = source;
        m_target = {};
        m_previousTarget = {};
        m_targetSurfaceArea = std::numeric_limits<float>::max();
        m_type = type;
        m_data.assign(data.begin(), data.end());
        m_phase = DragPhase::Begin;
        m_sourcePreviewEnabled = true;
        m_sourcePreviewSuppressed = false;
        m_previousSourcePreviewSuppressed = false;
        m_targetHighlightVisible = true;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::beginFrame() noexcept
    {
        if(m_phase == DragPhase::Drop)
        {
            clear();

            return;
        }

        if(m_phase == DragPhase::Cancel)
        {
            clear();

            return;
        }

        if(m_phase == DragPhase::None)
        {
            return;
        }

        m_previousTarget = m_target;
        m_previousSourcePreviewSuppressed = m_sourcePreviewSuppressed;
        m_target = {};
        m_targetSurfaceArea = std::numeric_limits<float>::max();
        m_phase = DragPhase::Drag;
        m_sourcePreviewSuppressed = false;
        m_targetHighlightVisible = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::endFrame(bool primaryReleased) noexcept
    {
        if(primaryReleased == true && m_phase != DragPhase::None && m_phase != DragPhase::Drop && m_phase != DragPhase::Cancel)
        {
            cancel();
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::enter(Id target) noexcept
    {
        DragDrop::enter(target, std::numeric_limits<float>::max());
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::enter(Id target, float surfaceArea) noexcept
    {
        ItemRef item;
        item.id = target;
        DragDrop::enter(item, surfaceArea);
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::enter(const ItemRef & target, float surfaceArea) noexcept
    {
        if(m_phase == DragPhase::None)
        {
            return;
        }

        if(m_phase == DragPhase::Drop)
        {
            return;
        }

        if(m_phase == DragPhase::Cancel)
        {
            return;
        }

        if(target.valid() == false)
        {
            return;
        }

        float resolvedArea = std::max(0.f, surfaceArea);
        bool replaceTarget = m_target.valid() == false;

        if(replaceTarget == false && resolvedArea < m_targetSurfaceArea)
        {
            replaceTarget = true;
        }

        if(replaceTarget == false)
        {
            return;
        }

        m_target = target;
        m_targetSurfaceArea = resolvedArea;
        m_phase = m_previousTarget == target ? DragPhase::Over : DragPhase::Enter;
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::leave(Id target) noexcept
    {
        ItemRef item;
        item.id = target;
        DragDrop::leave(item);
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::leave(const ItemRef & target) noexcept
    {
        if(m_target == target && m_phase != DragPhase::None)
        {
            m_target = {};
            m_targetSurfaceArea = std::numeric_limits<float>::max();
            m_phase = DragPhase::Leave;
            m_sourcePreviewSuppressed = false;
            m_targetHighlightVisible = true;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::accepts(Id target, TypeId type) const noexcept
    {
        ItemRef item;
        item.id = target;
        bool result = DragDrop::accepts(item, type);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::accepts(const ItemRef & target, TypeId type) const noexcept
    {
        return target.valid() == true && type == m_type && m_phase != DragPhase::None && m_phase != DragPhase::Drop && m_phase != DragPhase::Cancel;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::drop(Id target, TypeId type) noexcept
    {
        ItemRef item;
        item.id = target;
        bool result = DragDrop::drop(item, type);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::drop(const ItemRef & target, TypeId type) noexcept
    {
        if(accepts(target, type) == false)
        {
            return false;
        }

        ItemRef owner = m_previousTarget.valid() == false ? m_target : m_previousTarget;

        if(owner != target)
        {
            return false;
        }

        m_target = target;
        m_phase = DragPhase::Drop;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::cancel() noexcept
    {
        if(m_phase != DragPhase::None)
        {
            m_phase = DragPhase::Cancel;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::clear() noexcept
    {
        m_phase = DragPhase::None;
        m_source = {};
        m_target = {};
        m_previousTarget = {};
        m_targetSurfaceArea = std::numeric_limits<float>::max();
        m_type = 0;
        m_data.clear();
        m_sourcePreviewEnabled = true;
        m_sourcePreviewSuppressed = false;
        m_previousSourcePreviewSuppressed = false;
        m_targetHighlightVisible = true;
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::configureSource(bool previewVisible) noexcept
    {
        m_sourcePreviewEnabled = previewVisible;
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::configureTarget(bool highlightVisible, bool sourcePreviewVisible) noexcept
    {
        m_targetHighlightVisible = highlightVisible;
        m_sourcePreviewSuppressed = sourcePreviewVisible == false;
    }
    //////////////////////////////////////////////////////////////////////////
    DragPhase DragDrop::phase() const noexcept
    {
        return m_phase;
    }
    //////////////////////////////////////////////////////////////////////////
    Id DragDrop::source() const noexcept
    {
        return m_source.id;
    }
    //////////////////////////////////////////////////////////////////////////
    Id DragDrop::target() const noexcept
    {
        return m_target.id;
    }
    //////////////////////////////////////////////////////////////////////////
    ItemRef DragDrop::sourceItem() const noexcept
    {
        return m_source;
    }
    //////////////////////////////////////////////////////////////////////////
    ItemRef DragDrop::targetItem() const noexcept
    {
        return m_target;
    }
    //////////////////////////////////////////////////////////////////////////
    DragPayload DragDrop::payload() const noexcept
    {
        return {m_type, m_data};
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::sourcePreviewVisible() const noexcept
    {
        return m_sourcePreviewEnabled && m_previousSourcePreviewSuppressed == false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::targetHighlightVisible() const noexcept
    {
        return m_targetHighlightVisible;
    }
    //////////////////////////////////////////////////////////////////////////
    bool beginDragDropSource(Context * ui, const Response & source, TypeId type, ByteSpan data, const DragDropSourceOptions & options)
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(source.id == InvalidId)
        {
            return false;
        }

        if(source.disabled() == true)
        {
            return false;
        }

        DragDrop & state = ui->dragDrop;
        ItemRef sourceItem = source.item;

        if(sourceItem.valid() == false)
        {
            sourceItem.id = source.id;
        }

        bool ownsCapture = ui->capturedItem.valid() == true ? ui->capturedItem == sourceItem : ui->captured == source.id;

        if(state.phase() == DragPhase::None && source.active() == true && ownsCapture == true && Mosaic::pointerDragging(ui, PointerButton::Primary, options.threshold) == true)
        {
            (void)state.begin(sourceItem, type, data);
        }

        if(state.sourceItem() != sourceItem)
        {
            return false;
        }

        if(state.phase() == DragPhase::None)
        {
            return false;
        }

        if(state.phase() == DragPhase::Drop)
        {
            return false;
        }

        if(state.phase() == DragPhase::Cancel)
        {
            return false;
        }

        state.configureSource(options.previewVisible);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    DragDropAcceptResult acceptDragDropPayload(Context * ui, const Response & target, TypeId type, const DragDropTargetOptions & options)
    {
        DragDropAcceptResult result;

        if(ui == nullptr)
        {
            return result;
        }

        if(target.id == InvalidId)
        {
            return result;
        }

        if(target.disabled() == true)
        {
            return result;
        }

        DragDrop & state = ui->dragDrop;
        ItemRef targetItem = target.item;

        if(targetItem.valid() == false)
        {
            targetItem.id = target.id;
        }

        if(state.sourceItem() == targetItem)
        {
            state.leave(targetItem);

            return result;
        }

        if(state.accepts(targetItem, type) == false)
        {
            state.leave(targetItem);

            return result;
        }

        const Context::InteractionSnapshotItem * snapshot = ui->findInteractionSnapshot(targetItem);

        if(snapshot == nullptr)
        {
            state.leave(targetItem);

            return result;
        }

        const PointerState * pointer = ui->input.primaryPointer();
        Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;
        bool inputLayerBlocked = blockingLayer != InvalidId && snapshot->inputLayer != blockingLayer && Detail::popupOwnerCanInteract(ui, target.id) == false;
        bool matchingWindow = ui->pointerWindow == InvalidId || (snapshot->windowOwner == ui->pointerWindow && (ui->pointerWindowSubmission == 0 || snapshot->windowSubmission == ui->pointerWindowSubmission));
        bool hovered = pointer != nullptr && snapshot->visible == true && snapshot->disabled == false && snapshot->inputBlocked == false && inputLayerBlocked == false && snapshot->bounds.empty() == false && snapshot->clip.empty() == false && snapshot->bounds.contains(pointer->position) && snapshot->clip.contains(pointer->position) && matchingWindow == true;

        if(hovered == false)
        {
            state.leave(targetItem);

            return result;
        }

        Rect visibleBounds = Rect::intersection(snapshot->bounds, snapshot->clip);
        float surfaceArea = visibleBounds.width * visibleBounds.height;
        state.enter(targetItem, surfaceArea);

        if(state.targetItem() != targetItem)
        {
            return result;
        }

        state.configureTarget(options.drawDefaultHighlight, options.suppressSourcePreview == false);
        result.preview = state.phase() == DragPhase::Over;

        if(options.acceptBeforeDelivery == true)
        {
            result.payload = state.payload();
        }

        if(pointer->isReleased(PointerButton::Primary) == true && state.drop(targetItem, type) == true)
        {
            result.payload = state.payload();
            result.delivery = true;
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool dragDropSourcePreviewVisible(const Context * ui) noexcept
    {
        auto returnedValue = ui != nullptr && ui->dragDrop.sourcePreviewVisible();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void cancelDragDrop(Context * ui) noexcept
    {
        if(ui != nullptr)
        {
            ui->dragDrop.cancel();
        }
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
