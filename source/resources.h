#pragma once
#include "common.h"
#include "glmdefines.h"

enum QueueType { Graphics, Compute, Transfer };

namespace vulkan {

    enum class BufferHandle : u32 { Invalid = 0 };
    enum class SamplerHandle : u32 { Invalid = 0 };
    enum class TextureHandle : u32 { Invalid = 0 };
    enum class MaterialHandle : u32 { Invalid = 0 };
    enum class MeshHandle : u32 { Invalid = 0 };
    enum class VertexBufferHandle : u32 { Invalid = 0 };
    enum class SceneHandle : u32 { Invalid = 0 };
    enum class NodeHandle : u32 { Invalid = 0 };
    enum class LightHandle : u32 { Invalid = 0 };

    struct Buffer {
        VkBuffer handle;
        VmaAllocation allocation;
        VmaAllocationInfo info;
        vk::MemoryPropertyFlags properties;
        vk::DeviceAddress deviceAddress;

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
        std::string path;
        bool isCompute = false;
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