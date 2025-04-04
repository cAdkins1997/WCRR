
#include "materialmanager.h"

namespace vulkan  {
    MaterialManager::MaterialManager(Device& _device, const u64 initialCount) : device(_device) {
        materials.reserve(initialCount);
    }

    MaterialHandle MaterialManager::create_material(const fastgltf::Material &material) {
        Material newMaterial;

        newMaterial.baseColorFactor = glm::vec4(
            material.pbrData.baseColorFactor.x(),
            material.pbrData.baseColorFactor.y(),
            material.pbrData.baseColorFactor.z(),
            material.pbrData.baseColorFactor.w()
            );

        if (material.pbrData.baseColorTexture.has_value()) {
            auto textureIndex = material.pbrData.baseColorTexture.value().textureIndex;
            newMaterial.baseColorTexture = static_cast<TextureHandle>(textureIndex);
        };

        if (material.normalTexture.has_value()) {
            auto textureIndex = material.normalTexture.value().textureIndex;
            newMaterial.normalTexture = static_cast<TextureHandle>(textureIndex);
        };

        if (material.occlusionTexture.has_value()) {
            auto textureIndex = material.occlusionTexture.value().textureIndex;
            newMaterial.occlusionTexture = static_cast<TextureHandle>(textureIndex);
        };

        if (material.pbrData.metallicRoughnessTexture.has_value()) {
            auto textureIndex = material.pbrData.metallicRoughnessTexture.value().textureIndex;
            newMaterial.mrTexture = static_cast<TextureHandle>(textureIndex);
        }

        newMaterial.metalnessFactor = material.pbrData.metallicFactor;
        newMaterial.roughnessFactor = material.pbrData.roughnessFactor;


        if (material.emissiveTexture.has_value()) {
            auto textureIndex = material.emissiveTexture.value().textureIndex;
            newMaterial.emissiveTexture = static_cast<TextureHandle>(textureIndex);
        }

        newMaterial.emissiveStrength = material.emissiveStrength;
        newMaterial.bufferOffset = materialCount * sizeof(GPUMaterial);

        materials.push_back(newMaterial);
        const auto handle = static_cast<MaterialHandle>(newMaterial.magicNumber << 16 | materialCount);

        if (newMaterial.baseColorFactor.a < 1.0f) {
            transparentMaterials.push_back(handle);
        }
        else {
            opaqueMaterials.push_back(handle);
        }

        materialCount++;
        return handle;
    }

    std::vector<MaterialHandle> MaterialManager::create_materials(const fastgltf::Asset& asset) {
        std::vector<MaterialHandle> materialHandles;
        materialHandles.reserve(asset.materials.size());

        materialBuffer = device.create_buffer(
            sizeof(Material) * asset.materials.size(),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_CPU_TO_GPU
            );

        for (const auto& material : asset.materials) {
            materialHandles.emplace_back(create_material(material));
        }

        return materialHandles;
    }

    void MaterialManager::build_material_buffer(const u64 size) {
        if (size > 0) {
            materialBuffer = device.create_buffer(
                sizeof(GPUMaterial) * size,
                vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                vk::BufferUsageFlagBits::eTransferDst,
                VMA_MEMORY_USAGE_CPU_TO_GPU
                );

            materialBufferSize = size * sizeof(GPUMaterial);

            vk::BufferDeviceAddressInfo bdaInfo(materialBuffer.handle);
            materialBufferAddress = device.get_handle().getBufferAddress(bdaInfo);
        }

        /*device.deviceDeletionQueue.push_lambda([&]() {
            vmaDestroyBuffer(device.get_allocator(), materialBuffer.handle, materialBuffer.allocation);
        });*/
    }

    void MaterialManager::update_material_buffer() {
        auto* materialData = static_cast<GPUMaterial*>(materialBuffer.get_mapped_data());
        for (u32 i = 0; i < materials.size(); i++) {
            GPUMaterial gpuMaterial;
            gpuMaterial.metalnessFactor = materials[i].metalnessFactor;
            gpuMaterial.roughnessFactor = materials[i].roughnessFactor;
            gpuMaterial.emissiveStrength = materials[i].emissiveStrength;
            gpuMaterial.baseColorTexture = get_handle_index(materials[i].baseColorTexture);
            gpuMaterial.mrTexture = get_handle_index(materials[i].mrTexture);
            gpuMaterial.normalTexture = get_handle_index(materials[i].normalTexture);
            gpuMaterial.emissiveTexture = get_handle_index(materials[i].emissiveTexture);
            gpuMaterial.occlusionTexture = get_handle_index(materials[i].occlusionTexture);

            materialData[i] = gpuMaterial;
        }
    }

    MaterialType MaterialManager::get_material_type(MaterialHandle handle) const {
        if (std::ranges::find(opaqueMaterials, handle) != opaqueMaterials.end()) {
            return opaque;
        };
        if (std::ranges::find(transparentMaterials, handle) != transparentMaterials.end()) {
            return transparent;
        }

        return opaque;
    }

    Material& MaterialManager::get_material(MaterialHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return materials[index];
    }

    void MaterialManager::assert_handle(const MaterialHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < materials.size());
        assert(materials[index].magicNumber == metaData);
    }
}
