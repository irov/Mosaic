#pragma once

#include "Mosaic/DrawTypes.hpp"
#include "Mosaic/Input.hpp"
#include "Mosaic/Platform.hpp"

namespace Mosaic
{
    namespace Detail
    {
        struct FrameRenderData;
        struct FrameViewportAccess;
    }

    class FrameViewport
    {
    public:
        uint64_t id = 0;
        uint64_t surfaceId = 0;
        NativeSurfaceKind kind = NativeSurfaceKind::Scene;
        Rect bounds;
        Rect localBounds;
        Rect screenBounds;
        float dpiScale = 1.f;
        NativeSurfaceHandle nativeHandle = nullptr;
        RenderTargetHandle renderTarget = 0;
        uint64_t generation = 0;
        uint64_t applicationTag = 0;
        bool visible = true;

        [[nodiscard]] size_t drawCommandCount() const noexcept
        {
            return m_drawCommandCount;
        }

    private:
        Detail::FrameRenderData * m_renderData = nullptr;
        size_t m_drawCommandCount = 0;

        friend struct Detail::FrameViewportAccess;
    };

    using FrameViewportVector = Vector<FrameViewport>;

    struct TextInputState
    {
        Id owner = InvalidId;
        String value;
        String composition;
        size_t cursor = 0;
        size_t anchor = 0;
        size_t compositionBegin = 0;
        size_t compositionEnd = 0;
        size_t compositionSelectionBegin = 0;
        size_t compositionSelectionEnd = 0;
        bool active = false;
        bool password = false;
        bool multiline = false;
    };

    struct Frame
    {
        uint64_t number = 0;
        InputCapture inputCapture;
        TextInputState textInput;
        FrameViewportVector viewports;
        RenderStateVector renderStates;
        SemanticNodeVector semantics;
        DebugInfoVector debug;
        EventTraceVector events;
        StringVector diagnostics;
        FrameMetrics metrics;

        [[nodiscard]] const RenderState * renderState(uint64_t key) const noexcept
        {
            if(key == 0)
            {
                return nullptr;
            }

            if(key > renderStates.size())
            {
                return nullptr;
            }

            auto returnedValue = &renderStates[static_cast<size_t>(key - 1)];

            return returnedValue;
        }
    };
} // namespace Mosaic
