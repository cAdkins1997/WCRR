
#include "resourcehelpers.h"

namespace vulkan {
    Buffer make_staging_buffer(u64 allocSize, VmaAllocator allocator) {
        VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.pNext = nullptr;
        bufferInfo.size = allocSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateFlags allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationCreateInfo allocationCI{};
        allocationCI.flags = allocationFlags;
        allocationCI.usage = VMA_MEMORY_USAGE_AUTO;
        Buffer newBuffer{};

        vmaCreateBuffer(
                allocator,
                &bufferInfo,
                &allocationCI,
                (VkBuffer*)&newBuffer.handle,
                &newBuffer.allocation,
                &newBuffer.info);
        return newBuffer;
    }

    Buffer create_device_buffer(u64 size, VkBufferUsageFlags bufferUsage, VmaAllocator allocator) {
        VmaAllocationCreateInfo allocationCI{};
        allocationCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocationCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBufferCreateInfo deviceBufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        deviceBufferInfo.pNext = nullptr;
        deviceBufferInfo.size = size;
        deviceBufferInfo.usage = bufferUsage;

        Buffer deviceBuffer{};
        vmaCreateBuffer(
                allocator,
                &deviceBufferInfo,
                &allocationCI,
                &deviceBuffer.handle,
                &deviceBuffer.allocation,
                &deviceBuffer.info);

        return deviceBuffer;
    }

    void copy_buffer(
        vk::CommandBuffer cmd,
        const Buffer &bufferSrc,
        const Buffer &bufferDst,
        vk::DeviceSize srcOffset,
        vk::DeviceSize dstOffset,
        vk::DeviceSize dataSize)
    {
        vk::BufferCopy bufferCopy;
        bufferCopy.dstOffset = dstOffset;
        bufferCopy.srcOffset = srcOffset;
        bufferCopy.size = dataSize;
        cmd.copyBuffer(bufferSrc.handle, bufferDst.handle, 1, &bufferCopy);
    }

    void buffer_barrier(
        const vk::CommandBuffer cmd,
        const Buffer &buffer,
        const vk::DeviceSize offset,
        const vk::PipelineStageFlags srcStageFlags,
        const vk::AccessFlags srcAccessMask,
        const vk::PipelineStageFlags dstStageFlags,
        const vk::AccessFlags dstAccessMask)
    {
        vk::BufferMemoryBarrier barrier(srcAccessMask, dstAccessMask);
        barrier.buffer = buffer.handle;
        barrier.offset = offset;
        barrier.size = vk::WholeSize;
        barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
        barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;

        cmd.pipelineBarrier(
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
}
