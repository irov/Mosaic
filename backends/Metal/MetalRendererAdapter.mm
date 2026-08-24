#include "MetalRendererAdapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Mosaic
{
    namespace Detail
    {
        inline constexpr const char * MetalShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct MosaicVertex
{
    packed_float2 position;
    uint color;
    packed_float2 uv;
};

struct RasterData
{
    float4 position [[position]];
    float4 color;
    float2 uv;
};

vertex RasterData mosaicVertex(uint vertexId [[vertex_id]], const device MosaicVertex * vertices [[buffer(0)]], constant float2 & viewportSize [[buffer(1)]])
{
    MosaicVertex input = vertices[vertexId];
    float2 logicalPosition = float2(input.position);
    constexpr float channelScale = 1.0 / 255.0;

    RasterData output;
    output.position = float4(logicalPosition.x / viewportSize.x * 2.0 - 1.0, 1.0 - logicalPosition.y / viewportSize.y * 2.0, 0.0, 1.0);
    output.color = float4(float((input.color >> 16) & 0xff) * channelScale, float((input.color >> 8) & 0xff) * channelScale, float(input.color & 0xff) * channelScale, float((input.color >> 24) & 0xff) * channelScale);
    output.uv = float2(input.uv);

    return output;
}

fragment float4 mosaicFragment(RasterData input [[stage_in]], texture2d<float> colorTexture [[texture(0)]], constant uint & maskTexture [[buffer(0)]])
{
    constexpr sampler textureSampler(coord::normalized, address::clamp_to_edge, filter::linear);
    float4 sampled = colorTexture.sample(textureSampler, input.uv);
    float4 texel = maskTexture == 0 ? sampled : float4(1.0, 1.0, 1.0, sampled.r);
    float4 color = texel * input.color;
    color.rgb *= color.a;

    return color;
}
)";
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t nextCapacity(size_t required) noexcept
        {
            size_t capacity = 4096;
            while(capacity < required)
            {
                capacity *= 2;
            }

            return capacity;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] MTLScissorRect scissorRect(const Rect & logicalClip, const FrameViewport & viewport, id<MTLTexture> target) noexcept
        {
            float logicalWidth = std::max(1.f, viewport.bounds.width);
            float logicalHeight = std::max(1.f, viewport.bounds.height);
            double scaleX = static_cast<double>(target.width) / logicalWidth;
            double scaleY = static_cast<double>(target.height) / logicalHeight;

            double left = std::floor((logicalClip.x - viewport.bounds.x) * scaleX);
            double top = std::floor((logicalClip.y - viewport.bounds.y) * scaleY);
            double right = std::ceil((logicalClip.right() - viewport.bounds.x) * scaleX);
            double bottom = std::ceil((logicalClip.bottom() - viewport.bounds.y) * scaleY);

            NSUInteger x = static_cast<NSUInteger>(std::clamp(left, 0.0, static_cast<double>(target.width)));
            NSUInteger y = static_cast<NSUInteger>(std::clamp(top, 0.0, static_cast<double>(target.height)));
            NSUInteger maximumX = static_cast<NSUInteger>(std::clamp(right, static_cast<double>(x), static_cast<double>(target.width)));
            NSUInteger maximumY = static_cast<NSUInteger>(std::clamp(bottom, static_cast<double>(y), static_cast<double>(target.height)));

            return {x, y, maximumX - x, maximumY - y};
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    MetalRendererAdapter::MetalRendererAdapter(id<MTLDevice> device, MTKView * view) : m_device(device), m_view(view), m_commandQueue([device newCommandQueue]), m_pipelines([NSMutableDictionary dictionary]), m_textures([NSMutableDictionary dictionary]), m_maskTextures([NSMutableSet set]), m_frameSemaphore(dispatch_semaphore_create(FrameBufferCount))
    {
        static_assert(sizeof(RenderVertex) == sizeof(float) * 5);
        static_assert(offsetof(RenderVertex, position) == 0);
        static_assert(offsetof(RenderVertex, color) == sizeof(float) * 2);
        static_assert(offsetof(RenderVertex, uv) == sizeof(float) * 3);

        NSError * error = nil;
        NSString * source = [NSString stringWithUTF8String:Detail::MetalShaderSource];
        m_library = [m_device newLibraryWithSource:source options:nil error:&error];

        if(m_library == nil)
        {
            NSLog(@"Mosaic Metal shader error: %@", error.localizedDescription);
        }

        ByteVector whitePixels(4, std::byte{0xff});
        m_whiteTexture = createTexture(1, 1, whitePixels);
    }
    //////////////////////////////////////////////////////////////////////////
    TextureHandle MetalRendererAdapter::createTexture(uint32_t width, uint32_t height, ByteSpan rgbaPixels)
    {
        if(width == 0)
        {
            return 0;
        }

        if(height == 0)
        {
            return 0;
        }

        if(rgbaPixels.size() != static_cast<size_t>(width) * height * 4)
        {
            return 0;
        }

        MTLTextureDescriptor * descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> texture = [m_device newTextureWithDescriptor:descriptor];
        [texture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:rgbaPixels.data() bytesPerRow:static_cast<NSUInteger>(width) * 4];

        TextureHandle handle = m_nextTexture++;
        m_textures[@(handle)] = texture;

        return handle;
    }
    //////////////////////////////////////////////////////////////////////////
    TextureHandle MetalRendererAdapter::createMaskTexture(uint32_t width, uint32_t height, ByteSpan maskPixels)
    {
        if(width == 0)
        {
            return 0;
        }

        if(height == 0)
        {
            return 0;
        }

        if(maskPixels.size() != static_cast<size_t>(width) * height)
        {
            return 0;
        }

        MTLTextureDescriptor * descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:width height:height mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> texture = [m_device newTextureWithDescriptor:descriptor];
        [texture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:maskPixels.data() bytesPerRow:width];

        TextureHandle handle = m_nextTexture++;
        NSNumber * key = @(handle);
        m_textures[key] = texture;
        [m_maskTextures addObject:key];

        return handle;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MetalRendererAdapter::updateTexture(TextureHandle handle, uint32_t width, uint32_t height, ByteSpan rgbaPixels)
    {
        id<MTLTexture> existing = m_textures[@(handle)];

        if(existing == nil)
        {
            return false;
        }

        if(width == 0)
        {
            return false;
        }

        if(height == 0)
        {
            return false;
        }

        if(existing.width != width)
        {
            return false;
        }

        if(existing.height != height)
        {
            return false;
        }

        if(rgbaPixels.size() != static_cast<size_t>(width) * height * 4)
        {
            return false;
        }

        [existing replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:rgbaPixels.data() bytesPerRow:static_cast<NSUInteger>(width) * 4];

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MetalRendererAdapter::updateTextureRegion(TextureHandle handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t bytesPerRow, ByteSpan rgbaPixels)
    {
        id<MTLTexture> existing = m_textures[@(handle)];
        size_t requiredSize = height == 0 ? 0 : static_cast<size_t>(bytesPerRow) * (height - 1) + static_cast<size_t>(width) * 4;

        if(existing == nil)
        {
            return false;
        }

        if(width == 0)
        {
            return false;
        }

        if(height == 0)
        {
            return false;
        }

        if(bytesPerRow < width * 4)
        {
            return false;
        }

        if(x + width > existing.width)
        {
            return false;
        }

        if(y + height > existing.height)
        {
            return false;
        }

        if(rgbaPixels.size() < requiredSize)
        {
            return false;
        }

        [existing replaceRegion:MTLRegionMake2D(x, y, width, height) mipmapLevel:0 withBytes:rgbaPixels.data() bytesPerRow:bytesPerRow];

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MetalRendererAdapter::updateMaskTextureRegion(TextureHandle handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t bytesPerRow, ByteSpan maskPixels)
    {
        NSNumber * key = @(handle);
        id<MTLTexture> existing = m_textures[key];
        size_t requiredSize = height == 0 ? 0 : static_cast<size_t>(bytesPerRow) * (height - 1) + width;

        if(existing == nil)
        {
            return false;
        }

        if([m_maskTextures containsObject:key] == false)
        {
            return false;
        }

        if(width == 0)
        {
            return false;
        }

        if(height == 0)
        {
            return false;
        }

        if(bytesPerRow < width)
        {
            return false;
        }

        if(x + width > existing.width)
        {
            return false;
        }

        if(y + height > existing.height)
        {
            return false;
        }

        if(maskPixels.size() < requiredSize)
        {
            return false;
        }

        [existing replaceRegion:MTLRegionMake2D(x, y, width, height) mipmapLevel:0 withBytes:maskPixels.data() bytesPerRow:bytesPerRow];

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void MetalRendererAdapter::destroyTexture(TextureHandle handle)
    {
        if(handle == 0)
        {
            return;
        }

        if(handle == m_whiteTexture)
        {
            return;
        }

        NSNumber * key = @(handle);
        [m_maskTextures removeObject:key];
        [m_textures removeObjectForKey:key];
    }
    //////////////////////////////////////////////////////////////////////////
    id<MTLRenderPipelineState> MetalRendererAdapter::pipeline(BlendMode blend)
    {
        NSNumber * key = @(static_cast<uint8_t>(blend));
        id<MTLRenderPipelineState> cached = m_pipelines[key];

        if(cached != nil)
        {
            return cached;
        }

        MTLRenderPipelineDescriptor * descriptor = [[MTLRenderPipelineDescriptor alloc] init];
        descriptor.label = @"Mosaic UI Pipeline";
        descriptor.vertexFunction = [m_library newFunctionWithName:@"mosaicVertex"];
        descriptor.fragmentFunction = [m_library newFunctionWithName:@"mosaicFragment"];
        descriptor.colorAttachments[0].pixelFormat = m_view.colorPixelFormat;
        descriptor.rasterSampleCount = m_view.sampleCount;

        MTLRenderPipelineColorAttachmentDescriptor * color = descriptor.colorAttachments[0];
        color.blendingEnabled = blend != BlendMode::Opaque;
        color.rgbBlendOperation = MTLBlendOperationAdd;
        color.alphaBlendOperation = MTLBlendOperationAdd;
        color.sourceRGBBlendFactor = MTLBlendFactorOne;
        color.sourceAlphaBlendFactor = MTLBlendFactorOne;
        color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        color.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        if(blend == BlendMode::Additive)
        {
            color.destinationRGBBlendFactor = MTLBlendFactorOne;
            color.destinationAlphaBlendFactor = MTLBlendFactorOne;
        }
        else if(blend == BlendMode::Multiply)
        {
            color.sourceRGBBlendFactor = MTLBlendFactorDestinationColor;
            color.destinationRGBBlendFactor = MTLBlendFactorZero;
        }

        NSError * error = nil;
        id<MTLRenderPipelineState> created = [m_device newRenderPipelineStateWithDescriptor:descriptor error:&error];

        if(created == nil)
        {
            NSLog(@"Mosaic Metal pipeline error: %@", error.localizedDescription);

            return nil;
        }

        m_pipelines[key] = created;

        return created;
    }
    //////////////////////////////////////////////////////////////////////////
    id<MTLTexture> MetalRendererAdapter::texture(TextureHandle handle) const
    {
        id<MTLTexture> resolved = m_textures[@(handle)];
        auto returnedValue = resolved == nil ? m_textures[@(m_whiteTexture)] : resolved;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void MetalRendererAdapter::ensureVertexCapacity(size_t frameIndex, size_t bytes)
    {
        if(bytes <= m_vertexCapacities[frameIndex])
        {
            return;
        }

        m_vertexCapacities[frameIndex] = Detail::nextCapacity(bytes);
        m_vertexBuffers[frameIndex] = [m_device newBufferWithLength:m_vertexCapacities[frameIndex] options:MTLResourceStorageModeShared];
        m_vertexBuffers[frameIndex].label = @"Mosaic Vertices";
    }
    //////////////////////////////////////////////////////////////////////////
    void MetalRendererAdapter::ensureIndexCapacity(size_t frameIndex, size_t bytes)
    {
        if(bytes <= m_indexCapacities[frameIndex])
        {
            return;
        }

        m_indexCapacities[frameIndex] = Detail::nextCapacity(bytes);
        m_indexBuffers[frameIndex] = [m_device newBufferWithLength:m_indexCapacities[frameIndex] options:MTLResourceStorageModeShared];
        m_indexBuffers[frameIndex].label = @"Mosaic Indices";
    }
    //////////////////////////////////////////////////////////////////////////
    void MetalRendererAdapter::render(const FrameViewport & viewport, const RenderMesh & mesh)
    {
        MTKView * view = m_view;
        MTLRenderPassDescriptor * pass = view.currentRenderPassDescriptor;
        id<CAMetalDrawable> drawable = view.currentDrawable;

        if(pass == nil)
        {
            return;
        }

        if(drawable == nil)
        {
            return;
        }

        if(m_library == nil)
        {
            return;
        }

        size_t vertexBytes = mesh.vertices.size() * sizeof(RenderVertex);
        size_t indexBytes = mesh.indices.size() * sizeof(uint32_t);
        dispatch_semaphore_wait(m_frameSemaphore, DISPATCH_TIME_FOREVER);
        size_t frameIndex = m_frameIndex;
        m_frameIndex = (m_frameIndex + 1) % FrameBufferCount;
        ensureVertexCapacity(frameIndex, vertexBytes);
        ensureIndexCapacity(frameIndex, indexBytes);
        id<MTLBuffer> vertexBuffer = m_vertexBuffers[frameIndex];
        id<MTLBuffer> indexBuffer = m_indexBuffers[frameIndex];

        if(vertexBytes != 0)
        {
            std::memcpy(vertexBuffer.contents, mesh.vertices.data(), vertexBytes);
        }

        if(indexBytes != 0)
        {
            std::memcpy(indexBuffer.contents, mesh.indices.data(), indexBytes);
        }

        id<MTLCommandBuffer> commands = [m_commandQueue commandBuffer];

        if(commands == nil)
        {
            dispatch_semaphore_signal(m_frameSemaphore);

            return;
        }

        commands.label = @"Mosaic Frame";
        dispatch_semaphore_t frameSemaphore = m_frameSemaphore;
        [commands addCompletedHandler:^(id<MTLCommandBuffer>) {
          dispatch_semaphore_signal(frameSemaphore);
        }];
        id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];

        if(encoder == nil)
        {
            [commands commit];

            return;
        }

        encoder.label = @"Mosaic UI";

        float viewportSize[2] = {std::max(1.f, viewport.bounds.width), std::max(1.f, viewport.bounds.height)};
        [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
        [encoder setVertexBytes:viewportSize length:sizeof(viewportSize) atIndex:1];

        bool hasPipeline = false;
        BlendMode previousBlend = BlendMode::PremultipliedAlpha;
        id<MTLRenderPipelineState> renderPipeline = nil;
        bool hasScissor = false;
        MTLScissorRect previousScissor = {};
        TextureHandle previousTexture = std::numeric_limits<TextureHandle>::max();
        for(const RenderBatch & batch : mesh.batches)
        {
            const RenderState * state = batch.renderKey == 0 || batch.renderKey > mesh.renderStates.size() ? nullptr : &mesh.renderStates[static_cast<size_t>(batch.renderKey - 1)];
            BlendMode blend = state == nullptr ? BlendMode::PremultipliedAlpha : state->blend;
            bool pipelineChanged = hasPipeline == false;

            if(blend != previousBlend)
            {
                pipelineChanged = true;
            }

            if(pipelineChanged == true)
            {
                renderPipeline = pipeline(blend);

                if(renderPipeline != nil)
                {
                    [encoder setRenderPipelineState:renderPipeline];
                }

                previousBlend = blend;
                hasPipeline = true;
            }

            if(renderPipeline == nil)
            {
                continue;
            }

            Rect clip = state == nullptr || state->clip.empty() == true ? viewport.bounds : state->clip;
            MTLScissorRect scissor = Detail::scissorRect(clip, viewport, drawable.texture);

            if(scissor.width == 0)
            {
                continue;
            }

            if(scissor.height == 0)
            {
                continue;
            }

            bool scissorChanged = hasScissor == false;

            if(scissor.x != previousScissor.x)
            {
                scissorChanged = true;
            }

            if(scissor.y != previousScissor.y)
            {
                scissorChanged = true;
            }

            if(scissor.width != previousScissor.width)
            {
                scissorChanged = true;
            }

            if(scissor.height != previousScissor.height)
            {
                scissorChanged = true;
            }

            if(scissorChanged == true)
            {
                [encoder setScissorRect:scissor];
                previousScissor = scissor;
                hasScissor = true;
            }

            TextureHandle textureHandle = state == nullptr ? 0 : state->texture;

            if(textureHandle != previousTexture)
            {
                NSNumber * key = @(textureHandle);
                uint32_t maskTexture = [m_maskTextures containsObject:key] ? 1U : 0U;
                [encoder setFragmentTexture:texture(textureHandle) atIndex:0];
                [encoder setFragmentBytes:&maskTexture length:sizeof(maskTexture) atIndex:0];
                previousTexture = textureHandle;
            }

            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:batch.indexCount indexType:MTLIndexTypeUInt32 indexBuffer:indexBuffer indexBufferOffset:static_cast<NSUInteger>(batch.indexOffset) * sizeof(uint32_t) instanceCount:1 baseVertex:batch.vertexOffset baseInstance:0];
        }

        [encoder endEncoding];
        [commands presentDrawable:drawable];
        [commands commit];
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
