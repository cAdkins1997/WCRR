
#include "scenemanager.h"

#include "resourcehelpers.h"
#include "../application.h"

namespace vulkan {
    void Node::refresh(const glm::mat4& parentMatrix, SceneManager& sceneManager) {
        worldMatrix = parentMatrix * localMatrix;
        for (const auto childHandle : children) {
            sceneManager.get_node(childHandle).refresh(parentMatrix, sceneManager);
        }
    }

    SceneManager::SceneManager(Device& _device, UploadContext& _context) : device(_device), context(_context) {}

    void SceneManager::draw_scene(const GraphicsContext &graphicsContext, const FrameData& frame) const
    {
        const auto& vertexBuffer = vertexBuffers[0];
        graphicsContext.bind_index_buffer(vertexBuffer.indexBuffer);
        graphicsContext.draw_indirect(frame.drawIndirectCommandBuffer, get_num_surfaces());
    }

    void SceneManager::cpu_frustum_culling(Scene& scene, const glm::mat4& viewProjectionMatrix) {
        const Frustum viewFrustum = compute_frustum(viewProjectionMatrix);

        for (const auto& NodeHandle : scene.opaqueNodes) {
            const auto& node = get_node(NodeHandle);
            auto& mesh = get_mesh(node.mesh);
            
            for (auto& surface : mesh.surfaces) {
                AABB transformedAABB = recompute_aabb(surface.boundingVolume, node.worldMatrix);
                const auto [min, max] = transformedAABB;
                bool visible = true;
                for (const auto& plane : viewFrustum) {
                    i32 out = 0;
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

                if (visible) scene.renderables.push_back({surface, node.worldMatrix});
            }
        }
    }

    void SceneManager::update_nodes(const glm::mat4& rootMatrix, const SceneHandle handle) {
        for (auto& scene = get_scene(handle); const auto nodeHandle : scene.nodes) {
            auto& node = get_node(nodeHandle);
            node.refresh(rootMatrix, *this);
        }
    }

    NodeHandle SceneManager::create_node(fastgltf::Node gltfNode) {
        Node newNode;
        newNode.magicNumber = currentNode;

        std::visit(fastgltf::visitor { [&](fastgltf::math::fmat4x4 matrix) {
            memcpy(&newNode.localMatrix, matrix.data(), sizeof(matrix));
        },
            [&](fastgltf::TRS transform) {
                const glm::vec3 translation(transform.translation.x(), transform.translation.y(), transform.translation.z());
                const glm::quat rotation(transform.rotation.w(), transform.rotation.x(), transform.rotation.y(), transform.rotation.z());
                const glm::vec3 scale(transform.scale.x(), transform.scale.y(), transform.scale.z());

                const glm::mat4 translationMatrix = translate(glm::mat4(1.0f), translation);
                const glm::mat4 rotationMatrix = toMat4(rotation);
                const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

                newNode.localMatrix = translationMatrix * rotationMatrix * scaleMatrix;
            }
        }, gltfNode.transform);

        const auto handle = static_cast<NodeHandle>(newNode.magicNumber << 16 | nodeCount);

        if (gltfNode.meshIndex.has_value()) {
            const u16 metaData = get_metadata_at_index(gltfNode.meshIndex.value());
            newNode.mesh = static_cast<MeshHandle>(metaData << 16 | gltfNode.meshIndex.value());
        }

        nodes.push_back(newNode);

        nodeCount++;
        currentNode++;

        return handle;
    }

    SceneHandle SceneManager::create_scene(fastgltf::Asset &asset) {
        Scene newScene;

        auto meshes = create_meshes(asset);

        newScene.meshes.reserve(meshes.size());
        for (const auto& meshHandle : meshes) {
            newScene.meshes.push_back(meshHandle);
        }

        for (const auto& gltfSampler : asset.samplers) {
            auto sampler = create_sampler(gltfSampler);
            newScene.samplers.push_back(sampler);
            newScene.samplers.push_back(sampler);
        }

        newScene.lights.reserve(asset.lights.size());
        for (const auto& light : asset.lights) {
            newScene.lights.push_back(create_light(light));
        }

        newScene.nodes.reserve(asset.scenes.size());
        for (const auto& gltfNode : asset.nodes) {
            NodeHandle newNodeHandle = create_node(gltfNode);
            if (gltfNode.meshIndex.has_value()) {
                newScene.renderableNodes.push_back(newNodeHandle);
            }
            if (gltfNode.lightIndex.has_value()) {
                newScene.lightNodes.push_back(newNodeHandle);
            }

            newScene.nodes.push_back(newNodeHandle);
        }

        u32 index = 0;
        for (const auto& gltfNode : asset.nodes) {

            nodes[index].children.reserve(gltfNode.children.size());
            for (const auto& child : gltfNode.children) {

                const u16 currentMetaData = nodes[child].magicNumber;
                const auto childIndex = static_cast<u16>(child);
                auto childHandle = static_cast<NodeHandle>(currentMetaData << 16 | childIndex);

                nodes[index].children.push_back(childHandle);
            }

            index++;
        }

        for (const auto& nodeHandle : newScene.nodes) {
            for (auto& node = get_node(nodeHandle); const auto& childHandle : node.children) {
                get_node(childHandle).parent = nodeHandle;
            }
        }

        build_light_buffer(asset.lights.size());
        update_light_buffer();

        auto textureHandles = create_textures(
                VK_FORMAT_BC7_SRGB_BLOCK,
                VK_IMAGE_USAGE_SAMPLED_BIT,
                asset
        );

        newScene.textures =  std::move(textureHandles);

        for (const auto& texture : newScene.textures) {
            set_texture_sampler(texture, newScene.samplers[0]);
        }

        build_material_buffer(asset.materials.size());

        for (auto& gltfMaterial : asset.materials) {
            MaterialHandle materialHandle = create_material(gltfMaterial);
            newScene.materials.push_back(materialHandle);
        }

        update_material_buffer();

        for (const auto nodeHandle : newScene.renderableNodes) {
            const auto node = get_node(nodeHandle);
            auto mesh = get_mesh(node.mesh);
            MaterialType type = invalid;

            for (const auto surface : mesh.surfaces) {
                type = get_material_type(surface.material);
            }

            if (type == opaque) newScene.opaqueNodes.push_back(nodeHandle);
            if (type == transparent) newScene.transparentNodes.push_back(nodeHandle);
        }

        newScene.renderables.reserve(newScene.nodes.size());

        const auto handle = static_cast<SceneHandle>(newScene.magicNumber << 16 | sceneCount);
        scenes.push_back(std::move(newScene));

        sceneCount++;
        return handle;
    }

    LightHandle SceneManager::create_light(const fastgltf::Light& gltfLight) {
        Light newLight{};

        lightNames.append(std::to_string(lights.size()) + '\0');

        newLight.colour = {gltfLight.color.x(), gltfLight.color.y(), gltfLight.color.z()};
        newLight.intensity = gltfLight.intensity;

        if (gltfLight.range.has_value()) {
            newLight.range = gltfLight.range.value();
        }
        if (gltfLight.innerConeAngle.has_value()) {
            newLight.innerAngle = gltfLight.innerConeAngle.value();
        }
        if (gltfLight.outerConeAngle.has_value()) {
            newLight.outerAngle = gltfLight.outerConeAngle.value();
        }

        lights.push_back(newLight);

        const auto handle = static_cast<LightHandle>(currentLight << 16 | lightCount);
        lightCount++;
        currentLight++;

        return handle;
    }

    void SceneManager::build_light_buffer(const u64 size) {
        if (size > 0) {
            lightBuffer = device.create_buffer(
                sizeof(Light) * size,
                vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eTransferDst,
                VMA_MEMORY_USAGE_CPU_TO_GPU
                );

            lightBufferSize = size * sizeof(Light);
            lightBufferAddress = lightBuffer.deviceAddress;
        }
    }

    void SceneManager::update_light_buffer() {
        if (lightBufferSize > 0) {
            auto* lightData = static_cast<Light*>(lightBuffer.get_mapped_data());
            for (u32 i = 0; i < lights.size(); i++) {
                Light gpuLight;
                gpuLight.position = lights[i].position;
                gpuLight.colour = lights[i].colour;
                gpuLight.intensity = lights[i].intensity;
                gpuLight.range = lights[i].range;
                gpuLight.innerAngle = lights[i].innerAngle;
                gpuLight.outerAngle = lights[i].outerAngle;
                lightData[i] = gpuLight;
            }
            context.update_uniform(lightData, lightBufferSize, lightBuffer);
        }
    }


    void SceneManager::remove_node(const NodeHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        nodes.erase(nodes.begin() + index);
        nodeCount--;
    }

    void SceneManager::remove_scene(const SceneHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        scenes.erase(scenes.begin() + index);
        sceneCount--;
    }

    Node& SceneManager::get_node(const NodeHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return nodes[index];
    }

    Scene& SceneManager::get_scene(const SceneHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return scenes[index];
    }

    Light& SceneManager::get_light(const LightHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return lights[index];
    }

    MeshHandle SceneManager::create_mesh(const fastgltf::Mesh &gltfMesh, const VertexBuffer &vertexBuffer) {
        Mesh newMesh;
        newMesh.magicNumber = currentMesh;

        const auto handle = static_cast<MeshHandle>(newMesh.magicNumber << 16 | meshes.size());
        meshes.push_back(newMesh);
        currentMesh++;

        return handle;
    }

    std::vector<MeshHandle> SceneManager::create_meshes(const fastgltf::Asset &asset) {
                VertexBuffer vertexBuffer;
        indices.clear();
        vertices.clear();

        for (const auto& gltfMesh : asset.meshes) {
            Mesh newMesh;
            newMesh.magicNumber = currentMesh;

            for (auto&& primitive : gltfMesh.primitives) {
                Surface newSurface;
                newSurface.initialIndex = indices.size();
                newSurface.indexCount = asset.accessors[primitive.indicesAccessor.value()].count;
                if (primitive.materialIndex.has_value()) {
                    newSurface.material = static_cast<MaterialHandle>(primitive.materialIndex.value());
                }


                u64 initialVertex = vertices.size();
                {
                    auto& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];
                    indices.reserve(indices.size() + indexAccessor.count);

                    fastgltf::iterateAccessor<u32>(asset, indexAccessor,
                        [&](u32 idx) {
                        indices.push_back(idx + initialVertex);
                    });
                }

                {
                    auto& positionAccessor = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                    vertices.resize(vertices.size() + positionAccessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
                        [&](glm::vec3 v, size_t index) {
                            Vertex newVertex{};
                            newVertex.position = v;
                            newVertex.normal = { 1, 0, 0 };
                            newVertex.colour = glm::vec4 { 1.f };
                            newVertex.uvX = 0;
                            newVertex.uvY = 0;
                            vertices[initialVertex + index] = newVertex;
                    });
                }

                auto normals = primitive.findAttribute("NORMAL");
                if (normals != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normals->accessorIndex],
                        [&](glm::vec3 v, size_t index) {
                            vertices[initialVertex + index].normal = v;
                        });
                }

                auto uv = primitive.findAttribute("TEXCOORD_0");
                if (uv != primitive.attributes.end()) {

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uv->accessorIndex],
                        [&](glm::vec2 v, size_t index) {
                            vertices[initialVertex + index].uvX = v.x;
                            vertices[initialVertex + index].uvY = -v.y;
                        });
                }

                if (auto colors = primitive.findAttribute("COLOR_0"); colors != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[colors->accessorIndex],
                    [&](glm::vec4 v, u64 index) {
                        vertices[initialVertex + index].colour = v;
                    });
                }

                glm::vec3 min = vertices[initialVertex].position;
                glm::vec3 max = vertices[initialVertex].position;
                for (u64 i = initialVertex; i < vertices.size(); i++) {
                    min = glm::min(min, vertices[i].position);
                    max = glm::max(max, vertices[i].position);
                }

                newSurface.boundingVolume = {min, max};



                newMesh.surfaces.push_back(newSurface);
            }

            numSurfaces += newMesh.surfaces.size();
            const auto handle = static_cast<MeshHandle>(newMesh.magicNumber << 16 | meshes.size());
            meshes.push_back(newMesh);
            currentMesh++;

            vertexBuffer.meshes.push_back(handle);
        }

        upload_vertex_buffer(vertexBuffer);
        vertexBuffer.magicNumber = currentVertexBuffer;

        vertexBuffers.emplace_back(std::move(vertexBuffer));
        currentVertexBuffer++;

        return vertexBuffer.meshes;
    }

    void SceneManager::build_draw_indirect_buffers(Device& device, const UploadContext& context, const SceneHandle sceneHandle)
    {
        std::vector<vk::DrawIndexedIndirectCommand> indirectCommands;
        std::vector<GPUSurface> sceneSurfaces;

        u64 instanceCount = 0;
        for (const auto scene = get_scene(sceneHandle); const auto& nodeHandle : scene.opaqueNodes)
        {
            const auto node = get_node(nodeHandle);
            const auto renderMatrix = node.worldMatrix;
            const auto mesh = get_mesh(node.mesh);

            for (const auto& [boundingVolume, initialIndex, indexCount, material] : mesh.surfaces)
            {
                vk::DrawIndexedIndirectCommand command;
                command.indexCount = indexCount;
                command.firstIndex = initialIndex;
                command.firstInstance = instanceCount;
                command.instanceCount = 1;
                instanceCount++;

                GPUSurface gpuSurface;
                gpuSurface.renderMatrix = renderMatrix;
                gpuSurface.boundingVolume = boundingVolume;
                gpuSurface.indexCount = indexCount;
                gpuSurface.initialIndex = initialIndex;
                gpuSurface.materialIndex = get_handle_index(material);

                indirectCommands.push_back(command);
                sceneSurfaces.push_back(gpuSurface);
            }
        }

        const auto frames = device.get_frames();
        for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            auto& indirectBuffer = frames[i].drawIndirectCommandBuffer;
            auto& surfaceBuffer = frames[i].surfaceDataBuffer;

            const u64 indirectSize = sizeof(vk::DrawIndexedIndirectCommand) * indirectCommands.size();
            const u64 surfaceSize = sizeof(GPUSurface) * sceneSurfaces.size();

            auto indirectStaging = make_staging_buffer(indirectSize, device.get_allocator());
            auto surfaceStaging = make_staging_buffer(surfaceSize, device.get_allocator());

            memcpy(indirectStaging.get_mapped_data(), indirectCommands.data(), indirectSize);
            memcpy(surfaceStaging.get_mapped_data(), sceneSurfaces.data(), surfaceSize);

            context.copy_buffer(indirectStaging, indirectBuffer, 0, 0, indirectSize);
            context.copy_buffer(surfaceStaging, surfaceBuffer, 0, 0, surfaceSize);
        }
    }

    Mesh& SceneManager::get_mesh(MeshHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return meshes[index];
    }

    VertexBuffer& SceneManager::get_vertex_buffer(u32 index) {
        return vertexBuffers[index];
    }

    u16 SceneManager::get_metadata_at_index(u32 index) const {
        return meshes[index].magicNumber;
    }

    void SceneManager::upload_vertex_buffer(VertexBuffer &vertexBuffer) const {
        const u64 vertexBufferSize = vertices.size() * sizeof(Vertex);
        const u64 indexBufferSize = indices.size() * sizeof(u32);

        const auto allocator = device.get_allocator();

        constexpr VkBufferUsageFlags vertexBufferFlags =
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        vertexBuffer.vertexBuffer = create_device_buffer(vertexBufferSize, vertexBufferFlags, allocator);

        constexpr VkBufferUsageFlags indexBufferFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vertexBuffer.indexBuffer = create_device_buffer(indexBufferSize, indexBufferFlags, allocator);

        const Buffer stagingBuffer = make_staging_buffer(vertexBufferSize + indexBufferSize, allocator);

        void* data = stagingBuffer.info.pMappedData;

        vmaMapMemory(allocator, stagingBuffer.allocation, &data);
        memcpy(data, vertices.data(), vertexBufferSize);
        memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);
        vmaUnmapMemory(allocator, stagingBuffer.allocation);

        VkBufferDeviceAddressInfo bdaInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        bdaInfo.pNext = nullptr;
        bdaInfo.buffer = vertexBuffer.vertexBuffer.handle;
        VkDeviceAddress bda = vkGetBufferDeviceAddress(device.get_handle(), &bdaInfo);
        vertexBuffer.vertexBufferAddress = bda;

        context.begin();
        context.copy_buffer(stagingBuffer, vertexBuffer.vertexBuffer, 0, 0, vertexBufferSize);
        context.copy_buffer(stagingBuffer, vertexBuffer.indexBuffer, vertexBufferSize, 0, indexBufferSize);
        context.end();

        device.submit_upload_work(context, vk::PipelineStageFlagBits2::eNone, vk::PipelineStageFlagBits2::eCopy);

        device.get_handle().destroyBuffer(stagingBuffer.handle);
    }

    std::optional<fastgltf::Asset> SceneManager::load_gltf(const std::filesystem::path &path) const {

        fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
        constexpr auto options =
            fastgltf::Options::DontRequireValidAssetMember |
                fastgltf::Options::AllowDouble  |
                    fastgltf::Options::LoadExternalBuffers;


        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) {
            throw std::runtime_error("Failed to read glTF file");
        }

        fastgltf::Asset gltf;

        auto type = determineGltfFileType(data.get());
        if (type == fastgltf::GltfType::glTF) {
            auto load = parser.loadGltf(data.get(), path.parent_path(), options);
            if (load) {
                gltf = std::move(load.get());
            }
            else {
                std::cerr << "Failed to load glTF file" << to_underlying(load.error()) << std::endl;
                return{};
            }
        }
        else if (type == fastgltf::GltfType::GLB) {
            auto load = parser.loadGltfBinary(data.get(), path.parent_path(), options);
            if (load) {
                gltf = std::move(load.get());
            }
            else {
                std::cerr << "Failed to load glTF file" << to_underlying(load.error()) << std::endl;
                return{};
            }
        }
        else {
            std::cerr << "Failed to determine glTF container" << std::endl;
            return {};
        }

        auto asset = parser.loadGltf(data.get(), path.parent_path());
        if (auto error = asset.error(); error != fastgltf::Error::None) {
            throw std::runtime_error("failed to read GLTF buffer");
        }

        std::cout << '\n';

        std::cout << "Creating scene..." << '\n';
        std::cout << "Gltf Extensions Used: " << '\n';
        for (const auto& extension : gltf.extensionsRequired)
            std::cout << extension << '\n';
        for (const auto& extension : gltf.extensionsUsed)
            std::cout << extension << '\n';
        std::cout << std::endl;

        return gltf;
    }

    MaterialHandle SceneManager::create_material(const fastgltf::Material &material) {
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

    std::vector<MaterialHandle> SceneManager::create_materials(const fastgltf::Asset &asset) {
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

    void SceneManager::build_material_buffer(u64 size) {
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
    }

    void SceneManager::update_material_buffer() {
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

    MaterialType SceneManager::get_material_type(MaterialHandle handle) const {
        if (std::ranges::find(opaqueMaterials, handle) != opaqueMaterials.end()) {
            return opaque;
        };
        if (std::ranges::find(transparentMaterials, handle) != transparentMaterials.end()) {
            return transparent;
        }

        return opaque;
    }

    Material & SceneManager::get_material(MaterialHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return materials[index];
    }

    std::vector<TextureHandle> SceneManager::create_textures(
        VkFormat format,
        VkImageUsageFlags usage,
        fastgltf::Asset &asset) {
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
                    std::string prepend = "../assets/scenes/sponza/";
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
                    newImage = device.create_image(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipLevels, true);
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
                newImage = device.create_image(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipLevels, true);
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
                            newImage = device.create_image(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipLevels, true);
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

        for (auto currentTexture : ktxTexturePs) {
            const ku64 size = currentTexture->dataSize;
            const ku8* textureData = currentTexture->pData;
            for (u64 j = 0; j < size; j++) {
                data.push_back(textureData[j]);
            }

            ktxTexture_Destroy(currentTexture);
        }

        vmaMapMemory(device.get_allocator(), stagingBuffer.allocation, &stagingData);
        memcpy(stagingData, data.data(), stagingBufferSize);
        vmaUnmapMemory(device.get_allocator(), stagingBuffer.allocation);

        device.wait_on_work();
        context.begin();
        for (u32 i = 0; i < ktxTexturePs.size(); i++) {
            context.image_barrier(images[i].handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            device.immediateCommandBuffer.copyBufferToImage(stagingBuffer.handle, images[i].handle, vk::ImageLayout::eTransferDstOptimal, copyRegions[i]);
            context.image_barrier(images[i].handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
        context.end();
        device.submit_upload_work(context, vk::PipelineStageFlagBits2::eNone, vk::PipelineStageFlagBits2::eCopy);

        device.get_handle().destroyBuffer(stagingBuffer.handle);

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

    SamplerHandle SceneManager::create_sampler(const fastgltf::Sampler &fastgltfSampler) {
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

    void SceneManager::delete_texture(TextureHandle handle) {
        assert_handle(handle);
        auto index = get_handle_index(handle);
        auto& image = textures[index];
        vmaDestroyImage(device.get_allocator(), image.handle, image.allocation);
        textures.erase(textures.begin() + index);
        textureCount--;
    }

    Image& SceneManager::get_texture(TextureHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return textures[index];
    }

    Sampler & SceneManager::get_sampler(SamplerHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        Sampler& sampler = samplers[index];

        return samplers[index];
    }

    void SceneManager::write_textures(DescriptorBuilder &builder) {
        u32 i = 0;
        for (const auto& texture : textures) {
            const auto sampler = get_sampler(texture.sampler).sampler;
            builder.write_image(i, texture.view, sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
            i++;
        }
    }

    void SceneManager::set_texture_sampler(TextureHandle textureHandle, SamplerHandle samplerHandle) {
        auto texture = get_texture(textureHandle);
        texture.sampler = samplerHandle;
    }

    void SceneManager::assert_handle(const SceneHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index <= scenes.size() && "Handle out of bounds");
        assert(scenes[index].magicNumber == metaData && "Handle metadata does not match an existing scene");
    }

    void SceneManager::assert_handle(const NodeHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index <= nodes.size() && "Handle out of bounds");
        assert(nodes[index].magicNumber == metaData && "Handle metadata does not match an existing node");
    }

    void SceneManager::assert_handle(const LightHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index <= scenes.size() && "Handle out of bounds");
    }

    void SceneManager::assert_handle(MaterialHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < materials.size());
        assert(materials[index].magicNumber == metaData);
    }

    void SceneManager::assert_handle(MeshHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < meshes.size() && "Mesh index out of range");
        assert(meshes[index].magicNumber == metaData && "Mesh metadata doesn't match");
    }

    void SceneManager::assert_handle(VertexBufferHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < vertexBuffers.size() && "Vertex buffer index out of range");
        assert(vertexBuffers[index].magicNumber == metaData && "Vertex buffer metadata doesn't match");
    }

    void SceneManager::assert_handle(TextureHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < textures.size());
        assert(textures[index].magicNumber == metaData);
    }

    void SceneManager::assert_handle(SamplerHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < samplers.size());
        assert(samplers[index].magicNumber == metaData);
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

    AABB recompute_aabb(const AABB &oldAABB, const glm::mat4 &transform) {
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

    std::vector<glm::vec3> get_aabb_vertices(const AABB& aabb) {
        const glm::vec3& min = aabb.min;
        const glm::vec3& max = aabb.max;

        const glm::vec3 corners[8] = {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, max.z)
        };

        std::vector vertices = {
            corners[0], corners[1],
            corners[2], corners[3],
            corners[4], corners[5],
            corners[6], corners[7],

            corners[0], corners[2],
            corners[1], corners[3],
            corners[4], corners[6],
            corners[5], corners[7],

            corners[0], corners[4],
            corners[1], corners[5],
            corners[2], corners[6],
            corners[3], corners[7]
        };

        return vertices;
    }

    void SceneManager::release_gpu_resources() {
        vmaDestroyBuffer(device.get_allocator(), lightBuffer.handle, lightBuffer.allocation);
        vmaDestroyBuffer(device.get_allocator(), materialBuffer.handle, materialBuffer.allocation);

        for (auto& vertexBuffer : vertexBuffers) {
            vmaDestroyBuffer(device.get_allocator(), vertexBuffer.vertexBuffer.handle, vertexBuffer.vertexBuffer.allocation);
            vmaDestroyBuffer(device.get_allocator(), vertexBuffer.indexBuffer.handle, vertexBuffer.indexBuffer.allocation);
        }

        for (auto& texture : textures) {
            vmaDestroyImage(device.get_allocator(), texture.handle, texture.allocation);
            vkDestroyImageView(device.get_handle(), texture.view, nullptr);
        }

        for (auto& sampler : samplers) {
            vkDestroySampler(device.get_handle(), sampler.sampler, nullptr);
        }
    }
}
