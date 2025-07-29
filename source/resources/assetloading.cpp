
#include "assetloading.h"

    namespace vulkan::assetloading
    {

        SceneDescription load_scene(
                const UploadContext& context,
                Device& device,
                const std::filesystem::path& jsonPath,
                const std::filesystem::path& binPath)
        {
            auto desc = load_pak(device, jsonPath, binPath);
            upload::upload_scene_description_data(context, device, desc);

            return desc;
        }

        SceneDescription load_pak(const Device& device, const std::filesystem::path& jsonPath, const std::filesystem::path& binPath)
        {
            std::fstream bin(binPath, std::ios::in | std::ios::binary);

            simdjson::dom::parser parser;
            auto json = parser.load(jsonPath.string()).value();

            SceneDescription desc{};
            desc.nodes = read_nodes(bin, json);
            desc.geometryData.indices = read_indices(bin, json);
            desc.geometryData.vertices = read_vertices(bin, json);
            desc.meshes = read_meshes(bin, json);
            desc.materials = read_materials(bin, json);
            desc.lights = read_lights(bin, json);
            desc.samplers = read_samplers(device, bin, json);

            desc.lightBufferSize = desc.lights.size() * sizeof(Light);

            desc.textureData = read_images(device, json);

            return desc;
        }

        std::vector<u32> read_indices(std::fstream& bin, const simdjson::dom::element& json)
        {
            simdjson::dom::element element;
            if (auto error = json["index"].get(element); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load index buffer");

            u64 offset{};
            element.at(0)["offset"].get(offset);
            bin.seekg(offset, std::ios::beg);

            u64 size{};
            element.at(0)["size"].get(size);
            const u64 numIndices = size / sizeof(u32);

            std::vector<u32> indices;
            indices.reserve(numIndices);
            for (u64 i = 0; i < numIndices; i++)
            {
                u32 index{};
                bin.read(reinterpret_cast<char*>(&index), sizeof(u32));
                indices.push_back(index);
            }

            return indices;
        }

        std::vector<Vertex> read_vertices(std::fstream& bin, const simdjson::dom::element& json)
        {
            simdjson::dom::element element;
            if (auto error = json["vertex"].get(element); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load index buffer");

            u64 offset{};
            element.at(0)["offset"].get(offset);
            bin.seekg(offset, std::ios::beg);

            u64 size{};
            element.at(0)["size"].get(size);
            const u64 numVertices = size / sizeof(Vertex);

            std::vector<assetloading::Vertex> vertices;
            vertices.reserve(numVertices);

            for (u64 i = 0; i < numVertices; i++)
            {
                Vertex vertex{};
                bin.read(reinterpret_cast<char*>(&vertex), sizeof(Vertex));
                vertices.push_back(vertex);
            }

            return vertices;
        }

        std::vector<assetloading::Mesh> read_meshes(std::fstream& bin, const simdjson::dom::element& json)
        {
            simdjson::dom::element meshElement;
            if (auto error = json["mesh"].get(meshElement); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load mesh entries");

            simdjson::dom::element surfaceElement;
            if (auto error = json["surface"].get(surfaceElement); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load surface entries");

            std::vector<std::pair<Mesh, u32>> meshCountPairs;
            simdjson::dom::array meshEntries;
            meshElement.get(meshEntries);
            meshCountPairs.reserve(meshEntries.size());
            for (auto && entry : meshEntries)
            {
                u64 childCount{};
                entry["childCount"].get(childCount);
                meshCountPairs.emplace_back(vulkan::assetloading::Mesh{}, childCount);
            }

            simdjson::dom::array surfaceEntries;
            surfaceElement.get(surfaceEntries);

            u64 surfaceIndex = 0;
            for (auto & [mesh, count] : meshCountPairs)
            {
                mesh.surfaces.reserve(count);

                while (mesh.surfaces.size() < count)
                {
                    u64 offset{};
                    surfaceEntries.at(surfaceIndex)["offset"].get(offset);

                    u64 size{};
                    surfaceEntries.at(surfaceIndex)["size"].get(size);

                    bin.seekg(offset, std::ios::beg);

                    Surface surface{};
                    bin.read(reinterpret_cast<char*>(&surface), size);

                    mesh.surfaces.push_back(surface);
                    surfaceIndex++;
                }
            }

            std::vector<assetloading::Mesh> meshes;
            meshes.reserve(meshes.size());
            for (const auto& mesh : meshCountPairs | std::views::keys)
                meshes.push_back(mesh);


            return meshes;
        }

        std::vector<assetloading::Material> read_materials(std::fstream& bin, const simdjson::dom::element& json)
        {
            simdjson::dom::element materialElement;
            if (auto error = json["material"].get(materialElement); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load material entries");

            std::vector<assetloading::Material> materials;
            simdjson::dom::array materialEntries;
            materialElement.get(materialEntries);
            materials.reserve(materialEntries.size());

            for (auto && entry : materialElement)
            {
                u64 offset{};
                entry["offset"].get(offset);

                u64 size{};
                entry["size"].get(size);

                bin.seekg(offset, std::ios::beg);

                Material material{};
                bin.read(reinterpret_cast<char*>(&material), size);

                materials.push_back(material);
            }

            return materials;
        }

        std::vector<assetloading::Node> read_nodes(std::fstream& bin, const simdjson::dom::element& json)
        {
            simdjson::dom::element nodeElement;
            if (auto error = json["nodes"].get(nodeElement); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load node entries");

            simdjson::dom::element nodeChildOffsetElement;
            if (auto error = json["nodeChildOffset"].get(nodeChildOffsetElement); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load child entries");

            std::vector<assetloading::Node> nodes;
            std::vector<assetloading::ChildlessNode> childlessNodes;
            std::vector<u64> childCounts;

            simdjson::dom::array nodeEntries;
            nodeElement.get(nodeEntries);

            nodes.reserve(nodeEntries.size());
            childCounts.reserve(nodeEntries.size());

            for (const auto& entry : nodeEntries)
            {
                u64 childCount{};
                entry["childCount"].get(childCount);

                u64 offset{};
                entry["offset"].get(offset);

                u64 size{};
                entry["size"].get(size);

                childCounts.push_back(childCount);

                ChildlessNode node;
                bin.seekg(offset, std::ios::beg);
                bin.read(reinterpret_cast<char*>(&node), size);
                childlessNodes.push_back(node);
            }

            simdjson::dom::array childOffsetEntries;
            nodeChildOffsetElement.get(childOffsetEntries);

            for (const auto& [localMatrix, worldMatrix, mesh, light, parent] : childlessNodes)
            {
                Node node{};
                node.localMatrix = localMatrix;
                node.worldMatrix = worldMatrix;
                node.light = light;
                node.mesh = mesh;
                node.parent = parent;

                nodes.push_back(node);
            }

            u64 index = 0;
            for (u64 i = 0; i < nodes.size(); ++i)
            {
                auto& node = nodes[i];
                const auto childCount = childCounts[i];

                for (u64 j = 0; j < childCount; ++j)
                {
                    auto offsetEntry = childOffsetEntries.at(index);
                    u64 offset{};
                    offsetEntry["offset"].get(offset);

                    u64 size{};
                    offsetEntry["size"].get(size);

                    u16 childOffset{};
                    bin.seekg(offset, std::ios::beg);
                    bin.read(reinterpret_cast<char*>(&childOffset), size);
                    node.children.push_back(childOffset);
                    index++;
                }
            }

            return nodes;
        }

        std::vector<assetloading::Light> read_lights(std::fstream& bin, const simdjson::dom::element& json)
        {
            simdjson::dom::element lightElement;
            if (auto error = json["light"].get(lightElement); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load light entries");

            simdjson::dom::array lightEntries;
            lightElement.get(lightEntries);

            std::vector<Light> lights;
            lights.reserve(lightEntries.size());

            for (const auto&& entry : lightEntries)
            {
                u64 offset{};
                entry["offset"].get(offset);

                u64 size{};
                entry["size"].get(size);

                bin.seekg(offset, std::ios::beg);

                Light light{};
                bin.read(reinterpret_cast<char*>(&light), size);
                light.intensity = 1.0f;
                lights.push_back(light);
            }

            return lights;
        }

        std::vector<assetloading::Sampler> read_samplers(const Device& device, std::fstream& bin, const simdjson::dom::element& json)
        {

            std::vector<assetloading::Sampler> samplers;

            vk::SamplerCreateInfo samplerCI;
            samplerCI.minFilter = vk::Filter::eLinear;
            samplerCI.magFilter = vk::Filter::eLinear;
            samplerCI.mipmapMode = vk::SamplerMipmapMode::eLinear;
            vk::Sampler vkSampler;


            vk_check(
                    device.get_handle().createSampler(&samplerCI, nullptr, &vkSampler),
                    "failed to create sampler"
            );

            assetloading::Sampler sampler;
            sampler.sampler = vkSampler;
            sampler.magFilter = vk::Filter::eLinear;
            sampler.minFilter = vk::Filter::eLinear;
            samplers.push_back(sampler);

            return samplers;
        }

        TextureData read_images(const Device& device, const simdjson::dom::element& json)
        {
            std::vector<Image> images;
            u64 stagingBufferSize{};

            simdjson::dom::element imageElement;
            if (auto error = json["texture"].get(imageElement); error != simdjson::SUCCESS)
                throw std::runtime_error("Failed to load image entries");

            std::vector<ktxTexture*> texturePs;
            std::vector<std::vector<vk::BufferImageCopy>> copyRegions;

            simdjson::dom::array imageEntries;
            imageElement.get(imageEntries);

            images.reserve(imageEntries.size());
            texturePs.reserve(imageEntries.size());

            std::mutex mutex;

            std::for_each(std::execution::par, imageEntries.begin(), imageEntries.end(), [&](auto&& entry)
            {
                std::string_view path{};
                entry["path"].get(path);

                ktxTexture* texture = nullptr;

                const ktxResult result = ktxTexture_CreateFromNamedFile(
                        path.data(),
                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                        &texture);

                if (result != KTX_SUCCESS)
                    throw std::runtime_error("Failed to load texture");

                std::lock_guard guard(mutex);
                texturePs.push_back(texture);


                const ku32 width = texture->baseWidth;
                const ku32 height = texture->baseHeight;
                const ku32 mipLevels = texture->numLevels;

                const vk::Extent3D extent { width, height, 1 };
                const auto newImage = device.create_image(
                        extent,
                        VK_FORMAT_BC7_SRGB_BLOCK,
                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        mipLevels,
                        true);

                images.push_back(newImage);

                std::vector<vk::BufferImageCopy> newCopyRegions;
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

                        newCopyRegions.push_back(copyRegion);
                    }
                }

                copyRegions.push_back(newCopyRegions);
                stagingBufferSize += texture->dataSize;
            });

            return {images, texturePs, copyRegions, stagingBufferSize};
        }

        namespace upload
        {
            void upload_scene_description_data(const UploadContext& context, Device& device, SceneDescription& sceneDescription)
            {
                const auto& [vertices, indices] = sceneDescription.geometryData;
                const auto& [images, textures, regions, textureSize] = sceneDescription.textureData;
                const auto& lights = sceneDescription.lights;
                const auto& materials = sceneDescription.materials;
                const auto imageStaging = prepare_image_staging(device, sceneDescription.textureData);
                const auto geoStaging = prepare_vertex_index_staging(device, sceneDescription.geometryData);

                const u64 vertexBufferSize = vertices.size() * sizeof(Vertex);
                const u64 indexBufferSize = indices.size() * sizeof(u32);

                auto [vertexBuffer, indexBuffer] = prepare_geo_buffers(device, sceneDescription.geometryData);

                sceneDescription.geometryBuffer.vertexBuffer = vertexBuffer;
                sceneDescription.geometryBuffer.indexBuffer = indexBuffer;

                sceneDescription.materialBuffer = prepare_material_buffer(device, sceneDescription.materials);
                sceneDescription.lightBuffer = prepare_light_buffer(device, sceneDescription.lights);

                context.begin();
                context.update_uniform((void*)lights.data(), lights.size(), sceneDescription.materialBuffer);
                context.upload_uniform((void*)materials.data(), materials.size(), sceneDescription.lightBuffer);
                context.copy_buffer(geoStaging, vertexBuffer, 0, 0, vertexBufferSize);
                context.copy_buffer(geoStaging, indexBuffer, vertexBufferSize, 0, indexBufferSize);

                for (u32 i = 0; i < textures.size(); i++) {
                    context.image_barrier(images[i].handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
                    context.copy_buffer_to_image(imageStaging, images[i], vk::ImageLayout::eTransferDstOptimal, regions[i]);
                    context.image_barrier(images[i].handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
                }

                context.end();
                device.submit_upload_work(context, vk::PipelineStageFlagBits2::eNone, vk::PipelineStageFlagBits2::eCopy);
            }

            GeometryBuffer prepare_geo_buffers(Device& device, const GeometryData& geoData)
            {
                GeometryBuffer geoBuffer;
                const auto& [vertices, indices] = geoData;

                constexpr vk::BufferUsageFlags vertexBufferFlags =
                        vk::BufferUsageFlagBits::eStorageBuffer |
                        vk::BufferUsageFlagBits::eTransferDst |
                        vk::BufferUsageFlagBits::eVertexBuffer;
                const u64 vertexBufferSize = vertices.size() * sizeof(Vertex);
                geoBuffer.vertexBuffer = device.create_buffer(vertexBufferSize, vertexBufferFlags, VMA_MEMORY_USAGE_GPU_ONLY);

                constexpr vk::BufferUsageFlags indexBufferFlags = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
                const u64 indexBufferSize = indices.size() * sizeof(u32);
                geoBuffer.indexBuffer = device.create_buffer(indexBufferSize, indexBufferFlags, VMA_MEMORY_USAGE_GPU_ONLY);

                return geoBuffer;
            }

            Buffer prepare_vertex_index_staging(Device& device, const GeometryData& geoData)
            {
                const auto& [vertices, indices] = geoData;
                const u64 vertexBufferSize = vertices.size() * sizeof(Vertex);
                const u64 indexBufferSize = indices.size() * sizeof(u32);
                const u64 stagingSize = vertices.size() * sizeof(Vertex) + indices.size() * sizeof(u32);
                const auto allocator = device.get_allocator();

                auto staging = make_staging_buffer(stagingSize, device.get_allocator());
                auto data = staging.get_mapped_data();


                vmaMapMemory(allocator, staging.allocation, &data);
                memcpy(data, vertices.data(), vertexBufferSize);
                memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);
                vmaUnmapMemory(allocator, staging.allocation);

                return staging;
            }

            Buffer prepare_image_staging(Device& device, const TextureData& textureData)
            {
                const auto& [images, textures, regions, stagingSize] = textureData;

                auto staging = make_staging_buffer(stagingSize, device.get_allocator());
                auto stagingData = staging.get_mapped_data();
                std::vector<ku8> data;
                data.resize(stagingSize);

                auto ptr = data.data();
                for (const auto texture : textures)
                {
                    const ku64 size = texture->dataSize;
                    const ku8* bytes = texture->pData;
                    memcpy(ptr, bytes, size);
                    ptr += size / sizeof(ku8);

                    ktxTexture_Destroy(texture);
                }

                vmaMapMemory(device.get_allocator(), staging.allocation, &stagingData);
                memcpy(stagingData, data.data(), stagingSize);
                vmaUnmapMemory(device.get_allocator(), staging.allocation);

                return staging;
            }

            Buffer prepare_material_buffer(const Device& device, const std::vector<Material>& materials)
            {
                const u64 materialBufferSize = sizeof(GPUMaterial) * materials.size();
                auto materialBuffer = device.create_buffer(
                        materialBufferSize,
                        vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                        VMA_MEMORY_USAGE_CPU_TO_GPU
                );

                const auto materialData = static_cast<GPUMaterial*>(materialBuffer.get_mapped_data());

                for (u64 i = 0; i < materials.size(); i++) {
                    auto& material = materials[i];
                    const GPUMaterial gpuMat {
                        material.baseColorFactor,
                        material.metalnessFactor,
                        material.roughnessFactor,
                        material.emissiveStrength,

                        material.baseColorTexture,
                        material.mrTexture,
                        material.normalTexture,
                        material.occlusionTexture,
                        material.emissiveTexture,
                    };

                    materialData[i] = gpuMat;
                }

                return materialBuffer;
            }

            Buffer prepare_light_buffer(const Device& device, const std::vector<Light>& lights)
            {
                const u64 lightBufferSize = lights.size() * sizeof(Light);
                const auto lightBuffer = device.create_buffer(
                        lightBufferSize,
                        vk::BufferUsageFlagBits::eStorageBuffer |
                        vk::BufferUsageFlagBits::eShaderDeviceAddress |
                        vk::BufferUsageFlagBits::eTransferDst,
                        VMA_MEMORY_USAGE_CPU_TO_GPU
                );

                return lightBuffer;
            }
        }
    }
