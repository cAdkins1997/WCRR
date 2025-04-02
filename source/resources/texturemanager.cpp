
#include "texturemanager.h"

namespace vulkan {
    TextureManager::TextureManager(Device& _device, UploadContext& _context, const u64 initialCount = 0) : device(_device), context(_context) {
        textures.reserve(initialCount);
    }

    TextureHandle TextureManager::create_texture(
        vk::Format format,
        vk::ImageUsageFlags usage,
        fastgltf::Image& gltfImage,
        fastgltf::Asset& asset)
    {
        Image newImage;

        std::visit(fastgltf::visitor {
            [&](auto& arg) {},
                [&](fastgltf::sources::URI& path) {
                    std::string prepend = "../assets/";
                    prepend.append(path.uri.c_str());

                    ktxTexture* texture;

                    const ktxResult result = ktxTexture_CreateFromNamedFile(
                        prepend.c_str(),
                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                        &texture
                        );

                    if (result != KTX_SUCCESS)
                        throw std::runtime_error("Failed to load KTX texture" + prepend);

                    const ku32 width = texture->baseWidth;
                    const ku32 height = texture->baseHeight;
                    const ku32 mipLevels = texture->numLevels;

                    ku8* ktxTextureData = texture->pData;
                    ku64 textureSize = texture->dataSize;

                    const vk::Extent3D extent { width, height, 1 };

                    auto stagingBuffer = make_staging_buffer(textureSize * mipLevels, device.get_allocator());
                    void* stagingData = stagingBuffer.get_mapped_data();
                    vmaMapMemory(device.get_allocator(), stagingBuffer.allocation, &stagingData);
                    memcpy(stagingData, ktxTextureData, textureSize);
                    vmaUnmapMemory(device.get_allocator(), stagingBuffer.allocation);

                    std::vector<vk::BufferImageCopy> regions;
                    for (u32 i = 0; i < mipLevels; i++) {
                        ku64 offset;
                        if (ktxTexture_GetImageOffset(texture, i, 0, 0, &offset) == KTX_SUCCESS) {
                            vk::BufferImageCopy copyRegion;
                            copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                            copyRegion.imageSubresource.mipLevel = i;
                            copyRegion.imageSubresource.baseArrayLayer = 0;
                            copyRegion.imageSubresource.layerCount = 1;
                            copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                            copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                            copyRegion.imageExtent.depth = 1;
                            copyRegion.bufferOffset = offset;

                            regions.push_back(copyRegion);
                        }
                    }

                    newImage = device.create_image(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipLevels, true);
                    /*device.submit_immediate_work([&](vk::CommandBuffer cmd) {
                        UploadContext immediateContext(device.get_handle(), cmd, device.get_allocator());
                        image_barrier(newImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                        cmd.copyBufferToImage(stagingBuffer.handle, newImage.handle, vk::ImageLayout::eTransferDstOptimal, regions);
                        image_barrier(newImage.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
                    });

                    device.get_allocator().destroyBuffer(stagingBuffer.handle, stagingBuffer.allocation);*/

                    image_barrier(newImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                    device.immediateCommandBuffer.copyBufferToImage(stagingBuffer.handle, newImage.handle, vk::ImageLayout::eTransferDstOptimal, regions);
                    image_barrier(newImage.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            },

            [&](fastgltf::sources::Array& array) {

                ku8* ktxTextureData{};
                ku64 textureSize{};
                ktxTexture* texture;

                const ktxResult result = ktxTexture_CreateFromMemory(ktxTextureData, textureSize, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

                if (result != KTX_SUCCESS) {
                    throw std::runtime_error("Failed to load KTX texture");
                }

                const ku32 width = texture->baseWidth;
                const ku32 height = texture->baseHeight;
                const ku32 mipLevels = texture->numLevels;

                const vk::Extent3D extent{width, height, 1};

                auto stagingBuffer = make_staging_buffer(textureSize, device.get_allocator());
                void* stagingData = stagingBuffer.get_mapped_data();
                vmaMapMemory(device.get_allocator(), stagingBuffer.allocation, &stagingData);
                memcpy(ktxTextureData, stagingData, textureSize);
                vmaUnmapMemory(device.get_allocator(), stagingBuffer.allocation);

                std::vector<vk::BufferImageCopy> regions;
                    for (u32 i = 0; i < mipLevels; i++) {
                        ku64 offset;
                        if (ktxTexture_GetImageOffset(texture, i, 0, 0, &offset) == KTX_SUCCESS) {
                            vk::BufferImageCopy copyRegion;
                            copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                            copyRegion.imageSubresource.mipLevel = i;
                            copyRegion.imageSubresource.baseArrayLayer = 0;
                            copyRegion.imageSubresource.layerCount = 1;
                            copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                            copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                            copyRegion.imageExtent.depth = 1;
                            copyRegion.bufferOffset = offset;

                            regions.push_back(copyRegion);
                        }
                    }

                    newImage = device.create_image(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipLevels, true);
                    /*device.submit_immediate_work([&](vk::CommandBuffer cmd) {
                        UploadContext immediateContext(device.get_handle(), cmd, device.get_allocator());
                        image_barrier(newImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                        cmd.copyBufferToImage(stagingBuffer.handle, newImage.handle, vk::ImageLayout::eTransferDstOptimal, regions);
                        image_barrier(newImage.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
                    });

                    device.get_allocator().destroyBuffer(stagingBuffer.handle, stagingBuffer.allocation);*/

                    image_barrier(newImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                    device.immediateCommandBuffer.copyBufferToImage(stagingBuffer.handle, newImage.handle, vk::ImageLayout::eTransferDstOptimal, regions);
                    image_barrier(newImage.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            },

            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor {
                        [&](auto& arg) {},
                        [&](fastgltf::sources::Array& array) {
                            const auto ktxTextureBytes = reinterpret_cast<const ku8*>(array.bytes.data() + bufferView.byteOffset);
                            const auto byteLength = static_cast<i32>(bufferView.byteLength);

                            ktxTexture* texture;
                            const ktxResult result = ktxTexture_CreateFromMemory(
                                ktxTextureBytes,
                                byteLength,
                                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                &texture
                                );

                            if (result != KTX_SUCCESS) {
                                throw std::runtime_error("Failed to load KTX texture");
                            }

                            const ku32 width = texture->baseWidth;
                            const ku32 height = texture->baseHeight;
                            const ku32 mipLevels = texture->numLevels;

                            const vk::Extent3D extent{width, height, 1};

                            auto stagingBuffer = make_staging_buffer(byteLength, device.get_allocator());
                            void* stagingData = stagingBuffer.get_mapped_data();
                            vmaMapMemory(device.get_allocator(), stagingBuffer.allocation, &stagingData);
                            memcpy(texture, stagingData, byteLength);
                            vmaUnmapMemory(device.get_allocator(), stagingBuffer.allocation);

                            std::vector<vk::BufferImageCopy> regions;
                            for (u32 i = 0; i < mipLevels; i++) {
                                ku64 offset;
                                if (ktxTexture_GetImageOffset(texture, i, 0, 0, &offset) == KTX_SUCCESS) {
                                    vk::BufferImageCopy copyRegion;
                                    copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                                    copyRegion.imageSubresource.mipLevel = i;
                                    copyRegion.imageSubresource.baseArrayLayer = 0;
                                    copyRegion.imageSubresource.layerCount = 1;
                                    copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                                    copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                                    copyRegion.imageExtent.depth = 1;
                                    copyRegion.bufferOffset = offset;

                                    regions.push_back(copyRegion);
                                }
                            }

                            newImage = device.create_image(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipLevels, true);
                            /*device.submit_immediate_work([&](vk::CommandBuffer cmd) {
                                UploadContext immediateContext(device.get_handle(), cmd, device.get_allocator());
                                image_barrier(newImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                                cmd.copyBufferToImage(stagingBuffer.handle, newImage.handle, vk::ImageLayout::eTransferDstOptimal, regions);
                                image_barrier(newImage.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
                            });

                            device.get_allocator().destroyBuffer(stagingBuffer.handle, stagingBuffer.allocation);*/

                            image_barrier(newImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                            device.immediateCommandBuffer.copyBufferToImage(stagingBuffer.handle, newImage.handle, vk::ImageLayout::eTransferDstOptimal, regions);
                            image_barrier(newImage.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
                        }
                    }, buffer.data);
            }
        }, gltfImage.data);
        newImage.magicNumber = textureCount;


        textures.push_back(newImage);

        const auto handle = static_cast<TextureHandle>(newImage.magicNumber << 16 | textureCount);
        textureCount++;
        return handle;
    }

    std::vector<TextureHandle> TextureManager::create_textures(
        const vk::Format format,
        const vk::ImageUsageFlags usage,
        fastgltf::Asset &asset)
    {
        std::vector<TextureHandle> handles;
        std::vector<Image> images;
        std::vector<std::vector<vk::BufferImageCopy>> copyRegions;
        std::vector<ktxTexture*> ktxTexturePs;
        u64 stagingBufferSize = 0;


        const u64 numImages = asset.images.size();
        handles.reserve(numImages);
        images.reserve(numImages);

        u32 index = 0;
        for (auto& gltfImage : asset.images) {
            Image newImage;
            std::vector<vk::BufferImageCopy> newRegions;
            std::visit(fastgltf::visitor {
            [&](auto& arg) {},
                [&](fastgltf::sources::URI& path) {
                    std::string prepend = "../assets/";
                    prepend.append(path.uri.c_str());

                    ktxTexture* texture;

                    const ktxResult result = ktxTexture_CreateFromNamedFile(
                        prepend.c_str(),
                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                        &texture
                        );

                    if (result != KTX_SUCCESS)
                        throw std::runtime_error("Failed to load KTX texture " + prepend);

                    ktxTexturePs.push_back(texture);

                    const ku32 width = texture->baseWidth;
                    const ku32 height = texture->baseHeight;
                    const ku32 mipLevels = texture->numLevels;

                    const vk::Extent3D extent { width, height, 1 };
                    newImage = device.create_image(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipLevels, true);
                    images.push_back(newImage);

                    for (u32 i = 0; i < mipLevels; i++) {
                        ku64 imageOffset = stagingBufferSize;
                        ku64 mipOffset;
                        if (ktxTexture_GetImageOffset(texture, i, 0, 0, &mipOffset) == KTX_SUCCESS) {
                            vk::BufferImageCopy copyRegion;
                            ku64 offset = imageOffset + mipOffset;
                            copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                            copyRegion.imageSubresource.mipLevel = i;
                            copyRegion.imageSubresource.baseArrayLayer = 0;
                            copyRegion.imageSubresource.layerCount = 1;
                            copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                            copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                            copyRegion.imageExtent.depth = 1;
                            copyRegion.bufferOffset = offset;

                            newRegions.push_back(copyRegion);
                        }
                    }

                    stagingBufferSize += texture->dataSize;
            },

            [&](fastgltf::sources::Array& array) {

                ku8* ktxTextureData{};
                ku64 textureSize{};
                ktxTexture* texture;

                const ktxResult result = ktxTexture_CreateFromMemory(ktxTextureData, textureSize, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

                if (result != KTX_SUCCESS) {
                    throw std::runtime_error("Failed to load KTX texture");
                }

                const ku32 width = texture->baseWidth;
                const ku32 height = texture->baseHeight;
                const ku32 mipLevels = texture->numLevels;

                const vk::Extent3D extent {width, height, 1};
                newImage = device.create_image(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipLevels, true);
                images.push_back(newImage);

                stagingBufferSize += textureSize;

                for (u32 i = 0; i < mipLevels; i++) {
                    ku64 offset;
                    if (ktxTexture_GetImageOffset(texture, i, 0, 0, &offset) == KTX_SUCCESS) {
                        vk::BufferImageCopy copyRegion;
                        copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                        copyRegion.imageSubresource.mipLevel = i;
                        copyRegion.imageSubresource.baseArrayLayer = 0;
                        copyRegion.imageSubresource.layerCount = 1;
                        copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                        copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                        copyRegion.imageExtent.depth = 1;
                        copyRegion.bufferOffset = offset;

                        newRegions.push_back(copyRegion);
                    }
                }
            },

            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor {
                        [&](auto& arg) {},
                        [&](fastgltf::sources::Array& array) {
                            const auto ktxTextureBytes = reinterpret_cast<const ku8*>(array.bytes.data() + bufferView.byteOffset);
                            const auto byteLength = static_cast<i32>(bufferView.byteLength);

                            ktxTexture* texture;
                            const ktxResult result = ktxTexture_CreateFromMemory(
                                ktxTextureBytes,
                                byteLength,
                                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                &texture
                                );

                            if (result != KTX_SUCCESS) {
                                throw std::runtime_error("Failed to load KTX texture");
                            }

                            const ku32 width = texture->baseWidth;
                            const ku32 height = texture->baseHeight;
                            const ku32 mipLevels = texture->numLevels;
                            const ku32 textureSize = texture->dataSize;

                            const vk::Extent3D extent {width, height, 1};
                            newImage = device.create_image(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipLevels, true);
                            images.push_back(newImage);

                            const u64 dataSize = texture->dataSize;
                            const ku8* textureData = texture->pData;
                            stagingBufferSize += textureSize;

                            for (u32 i = 0; i < mipLevels; i++) {
                                ku64 offset;
                                if (ktxTexture_GetImageOffset(texture, i, 0, 0, &offset) == KTX_SUCCESS) {
                                    vk::BufferImageCopy copyRegion;
                                    copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                                    copyRegion.imageSubresource.mipLevel = i;
                                    copyRegion.imageSubresource.baseArrayLayer = 0;
                                    copyRegion.imageSubresource.layerCount = 1;
                                    copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                                    copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                                    copyRegion.imageExtent.depth = 1;
                                    copyRegion.bufferOffset = offset;

                                    newRegions.push_back(copyRegion);
                                }
                            }
                        }
                    }, buffer.data);
            }
        }, gltfImage.data);

            index++;
            newImage.magicNumber = textureCount;
            const auto handle = static_cast<TextureHandle>(newImage.magicNumber << 16 | textureCount);
            textureCount++;

            handles.push_back(handle);
            textures.push_back(newImage);
            copyRegions.push_back(newRegions);
        }

        Buffer stagingBuffer = make_staging_buffer(stagingBufferSize, device.get_allocator());
        void* stagingData = stagingBuffer.get_mapped_data();
        std::vector<ku8> data;
        data.reserve(stagingBufferSize);

        for (const auto& ktxTextureP : ktxTexturePs) {
            const ku64 size = ktxTextureP->dataSize;
            const ku8* textureData = ktxTextureP->pData;
            for (u64 i = 0; i < size; i++) {
                data.push_back(textureData[i]);
            }
        }

        vmaMapMemory(device.get_allocator(), stagingBuffer.allocation, &stagingData);
        memcpy(stagingData, data.data(), stagingBufferSize);
        vmaUnmapMemory(device.get_allocator(), stagingBuffer.allocation);

        for (u32 i = 0; i < ktxTexturePs.size(); i++) {
            image_barrier(images[i].handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            device.immediateCommandBuffer.copyBufferToImage(stagingBuffer.handle, images[i].handle, vk::ImageLayout::eTransferDstOptimal, copyRegions[i]);
            image_barrier(images[i].handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        //device.get_handle().destroyBuffer(stagingBuffer.handle);

        return handles;
    }

    inline vk::Filter convert_filter(const fastgltf::Filter filter) {
        switch (filter) {
            case fastgltf::Filter::Nearest:
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::NearestMipMapLinear:
                return vk::Filter::eNearest;

            case fastgltf::Filter::Linear:
            case fastgltf::Filter::LinearMipMapNearest:
            case fastgltf::Filter::LinearMipMapLinear:
            default:
                return vk::Filter::eLinear;
        }
    }

    inline vk::SamplerMipmapMode convert_mipmap_mode(const fastgltf::Filter filter) {
        switch (filter) {
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::LinearMipMapNearest:
                return vk::SamplerMipmapMode::eNearest;

            case fastgltf::Filter::NearestMipMapLinear:
            case fastgltf::Filter::LinearMipMapLinear:
            default:
                return vk::SamplerMipmapMode::eLinear;
        }
    }

    SamplerHandle TextureManager::create_sampler(const fastgltf::Sampler& fastgltfSampler) {
        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.magFilter = convert_filter(fastgltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = convert_filter(fastgltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.maxLod = vk::LodClampNone;
        samplerInfo.minLod = 0;
        samplerInfo.mipmapMode = convert_mipmap_mode(fastgltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));

        auto magFilter = convert_filter(fastgltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        const auto minFilter = convert_filter(fastgltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
        const auto mipMapMode = convert_mipmap_mode(fastgltfSampler.minFilter.value_or(magFilter));

        Sampler newSampler = device.create_sampler(magFilter, minFilter, mipMapMode);
        samplers.push_back(newSampler);

        const auto handle = static_cast<SamplerHandle>(newSampler.magicNumber << 16 | textureCount);
        samplerCount++;

        return handle;
    }

    void TextureManager::set_texture_sampler(TextureHandle textureHandle, SamplerHandle samplerHandle) {
        auto texture = get_texture(textureHandle);
        texture.sampler = samplerHandle;
    }

    Image& TextureManager::get_texture(const TextureHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return textures[index];
    }

    Sampler& TextureManager::get_sampler(const SamplerHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        Sampler& sampler = samplers[index];

        return samplers[index];
    }

    void TextureManager::write_textures(descriptors::DescriptorWriter& writer, u32 binding) {
        for (const auto& texture : textures) {
            const auto sampler = get_sampler(texture.sampler).sampler;
            writer.write_image(binding, texture.view, sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
        }
    }

    void TextureManager::write_textures(DescriptorBuilder& builder) {
        u32 i = 0;
        for (const auto& texture : textures) {
            const auto sampler = get_sampler(texture.sampler).sampler;
            builder.write_image(i, texture.view, sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
            i++;
        }
    }


    void TextureManager::image_barrier(
        vk::Image image,
        vk::ImageLayout currentLayout,
        vk::ImageLayout newLayout) const
    {
        vk::ImageMemoryBarrier2 imageBarrier(
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead
            );

        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        vk::ImageAspectFlags aspectMask = (newLayout == vk::ImageLayout::eAttachmentOptimal) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
        imageBarrier.subresourceRange = vk::ImageSubresourceRange(
            aspectMask,
            0,
            vk::RemainingMipLevels,
            0,
            vk::RemainingArrayLayers
            );
        imageBarrier.image = image;

        vk::DependencyInfo dependencyInfo;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageBarrier;
        device.immediateCommandBuffer.pipelineBarrier2(&dependencyInfo);
    }

    void TextureManager::assert_handle(const TextureHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < textures.size());
        assert(textures[index].magicNumber == metaData);
    };

    void TextureManager::assert_handle(const SamplerHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < samplers.size());
        assert(samplers[index].magicNumber == metaData);
    }
}