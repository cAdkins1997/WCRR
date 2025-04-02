#pragma once
#include "../device/device.h"
#include "../resources.h"
#include "../glmdefines.h"
#include "../pipelines/descriptors.h"

#include <fastgltf/types.hpp>
#include <ktx.h>

#include "../pipelines/descriptors2.h"

typedef ktx_uint64_t ku64;
typedef ktx_uint32_t ku32;
typedef ktx_uint16_t ku16;
typedef ktx_uint8_t ku8;

typedef ktx_int64_t ki64;
typedef ktx_int32_t ki32;
typedef ktx_int16_t ki16;

namespace vulkan {
    class Image;
    class Device;
    struct UploadContext;

    struct Sampler {
        vk::Filter magFilter;
        vk::Filter minFilter;
        vk::Sampler sampler;
        u16 magicNumber;
    };

    class TextureManager {
    public:
        TextureManager(Device& _device, UploadContext& _context, u64 initialCount);

        TextureHandle create_texture(
            vk::Format format,
            vk::ImageUsageFlags usage,
            fastgltf::Image& fastgltfImage,
            fastgltf::Asset& asset
            );

        std::vector<TextureHandle> create_textures(vk::Format format,
            vk::ImageUsageFlags usage,
            fastgltf::Asset& asset
            );

        SamplerHandle create_sampler(const fastgltf::Sampler& fastgltfSampler);

        Image& get_texture(TextureHandle handle);
        Sampler& get_sampler(SamplerHandle handle);

        void write_textures(descriptors::DescriptorWriter& writer, u32 binding);
        void write_textures(DescriptorBuilder& builder);
        void write_samplers(descriptors::DescriptorWriter& writer, u32 binding);

        void set_texture_sampler(TextureHandle textureHandle, SamplerHandle samplerHandle);

    private:
        Device& device;
        UploadContext& context;
        u32 textureCount = 0;
        u32 samplerCount = 0;
        std::vector<Image> textures;
        std::vector<Sampler> samplers;

        void image_barrier(vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout) const;

        void assert_handle(TextureHandle handle) const;
        void assert_handle(SamplerHandle handle) const;
    };

}
