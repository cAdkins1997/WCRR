#pragma once
#include "resourcehelpers.h"
#include "../device/device.h"
#include "../glmdefines.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>

namespace vulkan  {
    enum MaterialType {
        transparent, opaque, invalid
    };

    class Device;

    struct Material {
        glm::vec4 baseColorFactor{};
        f32 metalnessFactor{}, roughnessFactor{}, emissiveStrength{};

        TextureHandle baseColorTexture{};
        TextureHandle mrTexture{};
        TextureHandle normalTexture{};
        TextureHandle occlusionTexture{};
        TextureHandle emissiveTexture{};
        u32 bufferOffset{};
        u16 magicNumber{};
    };

    struct GPUMaterial {
        glm::vec4 baseColorFactor{};
        f32 metalnessFactor{}, roughnessFactor{}, emissiveStrength{};

        u32 baseColorTexture{};
        u32 mrTexture{};
        u32 normalTexture{};
        u32 occlusionTexture{};
        u32 emissiveTexture{};
    };

    class MaterialManager {
        public:
        explicit MaterialManager(Device& _device, u64 initialCount);

        MaterialHandle create_material(const fastgltf::Material& material);
        std::vector<MaterialHandle> create_materials(const fastgltf::Asset& asset);

        void build_material_buffer(u64 size);
        void update_material_buffer();

        Material& get_material(MaterialHandle handle);

        Buffer get_material_buffer() const { return materialBuffer; };
        u32 get_material_buffer_size() const { return materialBufferSize; };
        vk::DeviceAddress get_material_buffer_address() const { return materialBufferAddress; }

        MaterialType get_material_type(MaterialHandle handle) const;

    private:
        Device& device;
        u32 materialCount = 0;
        std::vector<Material> materials;
        std::vector<MaterialHandle> transparentMaterials;
        std::vector<MaterialHandle> opaqueMaterials;

        Buffer materialBuffer;
        vk::DeviceAddress materialBufferAddress;
        u32 materialBufferSize = 0;

        void assert_handle(MaterialHandle handle) const;
    };
}
