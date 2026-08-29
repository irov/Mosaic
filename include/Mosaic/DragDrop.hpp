#pragma once

#include "Mosaic/Types.hpp"

#include <limits>

namespace Mosaic
{
    enum class DragPhase : uint8_t
    {
        None,
        Begin,
        Drag,
        Enter,
        Over,
        Leave,
        Drop,
        Cancel
    };

    class DragDrop
    {
    public:
        bool begin(Id source, TypeId type, ByteSpan data);
        bool begin(const ItemRef & source, TypeId type, ByteSpan data);
        void beginFrame() noexcept;
        void endFrame(bool primaryReleased) noexcept;
        void enter(Id target) noexcept;
        void enter(Id target, float surfaceArea) noexcept;
        void enter(const ItemRef & target, float surfaceArea) noexcept;
        void leave(Id target) noexcept;
        void leave(const ItemRef & target) noexcept;
        [[nodiscard]] bool accepts(Id target, TypeId type) const noexcept;
        [[nodiscard]] bool accepts(const ItemRef & target, TypeId type) const noexcept;
        [[nodiscard]] bool drop(Id target, TypeId type) noexcept;
        [[nodiscard]] bool drop(const ItemRef & target, TypeId type) noexcept;
        void cancel() noexcept;
        void clear() noexcept;

        void configureSource(bool previewVisible) noexcept;
        void configureTarget(bool highlightVisible, bool sourcePreviewVisible) noexcept;

        [[nodiscard]] DragPhase phase() const noexcept;
        [[nodiscard]] Id source() const noexcept;
        [[nodiscard]] Id target() const noexcept;
        [[nodiscard]] ItemRef sourceItem() const noexcept;
        [[nodiscard]] ItemRef targetItem() const noexcept;
        [[nodiscard]] DragPayload payload() const noexcept;
        [[nodiscard]] bool sourcePreviewVisible() const noexcept;
        [[nodiscard]] bool targetHighlightVisible() const noexcept;

    private:
        DragPhase m_phase = DragPhase::None;
        ItemRef m_source;
        ItemRef m_target;
        ItemRef m_previousTarget;
        float m_targetSurfaceArea = std::numeric_limits<float>::max();
        TypeId m_type = 0;
        ByteVector m_data;
        bool m_sourcePreviewEnabled = true;
        bool m_sourcePreviewSuppressed = false;
        bool m_previousSourcePreviewSuppressed = false;
        bool m_targetHighlightVisible = true;
    };
} // namespace Mosaic
