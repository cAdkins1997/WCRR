
#include "resourcehelpers.h"

namespace vulkan {
    Buffer make_staging_buffer(u64 allocSize, vma::Allocator allocator) {
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
            allocator.createBuffer(&bufferInfo, &allocationCI, &newBuffer.handle, &newBuffer.allocation, &newBuffer.info),
            "Failed to create staging buffer"
            );

        return newBuffer;
    }

    Buffer create_device_buffer(u64 size, vk::BufferUsageFlags bufferUsage, vma::Allocator allocator) {
        vma::AllocationCreateInfo allocationCI;
        allocationCI.usage = vma::MemoryUsage::eGpuOnly;
        allocationCI.flags = vma::AllocationCreateFlagBits::eMapped;

        vk::BufferCreateInfo deviceBufferInfo;
        deviceBufferInfo.pNext = nullptr;
        deviceBufferInfo.size = size;
        deviceBufferInfo.usage = bufferUsage;

        Buffer deviceBuffer{};
        vk_check(
            allocator.createBuffer(&deviceBufferInfo, &allocationCI, &deviceBuffer.handle, &deviceBuffer.allocation, &deviceBuffer.info),
            "Failed to create upload context device buffer"
            );

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
