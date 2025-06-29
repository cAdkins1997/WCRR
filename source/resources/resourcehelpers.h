#pragma once

#include "../common.h"
#include "../rendererresources.h"
#include "../glmdefines.h"

namespace renderer {
    template<typename T>
    u32 get_handle_index(const T handle) {
        constexpr u32 indexMask = (1 << 16) - 1;
        const auto handleAsU32 = static_cast<u32>(handle);
        return handleAsU32 & indexMask;
    }

    template<typename T>
    u32 get_handle_metadata(const T handle) {
        constexpr u32 indexMask = (1 << 16) - 1;
        constexpr u32 metaDataMask = ~indexMask;
        const auto handleAsU32 = static_cast<u32>(handle);
        return (handleAsU32 & metaDataMask) >> 16;
    }

    [[nodiscard]] Buffer make_staging_buffer(u64 allocSize, VmaAllocator allocator);
    [[nodiscard]] Buffer create_device_buffer(u64 size, VkBufferUsageFlags bufferUsage, VmaAllocator allocator);
    void copy_buffer(vk::CommandBuffer cmd, const Buffer &bufferSrc, const Buffer &bufferDst, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset, vk::DeviceSize dataSize);
    void buffer_barrier(
    vk::CommandBuffer cmd,
    const Buffer &buffer,
    vk::DeviceSize offset,
    vk::PipelineStageFlags srcStageFlags,
    vk::AccessFlags srcAccessMask,
    vk::PipelineStageFlags dstStageFlags,
    vk::AccessFlags dstAccessMask
    );
}
