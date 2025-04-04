
#include "descriptors.h"

namespace vulkan {

    DescriptorBuilder::DescriptorBuilder(vulkan::Device &device) : _device(device) {}

    vk::DescriptorSet DescriptorBuilder::build(vk::DescriptorSetLayout &layout) {
        vk::DescriptorPoolCreateInfo poolCI;
        poolCI.setPoolSizes(poolSizes);
        poolCI.setMaxSets(1);
        poolCI.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);
        auto pool = _device.get_handle().createDescriptorPool(poolCI);

        /*_device.deviceDeletionQueue.push_lambda([&]() {
            _device.get_handle().destroyDescriptorPool(pool);
        });*/

        auto bindings = {
                vk::DescriptorSetLayoutBinding()
                        .setBinding(uniformBinding)
                        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                        .setDescriptorCount(uniformCount)
                        .setStageFlags(vk::ShaderStageFlagBits::eAll),
                vk::DescriptorSetLayoutBinding()
                        .setBinding(textureBinding)
                        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                        .setDescriptorCount(textureCount)
                        .setStageFlags(vk::ShaderStageFlagBits::eAll),
        };

        vk::DescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingsFlags;
        std::vector bindingFlags = {
                vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
                vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        };

        setLayoutBindingsFlags.setBindingFlags(bindingFlags);

        layout = _device.get_handle().createDescriptorSetLayout(
                vk::DescriptorSetLayoutCreateInfo()
                        .setBindings(bindings)
                        .setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
                        .setPNext(&setLayoutBindingsFlags)
        );

        auto sets = {layout};
        const auto descriptorSet = _device.get_handle().allocateDescriptorSets(vk::DescriptorSetAllocateInfo()
                                                                                       .setDescriptorPool(pool)
                                                                                       .setDescriptorSetCount(1)
                                                                                       .setSetLayouts(sets)
        )[0];

        return descriptorSet;
    }

    void DescriptorBuilder::write_buffer(vk::Buffer buffer, u64 size, u64 offset, vk::DescriptorType type) {
        auto &bufferInfo = bufferInfos.emplace_back(buffer, offset, size);

        vk::WriteDescriptorSet write(VK_NULL_HANDLE, uniformBinding, {}, 1);;
        write.descriptorType = type;
        write.pBufferInfo = &bufferInfo;
        writes.push_back(write);
    }

    void DescriptorBuilder::write_image(const u32 dstArrayElement, vk::ImageView image, vk::Sampler sampler,
                                        vk::ImageLayout layout, const vk::DescriptorType type) {
        auto &imageInfo = imageInfos.emplace_back(sampler, image, layout);

        vk::WriteDescriptorSet write(VK_NULL_HANDLE, textureBinding, {}, 1);
        write.descriptorType = type;
        write.pImageInfo = &imageInfo;
        write.dstArrayElement = dstArrayElement;

        writes.push_back(write);
    }

    void DescriptorBuilder::update_set(const vk::DescriptorSet &set) {
        for (auto &write: writes) {
            write.dstSet = set;
        }

        const u32 writeCount = static_cast<u32>(writes.size());

        _device.get_handle().updateDescriptorSets(writeCount, writes.data(), 0, nullptr);
    }
}