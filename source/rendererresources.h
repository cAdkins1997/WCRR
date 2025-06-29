#pragma once
#include "common.h"
#include "glmdefines.h"


namespace renderer {
    enum QueueType { Graphics, Compute, Transfer };
    enum class SamplerHandle : u32 { Invalid = 0 };

    struct Buffer {
        VkBuffer handle;
        VmaAllocation allocation;
        VmaAllocationInfo info;
        vk::MemoryPropertyFlags properties;
        vk::DeviceAddress address;

        void* get_mapped_data() { return info.pMappedData; }
    };

    struct Pipeline {
        vk::Pipeline pipeline;
        vk::PipelineLayout pipelineLayout;
        vk::DescriptorSetLayout setLayout;
        vk::DescriptorSet set;
    };

    struct Shader {
        vk::ShaderModule module;
    };

    class Image {
    public:
        VkImage handle{};
        VkImageView view{};
        VmaAllocation allocation{};
        VkExtent3D extent{};
        VkFormat format{};
        SamplerHandle sampler{};
        u32 mipLevels{};
        u16 magicNumber{};

        [[nodiscard]] VmaAllocation get_allocation() const { return allocation; }
    };
}