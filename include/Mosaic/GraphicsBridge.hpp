#pragma once

#include "Mosaic/Frame.hpp"

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
        explicit GraphicsBridge(Allocator * allocator = nullptr);
        ~GraphicsBridge();

        GraphicsBridge(const GraphicsBridge &) = delete;
        GraphicsBridge & operator=(const GraphicsBridge &) = delete;
        GraphicsBridge(GraphicsBridge &&) = delete;
        GraphicsBridge & operator=(GraphicsBridge &&) = delete;

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
