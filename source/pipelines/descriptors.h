#pragma once
#include <deque>

#include "../device/device.h"

namespace vulkan {

    class Device;

    class DescriptorBuilder {

    public:
        explicit DescriptorBuilder(Device &device);
        void release_descriptor_resources();

        vk::DescriptorSet build(vk::DescriptorSetLayout &layout);

        void write_buffer(vk::Buffer buffer, u64 size, u64 offset, vk::DescriptorType type);

        void write_image(u32 dstArrayElement, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout,
                         vk::DescriptorType type);

        void update_set(const vk::DescriptorSet &set);

    private:
        Device &_device;

        std::deque<vk::DescriptorBufferInfo> bufferInfos;
        std::deque<vk::DescriptorImageInfo> imageInfos;
        std::vector<vk::WriteDescriptorSet> writes;

        static constexpr u32 uniformBinding = 0;
        static constexpr u32 textureBinding = 1;
        static constexpr u32 uniformCount = 20;
        static constexpr u32 textureCount = 65536;

        std::vector<vk::DescriptorPoolSize> poolSizes = {
                {vk::DescriptorType::eUniformBuffer, uniformCount},
                {vk::DescriptorType::eCombinedImageSampler,  textureCount},
        };

        vk::DescriptorPool pool;
    };
}