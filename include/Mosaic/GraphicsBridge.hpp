#pragma once

#include "Mosaic/Frame.hpp"

struct gp_graphics_t;

namespace Mosaic
{
    class PlatformAdapter;

    namespace Detail
    {
        struct GraphicsBridgeState;
    }

    class GraphicsBridge
    {
    public:
        GraphicsBridge();
        ~GraphicsBridge();

        GraphicsBridge(const GraphicsBridge &) = delete;
        GraphicsBridge & operator=(const GraphicsBridge &) = delete;
        GraphicsBridge(GraphicsBridge &&) = delete;
        GraphicsBridge & operator=(GraphicsBridge &&) = delete;

        // graphics is the host-owned irov/graphics object every canvas of this
        // bridge is created from.
        [[nodiscard]] bool initialize(gp_graphics_t * graphics, Allocator * allocator = nullptr);
        void finalize();

        [[nodiscard]] bool build(const Frame & frame, RenderMesh * const _out, size_t viewportIndex = 0) const;
        [[nodiscard]] bool build(const Frame & frame, RenderMesh * const _out, PlatformAdapter & platform, size_t viewportIndex = 0) const;
        [[nodiscard]] bool prepare(const Frame & frame, size_t viewportIndex = 0) const;
        [[nodiscard]] bool prepare(const Frame & frame, PlatformAdapter & platform, size_t viewportIndex = 0) const;
        // The source frame must remain valid only for the duration of prepare.
        // Returned data is owned by this bridge and remains valid until its next prepare call.
        [[nodiscard]] const RenderMesh * renderData() const noexcept;
        [[nodiscard]] StringView lastError() const noexcept;

    private:
        [[nodiscard]] bool buildWithPlatform(const Frame & frame, RenderMesh * const _out, PlatformAdapter * platform, size_t viewportIndex) const;

    private:
        [[maybe_unused]] mutable Detail::GraphicsBridgeState * m_state = nullptr;
        mutable StringView m_lastError;
    };
} // namespace Mosaic
