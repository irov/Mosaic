#include "Mosaic/Mosaic.hpp"

#include "Context.hpp"
#include "Popup.hpp"

namespace Mosaic
{
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::begin(Id source, TypeId type, ByteSpan data)
    {
        if(source == InvalidId)
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
        m_target = InvalidId;
        m_previousTarget = InvalidId;
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
        m_target = InvalidId;
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

        m_target = target;
        m_phase = m_previousTarget == target ? DragPhase::Over : DragPhase::Enter;
    }
    //////////////////////////////////////////////////////////////////////////
    void DragDrop::leave(Id target) noexcept
    {
        if(m_target == target && m_phase != DragPhase::None)
        {
            m_target = InvalidId;
            m_phase = DragPhase::Leave;
            m_sourcePreviewSuppressed = false;
            m_targetHighlightVisible = true;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::accepts(Id target, TypeId type) const noexcept
    {
        return target != InvalidId && type == m_type && m_phase != DragPhase::None && m_phase != DragPhase::Drop && m_phase != DragPhase::Cancel;
    }
    //////////////////////////////////////////////////////////////////////////
    bool DragDrop::drop(Id target, TypeId type) noexcept
    {
        if(accepts(target, type) == false)
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
        m_source = InvalidId;
        m_target = InvalidId;
        m_previousTarget = InvalidId;
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
        return m_source;
    }
    //////////////////////////////////////////////////////////////////////////
    Id DragDrop::target() const noexcept
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

        if(state.phase() == DragPhase::None && source.active() == true && Mosaic::pointerDragging(ui, PointerButton::Primary, options.threshold) == true)
        {
            (void)state.begin(source.id, type, data);
        }

        if(state.source() != source.id)
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

        if(state.source() == target.id)
        {
            state.leave(target.id);

            return result;
        }

        if(state.accepts(target.id, type) == false)
        {
            state.leave(target.id);

            return result;
        }

        size_t nodeIndex = ui->findFrameNodeIndex(target.id);

        if(nodeIndex >= ui->nodes.size())
        {
            state.leave(target.id);

            return result;
        }

        const Context::Node & node = ui->nodes[nodeIndex];
        const Context::Persistent * persistent = ui->findState(target.id);
        const PointerState * pointer = ui->input.primaryPointer();
        Id blockingLayer = ui->blockingInputLayer != InvalidId ? ui->blockingInputLayer : ui->previousBlockingInputLayer;
        bool inputLayerBlocked = blockingLayer != InvalidId && node.inputLayer != blockingLayer && Detail::popupOwnerCanInteract(ui, node.id) == false;
        bool hovered = pointer != nullptr && persistent != nullptr && node.disabled == false && node.inputBlocked == false && inputLayerBlocked == false && persistent->lastBounds.empty() == false && persistent->lastClip.empty() == false && persistent->lastBounds.contains(pointer->position) && persistent->lastClip.contains(pointer->position) && (ui->pointerWindow == InvalidId || node.windowOwner == ui->pointerWindow);

        if(hovered == false)
        {
            state.leave(target.id);

            return result;
        }

        state.enter(target.id);
        state.configureTarget(options.drawDefaultHighlight, options.suppressSourcePreview == false);
        result.preview = state.phase() == DragPhase::Over;

        if(options.acceptBeforeDelivery == true)
        {
            result.payload = state.payload();
        }

        if(pointer->isReleased(PointerButton::Primary) == true && state.drop(target.id, type) == true)
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
