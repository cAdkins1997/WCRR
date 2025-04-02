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

    class CommandBuffer {
    public:
        bool bound = false;

        [[nodiscard]] vk::CommandBuffer get_handle() const { return handle; }

        vk::CommandBuffer handle;
    };


    struct Buffer {
        vk::Buffer handle;
        vma::Allocation allocation;
        vma::AllocationInfo info;
        vk::MemoryPropertyFlags properties;

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
        eastl::string path;
        bool isCompute = false;
    };

    class Image {
    public:
        vk::Image handle{};
        vk::ImageView view{};
        vma::Allocation allocation{};
        vk::Extent3D extent{};
        vk::Format format{};
        SamplerHandle sampler{};
        u32 mipLevels;
        u16 magicNumber;

        [[nodiscard]] vma::Allocation get_allocation() const { return allocation; }
    };
}