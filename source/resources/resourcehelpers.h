#pragma once

#include "../common.h"
#include "../resources.h"
#include "../glmdefines.h"

namespace vulkan {

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

    template<typename T>
    concept isHandle = requires(T a)
    {
        std::is_enum<T>::type;
    };

    [[nodiscard]] Buffer make_staging_buffer(u64 allocSize, vma::Allocator allocator);
    [[nodiscard]] Buffer create_device_buffer(u64 size, vk::BufferUsageFlags bufferUsage, vma::Allocator allocator);
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
