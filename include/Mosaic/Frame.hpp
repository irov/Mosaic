#pragma once

#include "Mosaic/DrawTypes.hpp"
#include "Mosaic/Input.hpp"

namespace Mosaic
{
    struct FrameViewport
    {
        uint64_t id = 0;
        Rect bounds;
        float dpiScale = 1.f;
        void * nativeHandle = nullptr;
        RenderTargetHandle renderTarget = 0;
        DrawCommandVector drawCommands;
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
