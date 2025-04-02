
#include "descriptors.h"

namespace vulkan::descriptors {
    void DescriptorLayoutBuilder::add_binding(u32 binding, vk::DescriptorType type) {
        vk::DescriptorSetLayoutBinding newBind{};
        newBind.binding = binding;
        newBind.descriptorCount = 1;
        newBind.descriptorType = type;
        bindings.push_back(newBind);
    }

    void DescriptorLayoutBuilder::clear() {
        bindings.clear();
    }

    vk::DescriptorSetLayout DescriptorLayoutBuilder::build(
        const vk::Device device,
        vk::Flags<vk::ShaderStageFlagBits> stages,
        const void *next)
    {

        for (auto& binding : bindings) {
            binding.stageFlags |= stages;
        }

        vk::DescriptorSetLayoutCreateInfo layoutCI;
        layoutCI.pNext = next;
        layoutCI.bindingCount = static_cast<u32>(bindings.size());
        layoutCI.pBindings = bindings.data();
        layoutCI.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPoolEXT;

        vk::DescriptorSetLayout set;
        vk_check(
            device.createDescriptorSetLayout(&layoutCI, nullptr, &set),
            "Failed to create descriptor set layout"
            );

        return set;
    }

    void DescriptorAllocator::init(const vk::Device& device, u32 maxSets, std::span<PoolSizeRatio> poolRatios) {
        ratios.clear();

        for (auto& ratio : ratios) {
            ratios.push_back(ratio);
        }

        const vk::DescriptorPool newPool = create_pool(device, maxSets, poolRatios);

        setsPerPool = maxSets * 1.5f;

        readyPools.push_back(newPool);
    }

    void DescriptorAllocator::clear_pools(const vk::Device& device) {

        for (const auto& pool : readyPools) {
            device.resetDescriptorPool(pool, {});
        }

        for (const auto& pool : fullPools) {
            device.resetDescriptorPool(pool, {});
        }

        fullPools.clear();
    }

    void DescriptorAllocator::destroy_pools(const vk::Device& device) {
        for (const auto& pool : readyPools) {
            device.destroyDescriptorPool(pool, {});
        }
        readyPools.clear();

        for (const auto& pool : fullPools) {
            device.destroyDescriptorPool(pool, {});
        }

        fullPools.clear();

    }

    vk::DescriptorSet DescriptorAllocator::allocate(const vk::Device &device, vk::DescriptorSetLayout layout) {
        vk::DescriptorPool poolToUse = get_pool(device);

        vk::DescriptorSetAllocateInfo setAI(poolToUse, 1, &layout);

        vk::DescriptorSet set;
        auto result = device.allocateDescriptorSets(&setAI, &set);

        if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool) {
            fullPools.push_back(poolToUse);
            poolToUse = get_pool(device);
            setAI.descriptorPool = poolToUse;

            if (device.allocateDescriptorSets(&setAI, &set) != vk::Result::eSuccess) {

            }

            readyPools.push_back(poolToUse);
        }

        return set;
    }


    vk::DescriptorPool DescriptorAllocator::get_pool(const vk::Device& device) {
        vk::DescriptorPool newPool;
        if (!readyPools.empty()) {
            newPool = readyPools.back();
            readyPools.pop_back();
        }
        else {
            newPool = create_pool(device, setsPerPool, ratios);

            setsPerPool = setsPerPool * 1.5;
            if (setsPerPool > 4092) {
                setsPerPool = 4092;
            }
        }
        return newPool;
    }

    vk::DescriptorPool DescriptorAllocator::create_pool(const vk::Device& device, i32 setCount, std::span<PoolSizeRatio> poolSizeRatio) {

        std::vector<vk::DescriptorPoolSize> poolSizes;
        for (PoolSizeRatio& ratio : ratios) {
            poolSizes.emplace_back(ratio.type, static_cast<u32>(ratio.ratio * setCount));
        }

        vk::DescriptorPoolCreateInfo poolCI({}, setCount, static_cast<i32>(poolSizes.size()), poolSizes.data());
        poolCI.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
        poolCI.maxSets = poolSizeRatio.size();
        poolCI.poolSizeCount = static_cast<u32>(poolSizes.size());
        poolCI.pPoolSizes = poolSizes.data();

        vk::DescriptorPool newPool = device.createDescriptorPool(poolCI, nullptr);
        return newPool;
    }

    void DescriptorWriter::write_image(i32 binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type) {
        imageInfos.emplace_back(sampler, image, layout);
        vk::DescriptorImageInfo& imageInfo = imageInfos.front();

        vk::WriteDescriptorSet write(VK_NULL_HANDLE, binding, {}, 1);
        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE;
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pImageInfo = &imageInfo;

        writes.push_back(write);
    }


    void DescriptorWriter::write_buffer(i32 binding, vk::Buffer buffer, u64 size, u64 offset, vk::DescriptorType type) {
        bufferInfos.emplace_back(buffer, offset, size);
        vk::DescriptorBufferInfo& bufferInfo = bufferInfos.back();

        vk::WriteDescriptorSet write;
        write.sType = vk::StructureType::eWriteDescriptorSet;
        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE;
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pBufferInfo = &bufferInfo;
        writes.push_back(write);
    }

    void DescriptorWriter::clear() {

        imageInfos.clear();
        writes.clear();
        bufferInfos.clear();
    }


    void DescriptorWriter::update_set(vk::Device device, vk::DescriptorSet set) {
        for (vk::WriteDescriptorSet& write : writes) {
            write.dstSet = set;
        }

        const u32 writeCount = static_cast<u32>(writes.size());
        device.updateDescriptorSets(writeCount, writes.data(), 0, nullptr);
    }
}