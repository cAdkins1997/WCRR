#include "commands.h"

namespace vulkan {
    ComputeContext::ComputeContext(vk::CommandBuffer& commandBuffer) : _commandBuffer(commandBuffer) {}

    void ComputeContext::begin() const {
        _commandBuffer.reset();
        vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        vk_check(_commandBuffer.begin(&beginInfo), "Failed to begin command buffer");
    }

    void ComputeContext::end() const {
        _commandBuffer.end();
    }

    void ComputeContext::image_barrier(vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout) const {
        vk::ImageMemoryBarrier2 imageBarrier(
        vk::PipelineStageFlagBits2::eAllCommands,vk::AccessFlagBits2::eMemoryWrite,
        vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead
        );


        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        vk::ImageAspectFlags aspectMask = newLayout == vk::ImageLayout::eDepthAttachmentOptimal ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
        imageBarrier.subresourceRange = vk::ImageSubresourceRange(
            aspectMask,
            0,
            vk::RemainingMipLevels,
            0,
            vk::RemainingArrayLayers
            );
        imageBarrier.image = image;

        vk::DependencyInfo dependencyInfo;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageBarrier;

        _commandBuffer.pipelineBarrier2(&dependencyInfo);
    }

    void ComputeContext::copy_image(vk::Image src, vk::Image dst, vk::Extent3D srcSize, vk::Extent3D dstSize) const {
        vk::ImageBlit2 blitRegion;

        blitRegion.srcOffsets[1].x = srcSize.width;
        blitRegion.srcOffsets[1].y = srcSize.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = dstSize.width;
        blitRegion.dstOffsets[1].y = dstSize.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        vk::BlitImageInfo2 blitInfo(
            src,
            vk::ImageLayout::eTransferSrcOptimal,
            dst,
            vk::ImageLayout::eTransferDstOptimal,
            1,
            &blitRegion,
            vk::Filter::eLinear
            );


        _commandBuffer.blitImage2(&blitInfo);
    }

