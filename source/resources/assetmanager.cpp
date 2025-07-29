
#include "assetmanager.h"

namespace vulkan::assetmanager {

    AssetManager::AssetManager(Device& device, UploadContext& context) {
        build_error_assets(device, context);
    }

    void AssetManager::draw_scene(const GraphicsContext &graphicsContext, const glm::mat4 &viewProjectionMatrix) {
        pc.materialBuffer = materialBuffer.address;
        pc.vertexBuffer = vertexBuffer.address;
        pc.lightBuffer = lightBuffer.address;

        pc.numLights = static_cast<u32>(lights.size());

        cpu_frustum_culling(viewProjectionMatrix);

        graphicsContext.bind_index_buffer(indexBuffer);

        for (const auto&[surface, worldMatrix] : renderables) {
            pc.renderMatrix = worldMatrix;
            pc.materialIndex = get_handle_index(surface.material);
            graphicsContext.set_push_constants(&pc, sizeof(pc), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
            graphicsContext.draw(surface.indexCount, surface.initialIndex);
        }

        renderables.clear();
    }

    void AssetManager::cpu_frustum_culling(const glm::mat4 &viewProjectionMatrix) {
        const Frustum viewFrustum = compute_frustum(viewProjectionMatrix);

        for (const auto& NodeHandle : opaqueNodes) {
            const auto& node = get_node(NodeHandle);

            for (auto& [mesh, metaData] = meshes[node.mesh]; auto& surface : mesh.surfaces) {
                AABB transformedAABB = recompute_aabb(surface.boundingVolume, node.worldMatrix);
                const auto [min, max] = transformedAABB;
                bool visible = true;
                for (const auto& plane : viewFrustum) {
                    int out = 0;
                    out += dot(plane, glm::vec4(min.x, min.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
                    out += dot(plane, glm::vec4(max.x, min.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
                    out += dot(plane, glm::vec4(min.x, max.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
                    out += dot(plane, glm::vec4(max.x, max.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
                    out += dot(plane, glm::vec4(min.x, min.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
                    out += dot(plane, glm::vec4(max.x, min.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
                    out += dot(plane, glm::vec4(min.x, max.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
                    out += dot(plane, glm::vec4(max.x, max.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
                    if (out == 8) visible = false;
                }

                if (visible) renderables.push_back({surface, node.worldMatrix});
            }
        }
    }

    Frustum compute_frustum(const glm::mat4& viewProjection) {
        const glm::mat4 transpose = glm::transpose(viewProjection);

        const Plane leftPlane = transpose[3] + transpose[0];
        const Plane rightPlane = transpose[3] - transpose[0];
        const Plane bottomPlane = transpose[3] + transpose[1];
        const Plane topPlane = transpose[3] - transpose[1];
        const Plane nearPlane = transpose[3] + transpose[2];

        return {leftPlane, rightPlane, bottomPlane, topPlane, nearPlane};
    }

    AABB recompute_aabb(const AABB& oldAABB, const glm::mat4& transform) {
        const glm::vec3& min = oldAABB.min;
        const glm::vec3& max = oldAABB.max;

        const glm::vec3 corners[8] = {
                glm::vec3(transform * glm::vec4(min.x, min.y, min.z, 1.0f)),
                glm::vec3(transform * glm::vec4(min.x, max.y, min.z, 1.0f)),
                glm::vec3(transform * glm::vec4(min.x, min.y, max.z, 1.0f)),
                glm::vec3(transform * glm::vec4(min.x, max.y, max.z, 1.0f)),
                glm::vec3(transform * glm::vec4(max.x, min.y, min.z, 1.0f)),
                glm::vec3(transform * glm::vec4(max.x, max.y, min.z, 1.0f)),
                glm::vec3(transform * glm::vec4(max.x, min.y, max.z, 1.0f)),
                glm::vec3(transform * glm::vec4(max.x, max.y, max.z, 1.0f))
        };


        AABB result {corners[0], corners[0]};

        for (const auto& corner : corners) {
            result.min = glm::min(result.min, corner);
            result.max = glm::max(result.max, corner);
        }

        return result;
    }

    void AssetManager::build_asset_metadata(const SceneDescription &desc) {

        nodes.reserve(desc.nodes.size());
        for (const auto& node : desc.nodes)
            nodes.emplace_back(std::pair{node, nodes.size()});

        u64 numSurface = 0;
        meshes.reserve(desc.meshes.size());
        for (const auto& mesh : desc.meshes) {
            meshes.emplace_back(std::pair{mesh, meshes.size()});
            for (const auto& surface : mesh.surfaces) numSurface++;
        }

        renderables.reserve(numSurface);

        textures.reserve(desc.textureData.images.size());
        for (const auto& texture : desc.textureData.images)
            textures.emplace_back(std::pair{texture, textures.size()});

        samplers.reserve(desc.samplers.size());
        for (const auto& sampler : desc.samplers)
            samplers.emplace_back(std::pair{sampler, samplers.size()});

        materials.reserve(desc.materials.size());
        for (const auto& material : desc.materials)
            materials.emplace_back(std::pair{material, materials.size()});

        lights.reserve(desc.lights.size());
        for (const auto& light : desc.lights) {
            lightNames.append(std::to_string(lights.size()) + '\0');
            lights.emplace_back(std::pair{light, lights.size()});
        }

        lightData.reserve(lights.size());
        for (const auto &key: lights | std::views::keys)
            lightData.emplace_back(key);

        lightBuffer = desc.lightBuffer;
        lightBufferSize += desc.lightBufferSize;
        vertexBuffer = desc.geometryBuffer.vertexBuffer;
        indexBuffer = desc.geometryBuffer.indexBuffer;
        materialBuffer = desc.materialBuffer;
    }

    void AssetManager::build_handles() {
        to_handles<Node, NodeHandle>(nodes, nodeHandles);
        to_handles<Mesh, MeshHandle>(meshes, meshHandles);
        to_handles<Image, TextureHandle>(textures, textureHandles);
        to_handles<Sampler, SamplerHandle>(samplers, samplerHandles);
        to_handles<Material, MaterialHandle>(materials, materialHandles);
        to_handles<Light, LightHandle>(lights, lightHandles);

        for (auto& key: textures | std::views::keys)
            key.sampler = samplerHandles[1];
    }

    void AssetManager::add_asset(const SceneDescription &desc) {
        build_asset_metadata(desc);
        build_handles();
        bin_nodes();
    }

    void AssetManager::assert_handle(const NodeHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index <= nodes.size() && "Handle out of bounds");
        assert(nodes[index].second == metaData && "Handle metadata does not match an existing node");
    }

    void AssetManager::assert_handle(const LightHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index <= lights.size() && "Handle out of bounds");
        assert(materials[index].second == metaData);
    }

    void AssetManager::assert_handle(MaterialHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < materials.size());
        assert(materials[index].second == metaData);
    }

    void AssetManager::assert_handle(MeshHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < meshes.size() && "Mesh index out of range");
        assert(meshes[index].second == metaData && "Mesh metadata doesn't match");
    }

    void AssetManager::assert_handle(TextureHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < textures.size());
        assert(textures[index].second == metaData);
    }

    void AssetManager::assert_handle(SamplerHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < samplers.size());
        assert(samplers[index].second == metaData);
    }

    Node& AssetManager::get_node(NodeHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return nodes[index].first;
    }

    Light& AssetManager::get_light(LightHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return lights[index].first;
    }

    Mesh& AssetManager::get_mesh(MeshHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return meshes[index].first;
    }

    Material& AssetManager::get_material(MaterialHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return materials[index].first;
    }

    Image& AssetManager::get_texture(TextureHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return textures[index].first;
    }

    Sampler& AssetManager::get_sampler(SamplerHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return samplers[index].first;
    }

    void AssetManager::update_light_buffer() {
        if (!lights.empty()) {
            auto* lightData = static_cast<Light*>(lightBuffer.get_mapped_data());
            for (u32 i = 0; i < lights.size(); i++) {
                Light gpuLight;
                gpuLight.position = lights[i].first.position;
                gpuLight.colour = lights[i].first.colour;
                gpuLight.intensity = lights[i].first.intensity;
                gpuLight.range = lights[i].first.range;
                gpuLight.innerAngle = lights[i].first.innerAngle;
                gpuLight.outerAngle = lights[i].first.outerAngle;
                lightData[i] = gpuLight;
            }
        }
    }

    void AssetManager::update_nodes(const glm::mat4& rootMatrix) {
        for (const auto handle : nodeHandles) {
            auto node = get_node(handle);
            for (const auto childIndex : node.children) {
                auto child = nodes[childIndex].first;
                node.worldMatrix = rootMatrix * child.localMatrix;
            }
        }
    }

    void AssetManager::update_material_buffer() {
        auto* materialData = static_cast<Material*>(materialBuffer.get_mapped_data());
        for (u32 i = 0; i < materials.size(); i++) {
            materialData[i] = materials[i].first;
        }
    }

    void AssetManager::write_textures(DescriptorBuilder &builder) {
        u32 i = 0;
        for (const auto &key: textures | std::views::keys) {
            const auto sampler = get_sampler(key.sampler).sampler;
            builder.write_image(i, key.view, sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
            i++;
        }
    }

    void AssetManager::bin_nodes() {
        for (const auto& handle : nodeHandles) {
            auto node = get_node(handle);
            if (node.mesh != 0) renderableNodes.push_back(handle);
            if (node.light != 0) lightNodes.push_back(handle);
        }

        for (const auto& handle : renderableNodes) {
            auto node = get_node(handle);
            auto mesh = meshes[node.mesh].first;
            auto type = opaque;
            for (const auto& surface : mesh.surfaces) {
                const auto material = materials[surface.material].first;
                type = material.type;
            }
            if (type == opaque) opaqueNodes.push_back(handle);
            else if (type == transparentBlend) blendedNodes.push_back(handle);
            else if (type == transparentMask) maskedNodes.push_back(handle);
        }
    }

    void AssetManager::build_error_assets(Device& device, UploadContext& context) {
        constexpr auto errorNode = Node{};
        nodes.emplace_back(errorNode, 0);
        nodeHandles.push_back(NodeHandle::Invalid);

        auto errorMesh = build_error_mesh(device, context);
        meshes.emplace_back(errorMesh, 0);
        meshHandles.push_back(MeshHandle::Invalid);

        auto errorImage = build_error_image(device, context);
        textures.emplace_back(errorImage, 0);
        textureHandles.push_back(TextureHandle::Invalid);

        const auto errorSampler = Sampler{};
        samplers.emplace_back(errorSampler, 0);
        samplerHandles.push_back(SamplerHandle::Invalid);

        constexpr auto errorMaterial = Material{};
        materials.emplace_back(errorMaterial, 0);
        materialHandles.push_back(MaterialHandle::Invalid);

        constexpr auto errorLight = Light{};
        lights.emplace_back(errorLight, 0);
        lightHandles.push_back(LightHandle::Invalid);
    }

    Image AssetManager::build_error_image(Device &device, UploadContext &context) {
        const u32 black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
        const u32 magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
        std::array<u32, 16 * 16 > pixels{};
        for (int x = 0; x < 16; x++)
            for (int y = 0; y < 16; y++)
                pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;

        const auto errorImage = device.create_image(
                {1, 1, 1},
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                0,
                false
                );

        auto staging = make_staging_buffer(pixels.size() * sizeof(u32), device.get_allocator());
        auto stagingData = staging.get_mapped_data();

        vmaMapMemory(device.get_allocator(), staging.allocation, &stagingData);
        memcpy(stagingData, pixels.data(), pixels.size());
        vmaUnmapMemory(device.get_allocator(), staging.allocation);

        context.begin();
        context.image_barrier(errorImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        context.copy_buffer_to_image(staging, errorImage, {1, 1, 1}, vk::ImageLayout::eTransferDstOptimal);
        context.image_barrier(errorImage.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        context.end();

        device.wait_on_work();
        device.submit_upload_work(context, vk::PipelineStageFlagBits2::eNone, vk::PipelineStageFlagBits2::eCopy);
        device.wait_on_work();

        return errorImage;
    }

    Mesh AssetManager::build_error_mesh(Device &device, UploadContext &context) {
        Mesh errorMesh{};
        constexpr Surface errorSurface{ .indexCount = 100};
        errorMesh.surfaces.push_back(errorSurface);
        return errorMesh;
    }
}