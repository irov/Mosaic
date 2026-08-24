#pragma once

#include "Mosaic/Frame.hpp"

namespace Mosaic
{
    class RendererAdapter
    {
    public:
        virtual ~RendererAdapter() = default;
        [[nodiscard]] virtual TextureHandle createTexture(uint32_t width, uint32_t height, ByteSpan rgbaPixels) = 0;

        [[nodiscard]] virtual TextureHandle createMaskTexture(uint32_t width, uint32_t height, ByteSpan maskPixels)
        {
            (void)width;
            (void)height;
            (void)maskPixels;

            return 0;
        }

        virtual bool updateTexture(TextureHandle texture, uint32_t width, uint32_t height, ByteSpan rgbaPixels)
        {
            (void)texture;
            (void)width;
            (void)height;
            (void)rgbaPixels;

            return false;
        }

        virtual bool updateTextureRegion(TextureHandle texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t bytesPerRow, ByteSpan rgbaPixels)
        {
            (void)texture;
            (void)x;
            (void)y;
            (void)width;
            (void)height;
            (void)bytesPerRow;
            (void)rgbaPixels;

            return false;
        }

        virtual bool updateMaskTextureRegion(TextureHandle texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t bytesPerRow, ByteSpan maskPixels)
        {
            (void)texture;
            (void)x;
            (void)y;
            (void)width;
            (void)height;
            (void)bytesPerRow;
            (void)maskPixels;

            return false;
        }

        virtual void destroyTexture(TextureHandle texture)
        {
            (void)texture;
        }

        virtual void render(const FrameViewport & viewport, const RenderMesh & mesh) = 0;
    };
} // namespace Mosaic
