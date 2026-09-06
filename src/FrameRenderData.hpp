#pragma once

#include "DrawCommand.hpp"
#include "Mosaic/Frame.hpp"

namespace Mosaic
{
    namespace Detail
    {
        struct FrameRenderData
        {
            DrawCommandVector commands;
            DrawCommandStorage storage;
            const DrawCommandStorage * sharedStorage = nullptr;
        };

        struct FrameViewportAccess
        {
            [[nodiscard]] static FrameRenderData * renderData(FrameViewport & viewport) noexcept
            {
                return viewport.m_renderData;
            }

            [[nodiscard]] static const FrameRenderData * renderData(const FrameViewport & viewport) noexcept
            {
                return viewport.m_renderData;
            }

            static void setRenderData(FrameViewport & viewport, FrameRenderData * data) noexcept
            {
                viewport.m_renderData = data;
                viewport.m_drawCommandCount = data == nullptr ? 0 : data->commands.size();
            }

            [[nodiscard]] static DrawCommandVector * commands(FrameViewport & viewport) noexcept
            {
                FrameRenderData * data = viewport.m_renderData;

                if(data == nullptr)
                {
                    return nullptr;
                }

                return &data->commands;
            }

            [[nodiscard]] static const DrawCommandVector * commands(const FrameViewport & viewport) noexcept
            {
                const FrameRenderData * data = viewport.m_renderData;

                if(data == nullptr)
                {
                    return nullptr;
                }

                return &data->commands;
            }

            [[nodiscard]] static const DrawCommandStorage * storage(const FrameViewport & viewport) noexcept
            {
                const FrameRenderData * data = viewport.m_renderData;

                if(data == nullptr)
                {
                    return nullptr;
                }

                if(data->sharedStorage != nullptr)
                {
                    return data->sharedStorage;
                }

                return &data->storage;
            }
        };

        using FrameRenderDataPtr = UniquePtr<FrameRenderData>;
        using FrameRenderDataPtrVector = Vector<FrameRenderDataPtr>;
    }
} // namespace Mosaic