    void ComputeContext::bind_pipeline(const vulkan::Pipeline &pipeline) {
        _pipeline = pipeline;

        _commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline);
        _commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, 1, &pipeline.set, 0, nullptr);
    }

    void ComputeContext::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const {
        _commandBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
    }

    UploadContext::UploadContext(
        const vk::Device &device,
        vk::CommandBuffer &commandBuffer,
        vma::Allocator &allocator) :
    _allocator(allocator) , _commandBuffer(commandBuffer), _device(device) {}

    void UploadContext::begin() const {
        _commandBuffer.reset();
        vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        vk_check(_commandBuffer.begin(&beginInfo), "Failed to begin command buffer");
    }

    void UploadContext::end() const {
        _commandBuffer.end();
    }

    void UploadContext::image_barrier(
        vk::Image image,
        vk::ImageLayout currentLayout,
        vk::ImageLayout newLayout
        ) const
    {
        vk::ImageMemoryBarrier2 imageBarrier(
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead
            );

        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        vk::ImageAspectFlags aspectMask = (newLayout == vk::ImageLayout::eAttachmentOptimal) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
        imageBarrier.subresourceRange = vk::ImageSubresourceRange(
            aspectMask,
            0,
            vk::RemainingMipLevels,
            0,
            vk::RemainingArrayLayers
            );
        imageBarrier.image = image;

        vk::DependencyInfo dependencyInfo;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageBarrier;
        _commandBuffer.pipelineBarrier2(&dependencyInfo);
    }

    void UploadContext::buffer_barrier(
        const Buffer &buffer,
        vk::DeviceSize offset,
        vk::PipelineStageFlags srcStageFlags,
        vk::AccessFlags srcAccessMask,
        vk::PipelineStageFlags dstStageFlags,
        vk::AccessFlags dstAccessMask) const
    {
        vk::BufferMemoryBarrier barrier(srcAccessMask, dstAccessMask);
        barrier.buffer = buffer.handle;
        barrier.offset = offset;
        barrier.size = vk::WholeSize;
        barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;

        _commandBuffer.pipelineBarrier(
            srcStageFlags,
            dstStageFlags,
            {},
            0,
            nullptr,
            1,
            &barrier,
            0,
            nullptr
            );
    }

    void UploadContext::copy_buffer(
        const Buffer& bufferSrc,
        const Buffer& bufferDst,
        vk::DeviceSize srcOffset,
        vk::DeviceSize dstOffset,
        vk::DeviceSize dataSize) const
    {
        vk::BufferCopy bufferCopy;
        bufferCopy.dstOffset = dstOffset;
        bufferCopy.srcOffset = srcOffset;
        bufferCopy.size = dataSize;
        _commandBuffer.copyBuffer(bufferSrc.handle, bufferDst.handle, 1, &bufferCopy);
    }

    void UploadContext::copy_buffer_to_image(
        const Buffer& buffer,
        const Image& image,
        vk::ImageLayout layout,
        const std::vector<vk::BufferImageCopy>& regions) const
    {
        _commandBuffer.copyBufferToImage(buffer.handle, image.handle, layout, regions.size(), regions.data());
    }

    void UploadContext::upload_image(void *data, const Image &image) const {
        const auto extent = image.extent;
        const u64 size = extent.width * extent.height * extent.depth * 4;

        const Buffer stagingBuffer = make_staging_buffer(size);

        image_barrier(image.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vk::BufferImageCopy copyRegion;
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = extent;

        _commandBuffer.copyBufferToImage(stagingBuffer.handle, image.handle, vk::ImageLayout::eTransferDstOptimal, copyRegion);

        image_barrier(image.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    }

    void UploadContext::upload_uniform(void *data, u64 dataSize, Buffer &uniform) const {
        auto propertyFlags = _allocator.getAllocationMemoryProperties(uniform.allocation);
        if (propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) {
            memcpy(uniform.get_mapped_data(), data, dataSize);
            _allocator.flushAllocation(uniform.allocation, 0, vk::WholeSize),

            buffer_barrier(
                uniform,
                0,
                vk::PipelineStageFlagBits::eHost,
                vk::AccessFlagBits::eHostWrite,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferRead
            );
        }
        else {
            Buffer stagingBuffer = make_staging_buffer(dataSize);
            memcpy(stagingBuffer.get_mapped_data(), data, dataSize);
            _allocator.flushAllocation(uniform.allocation, 0, vk::WholeSize);

            buffer_barrier(
                stagingBuffer,
                0,
                vk::PipelineStageFlagBits::eHost,
                vk::AccessFlagBits::eHostWrite,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferRead
            );

            copy_buffer(stagingBuffer, uniform , 0, 0, dataSize);

            buffer_barrier(
                uniform,
                0,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferWrite,
                vk::PipelineStageFlagBits::eVertexShader,
                vk::AccessFlagBits::eUniformRead
            );
        }
    }

    void UploadContext::update_uniform(void *data, size_t dataSize, Buffer &uniform) const {
        auto pBufferData = uniform.get_mapped_data();
        memcpy(pBufferData, data, dataSize);
    }

    Buffer UploadContext::make_staging_buffer(size_t allocSize) const {
        vk::BufferCreateInfo bufferInfo;
        bufferInfo.pNext = nullptr;
        bufferInfo.size = allocSize;
        bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

        vma::AllocationCreateFlags allocationFlags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped;
        vma::AllocationCreateInfo allocationCI;
        allocationCI.flags = allocationFlags;
        allocationCI.usage = vma::MemoryUsage::eAuto;
        Buffer newBuffer{};

        vk_check(
            _allocator.createBuffer(&bufferInfo, &allocationCI, &newBuffer.handle, &newBuffer.allocation, &newBuffer.info),
            "Failed to create staging buffer"
            );

        return newBuffer;
    }

    vulkan::Buffer UploadContext::create_device_buffer(size_t size, vk::BufferUsageFlags bufferUsage) const {
        vma::AllocationCreateInfo allocationCI;
        allocationCI.usage = vma::MemoryUsage::eGpuOnly;
        allocationCI.flags = vma::AllocationCreateFlagBits::eMapped;

        vk::BufferCreateInfo deviceBufferInfo;
        deviceBufferInfo.pNext = nullptr;
        deviceBufferInfo.size = size;
        deviceBufferInfo.usage = bufferUsage;

        Buffer deviceBuffer{};
        vk_check(
            _allocator.createBuffer(&deviceBufferInfo, &allocationCI, &deviceBuffer.handle, &deviceBuffer.allocation, &deviceBuffer.info),
            "Failed to create upload context device buffer"
            );

        return deviceBuffer;
    }

    void UploadContext::destroy_buffer(const Buffer &buffer) const {
        _device.destroyBuffer(buffer.handle);
    }

    void UploadContext::destroy_image(const Image &image) const {
        _device.destroyImageView(image.view);
        _allocator.destroyImage(image.handle, image.allocation);
    }

    GraphicsContext::GraphicsContext(vk::CommandBuffer& commandBuffer) : _commandBuffer(commandBuffer){}

    void GraphicsContext::begin() const {
        _commandBuffer.reset();
        vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        vk_check(_commandBuffer.begin(&beginInfo), "Failed to begin command buffer");
    }

    void GraphicsContext::end() const {
        _commandBuffer.end();
    }

    void GraphicsContext::memory_barrier(
        vk::PipelineStageFlags2 srcStageFlags, vk::AccessFlags2 srcAccessMask,
        vk::PipelineStageFlags2 dstStageFlags, vk::AccessFlags2 dstAccessMask) const {
        vk::MemoryBarrier2 memoryBarrier;
        memoryBarrier.srcStageMask = srcStageFlags;
        memoryBarrier.srcAccessMask = srcAccessMask;
        memoryBarrier.dstStageMask = dstStageFlags;
        memoryBarrier.dstAccessMask = dstAccessMask;

        vk::DependencyInfo dependencyInfo;
        dependencyInfo.memoryBarrierCount = 1;
        dependencyInfo.pMemoryBarriers = &memoryBarrier;
        _commandBuffer.pipelineBarrier2(&dependencyInfo);
    }

    void GraphicsContext::buffer_barrier(vulkan::Buffer buffer, vk::DeviceSize offset, vk::PipelineStageFlags2 srcStageFlags,
        vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageFlags, vk::AccessFlags2 dstAccessMask) const {
        vk::BufferMemoryBarrier2 bufferBarrier;
        bufferBarrier.srcStageMask = srcStageFlags;
        bufferBarrier.srcAccessMask = srcAccessMask;
        bufferBarrier.dstStageMask = dstStageFlags;
        bufferBarrier.dstAccessMask = dstAccessMask;

        bufferBarrier.buffer = buffer.handle;
        bufferBarrier.offset = offset;

        vk::DependencyInfo dependencyInfo;
        dependencyInfo.bufferMemoryBarrierCount = 1;
        dependencyInfo.pBufferMemoryBarriers = &bufferBarrier;
        _commandBuffer.pipelineBarrier2(&dependencyInfo);
    }

    void GraphicsContext::image_barrier(vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout) const {
        vk::ImageMemoryBarrier2 imageBarrier(
        vk::PipelineStageFlagBits2::eAllCommands,vk::AccessFlagBits2::eMemoryWrite,
        vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead
        );


        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        vk::ImageAspectFlags aspectMask = (newLayout == vk::ImageLayout::eDepthAttachmentOptimal ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor);
        imageBarrier.subresourceRange = vk::ImageSubresourceRange(
            aspectMask,
            0,
            vk::RemainingMipLevels,
            0,
            vk::RemainingArrayLayers
            );
        imageBarrier.image = image;

        vk::DependencyInfo dependencyInfo;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageBarrier;
        _commandBuffer.pipelineBarrier2(&dependencyInfo);
    }

    void GraphicsContext::copy_image(vk::Image src, vk::Image dst, vk::Extent3D srcSize, vk::Extent3D dstSize) const {
        vk::ImageBlit2 blitRegion;

        blitRegion.srcOffsets[1].x = srcSize.width;
        blitRegion.srcOffsets[1].y = srcSize.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = dstSize.width;
        blitRegion.dstOffsets[1].y = dstSize.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        vk::BlitImageInfo2 blitInfo(
            src,
            vk::ImageLayout::eTransferSrcOptimal,
            dst,
            vk::ImageLayout::eTransferDstOptimal,
            1,
            &blitRegion,
            vk::Filter::eLinear
            );

        _commandBuffer.blitImage2(&blitInfo);
    }

    void GraphicsContext::set_up_render_pass(
        vk::Extent2D extent,
        const vk::RenderingAttachmentInfo *drawImage,
        const vk::RenderingAttachmentInfo *depthImage
        ) const {
        vk::Rect2D renderArea;
        renderArea.extent = extent;
        vk::RenderingInfo renderInfo;
        renderInfo.renderArea = renderArea;
        renderInfo.pColorAttachments = drawImage;
        renderInfo.pDepthAttachment = depthImage;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        _commandBuffer.beginRendering(&renderInfo);
    }

    void GraphicsContext::end_render_pass() const {
        _commandBuffer.endRendering();
    }

    void GraphicsContext::set_viewport(float x, float y, float minDepth, float maxDepth) const {
        vk::Viewport viewport(0, 0, x, y);
        viewport.minDepth = minDepth;
        viewport.maxDepth = maxDepth;
        _commandBuffer.setViewport(0, 1, &viewport);
    }

    void GraphicsContext::set_viewport(vk::Extent2D extent, float minDepth, float maxDepth) const {
        vk::Viewport viewport(0, 0, extent.width, extent.height);
        viewport.minDepth = minDepth;
        viewport.maxDepth = maxDepth;
        _commandBuffer.setViewport(0, 1, &viewport);
    }

    void GraphicsContext::set_scissor(uint32_t x, uint32_t y) const {
        vk::Rect2D scissor;
        vk::Extent2D extent {x, y};
        scissor.extent = extent;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        _commandBuffer.setScissor(0, 1, &scissor);
    }

    void GraphicsContext::set_scissor(vk::Extent2D extent) const {
        vk::Rect2D scissor;
        scissor.extent = extent;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        _commandBuffer.setScissor(0, 1, &scissor);
    }

    void GraphicsContext::bind_pipeline(const vulkan::Pipeline &pipeline) {
        _pipeline = pipeline;
        _commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline);
        _commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, 1, &pipeline.set, 0, nullptr);
    }

    void GraphicsContext::bind_index_buffer(const vulkan::Buffer &indexBuffer) const {
        _commandBuffer.bindIndexBuffer(indexBuffer.handle, 0, vk::IndexType::eUint32);
    }

    void GraphicsContext::bind_vertex_buffer(const vulkan::Buffer &vertexBuffer) const {
        vk::DeviceSize offsets[] = {0};
        _commandBuffer.bindVertexBuffers(0, vertexBuffer.handle, offsets);
    }

    void GraphicsContext::set_push_constants(const void *pPushConstants, const u64 size, const vk::ShaderStageFlags shaderStage) const {
        _commandBuffer.pushConstants(_pipeline.pipelineLayout, shaderStage, 0, size, pPushConstants);
    }

    void GraphicsContext::draw(uint32_t count, uint32_t startIndex) const {
        _commandBuffer.drawIndexed(count, 1, startIndex, 0, 0);
    }
}
