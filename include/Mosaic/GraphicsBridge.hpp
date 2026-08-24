#pragma once

#include "Mosaic/Frame.hpp"

namespace Mosaic
{
    class PlatformAdapter;

    class GraphicsBridge
    {
    public:
        GraphicsBridge();
        ~GraphicsBridge();

        GraphicsBridge(const GraphicsBridge &) = delete;
        GraphicsBridge & operator=(const GraphicsBridge &) = delete;
        GraphicsBridge(GraphicsBridge &&) = delete;
        GraphicsBridge & operator=(GraphicsBridge &&) = delete;

        [[nodiscard]] bool build(const Frame & frame, RenderMesh * const _out, size_t viewportIndex = 0) const;
        [[nodiscard]] bool build(const Frame & frame, RenderMesh * const _out, PlatformAdapter & platform, size_t viewportIndex = 0) const;

    private:
        [[nodiscard]] bool buildWithPlatform(const Frame & frame, RenderMesh * const _out, PlatformAdapter * platform, size_t viewportIndex) const;

    private:
        [[maybe_unused]] mutable void * m_canvas = nullptr;
    };
} // namespace Mosaic
