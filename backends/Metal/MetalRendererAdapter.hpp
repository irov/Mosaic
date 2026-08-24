#pragma once

#include <Mosaic/Mosaic.hpp>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <dispatch/dispatch.h>

namespace Mosaic
{
    class MetalRendererAdapter final : public RendererAdapter
    {
    public:
        MetalRendererAdapter(id<MTLDevice> device, MTKView * view);

        [[nodiscard]] TextureHandle createTexture(uint32_t width, uint32_t height, ByteSpan rgbaPixels) override;
        [[nodiscard]] TextureHandle createMaskTexture(uint32_t width, uint32_t height, ByteSpan maskPixels) override;
        bool updateTexture(TextureHandle texture, uint32_t width, uint32_t height, ByteSpan rgbaPixels) override;
        bool updateTextureRegion(TextureHandle texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t bytesPerRow, ByteSpan rgbaPixels) override;
        bool updateMaskTextureRegion(TextureHandle texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t bytesPerRow, ByteSpan maskPixels) override;
        void destroyTexture(TextureHandle texture) override;

        void render(const FrameViewport & viewport, const RenderMesh & mesh) override;

    private:
        [[nodiscard]] id<MTLRenderPipelineState> pipeline(BlendMode blend);
        [[nodiscard]] id<MTLTexture> texture(TextureHandle handle) const;
        void ensureVertexCapacity(size_t frameIndex, size_t bytes);
        void ensureIndexCapacity(size_t frameIndex, size_t bytes);

    private:
        static constexpr size_t FrameBufferCount = 3;
        using BufferArray = Array<id<MTLBuffer>, FrameBufferCount>;
        using CapacityArray = Array<size_t, FrameBufferCount>;

        id<MTLDevice> m_device;
        __unsafe_unretained MTKView * m_view;
        id<MTLCommandQueue> m_commandQueue;
        id<MTLLibrary> m_library;
        NSMutableDictionary<NSNumber *, id<MTLRenderPipelineState>> * m_pipelines;
        NSMutableDictionary<NSNumber *, id<MTLTexture>> * m_textures;
        NSMutableSet<NSNumber *> * m_maskTextures;
        BufferArray m_vertexBuffers = {};
        BufferArray m_indexBuffers = {};
        CapacityArray m_vertexCapacities = {};
        CapacityArray m_indexCapacities = {};
        dispatch_semaphore_t m_frameSemaphore;
        size_t m_frameIndex = 0;
        TextureHandle m_whiteTexture = 0;
        TextureHandle m_nextTexture = 1;
    };
} // namespace Mosaic
