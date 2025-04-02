
#pragma once

#include <deque>

#include "../common.h"
#include "../glmdefines.h"

#include <EASTL/deque.h>
#include <EASTL/span.h>

namespace vulkan::descriptors {

    struct DescriptorLayoutBuilder {

        std::vector<vk::DescriptorSetLayoutBinding> bindings;

        void add_binding(u32 binding, vk::DescriptorType type);
        void clear();

        vk::DescriptorSetLayout build(
            vk::Device device,
            vk::Flags<vk::ShaderStageFlagBits> stages,
            const void *next = nullptr
        );
    };

    struct DescriptorAllocator {

        struct PoolSizeRatio {
            vk::DescriptorType type{};
            float ratio{};
        };

        void init(const vk::Device& device, u32 maxSets, std::span<PoolSizeRatio> poolRatios);
        void clear_pools(const vk::Device& device);
        void destroy_pools(const vk::Device& device);

        vk::DescriptorSet allocate(const vk::Device& device, vk::DescriptorSetLayout layout);

    private:
        vk::DescriptorPool get_pool(const vk::Device& device);
        vk::DescriptorPool create_pool(const vk::Device& device, i32 setCount, std::span<PoolSizeRatio> poolSizeRatio);

        std::vector<PoolSizeRatio> ratios{};
        std::vector<vk::DescriptorPool> fullPools{};
        std::vector<vk::DescriptorPool> readyPools{};
        u32 setsPerPool{};
    };

    struct DescriptorWriter {

        std::deque<vk::DescriptorImageInfo> imageInfos;
        std::deque<vk::DescriptorBufferInfo> bufferInfos;
        std::vector<vk::WriteDescriptorSet> writes;

        void write_image(i32 binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type);
        void write_buffer(i32 binding, vk::Buffer buffer, u64 size, u64 offset, vk::DescriptorType type);

        void clear();
        void update_set(vk::Device device, vk::DescriptorSet set);
    };

    struct DescriptorStructures {
        DescriptorLayoutBuilder builder;
        DescriptorAllocator allocator;
        DescriptorWriter writer;
    };
}