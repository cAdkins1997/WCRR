
#include "scenemanager.h"

#include "../application.h"

namespace vulkan {
    void Node::refresh(const glm::mat4& parentMatrix, SceneManager& sceneManager) {
        worldMatrix = parentMatrix * localMatrix;
        for (const auto childHandle : children) {
            sceneManager.get_node(childHandle).refresh(parentMatrix, sceneManager);
        }
    }

    SceneManager::SceneManager(Device &_device, UploadContext& _context) : device(_device), context(_context) {}

    void SceneManager::draw_scene(const GraphicsContext& context, MeshManager& meshManager, const MaterialManager& materialManager, const SceneHandle handle) const {
        assert_handle(handle);
        const u32 sceneIndex = get_handle_index(handle);

        auto materialBuffer = materialManager.get_material_buffer_address();
        auto vertexBuffer = meshManager.get_vertex_buffer(0);
        context.bind_index_buffer(vertexBuffer.indexBuffer);

        for (auto scene = scenes[sceneIndex]; const auto& nodeHandle : scene.opaqueNodes) {
            assert_handle(nodeHandle);
            const u32 index = get_handle_index(nodeHandle);
            auto& node = nodes[index];
            auto& mesh = meshManager.get_mesh(node.mesh);

            const u32 numLights = static_cast<u32>(scene.lights.size());

            for (const auto& surface : mesh.surfaces) {
                const auto material = get_handle_index(surface.material);

                PushConstants pc { node.worldMatrix, vertexBuffer.vertexBufferAddress, materialBuffer, lightBufferAddress, material, numLights};
                context.set_push_constants(&pc, sizeof(pc), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
                context.draw(surface.indexCount, surface.initialIndex);
            }
        }
    }

    void SceneManager::update_nodes(const glm::mat4& rootMatrix, const SceneHandle handle) {
        for (auto& scene = get_scene(handle); const auto nodeHandle : scene.nodes) {
            auto& node = get_node(nodeHandle);
            node.refresh(rootMatrix, *this);
        }
    }

    NodeHandle SceneManager::create_node(fastgltf::Node gltfNode, const MeshManager& meshManager) {
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

        if (gltfNode.lightIndex.has_value()) {
            auto& light = lights[gltfNode.lightIndex.value()];
            light.node = handle;
            const u16 metaData = light.magicNumber;
            newNode.light = static_cast<NodeHandle>(metaData << 16 | gltfNode.lightIndex.value());
        }

        if (gltfNode.meshIndex.has_value()) {
            const u16 metaData = meshManager.get_metadata_at_index(gltfNode.meshIndex.value());
            newNode.mesh = static_cast<MeshHandle>(metaData << 16 | gltfNode.meshIndex.value());
        }

        nodes.push_back(newNode);

        nodeCount++;
        currentNode++;

        return handle;
    }

    SceneHandle SceneManager::create_scene(
        fastgltf::Asset &asset,
        MeshManager& meshManager,
        TextureManager& textureManager,
        MaterialManager& materialManager) {
        Scene newScene;

        auto meshes = meshManager.create_meshes(asset);

        newScene.meshes.reserve(meshes.size());
        for (const auto& meshHandle : meshes) {
            newScene.meshes.push_back(meshHandle);
        }

        for (const auto& gltfSampler : asset.samplers) {
            auto sampler = textureManager.create_sampler(gltfSampler);
            newScene.samplers.push_back(sampler);
        }

        newScene.lights.reserve(asset.lights.size());
        for (const auto& light : asset.lights) {
            newScene.lights.push_back(create_light(light));
        }

        newScene.nodes.reserve(asset.scenes.size());
        for (const auto& gltfNode : asset.nodes) {
            NodeHandle newNodeHandle = create_node(gltfNode, meshManager);
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

        auto textureHandles = textureManager.create_textures(
                VK_FORMAT_BC7_SRGB_BLOCK,
                VK_IMAGE_USAGE_SAMPLED_BIT,
                asset
        );

        newScene.textures =  std::move(textureHandles);

        for (const auto& texture : newScene.textures) {
            textureManager.set_texture_sampler(texture, newScene.samplers[0]);
        }

        materialManager.build_material_buffer(asset.materials.size());

        for (auto& gltfMaterial : asset.materials) {
            MaterialHandle materialHandle = materialManager.create_material(gltfMaterial);
            newScene.materials.push_back(materialHandle);
        }

        materialManager.update_material_buffer();

        for (const auto nodeHandle : newScene.renderableNodes) {
            const auto node = get_node(nodeHandle);
            auto mesh = meshManager.get_mesh(node.mesh);
            MaterialType type = invalid;

            for (const auto surface : mesh.surfaces) {
                type = materialManager.get_material_type(surface.material);
            }

            if (type == opaque) newScene.opaqueNodes.push_back(nodeHandle);
            if (type == transparent) newScene.transparentNodes.push_back(nodeHandle);
        }

        const auto handle = static_cast<SceneHandle>(newScene.magicNumber << 16 | sceneCount);
        scenes.push_back(std::move(newScene));

        sceneCount++;
        return handle;
    }

    LightHandle SceneManager::create_light(const fastgltf::Light& gltfLight) {
        Light newLight{};
        newLight.magicNumber = currentLight;
        newLight.colour = glm::vec3(gltfLight.color.x(), gltfLight.color.y(), gltfLight.color.z());
        newLight.intensity = gltfLight.intensity;
        if (newLight.intensity <= 0.0f) newLight.intensity = 5.0f;

        if (gltfLight.range.has_value()) {
            newLight.range = gltfLight.range.value();
        }
        if (gltfLight.type == fastgltf::LightType::Directional) {
            newLight.type = Directional;
        }
        else if (gltfLight.type == fastgltf::LightType::Point) {
            newLight.type = Point;
        }
        else if (gltfLight.type == fastgltf::LightType::Spot) {
            newLight.type = Spot;
        }
        if (gltfLight.innerConeAngle.has_value()) {
            newLight.innerAngle = gltfLight.innerConeAngle.value();
        }
        if (gltfLight.outerConeAngle.has_value()) {
            newLight.outerAngle = gltfLight.outerConeAngle.value();
        }

        newLight.name = gltfLight.name;
        lights.push_back(newLight);

        const auto handle = static_cast<LightHandle>(newLight.magicNumber << 16 | nodeCount);
        lightCount++;
        currentLight++;

        return handle;
    }

    LightHandle SceneManager::create_point_light(glm::vec3 direction, glm::vec3 colour, f32 intensity, f32 range) {
        Light newLight;
        newLight.direction = direction;
        newLight.magicNumber = currentLight;
        newLight.colour = colour;
        newLight.intensity = intensity;
        newLight.range = range;
        newLight.type = Point;
        newLight.name = "Manual Point Light #" + std::to_string(currentLight);

        lights.push_back(newLight);

        const auto handle = static_cast<LightHandle>(newLight.magicNumber << 16 | nodeCount);
        lightCount++;
        currentLight++;

        return handle;
    }

    void SceneManager::update_lights() {
        for (auto light : lights) {
            auto node = get_node(light.node);
            const auto mult = glm::vec4(light.direction, 1.0f) * node.worldMatrix;
            light.direction = normalize(glm::vec3(mult.x, mult.y, mult.z));
        }
    }

    void SceneManager::build_light_buffer(const u64 size) {
        if (size > 0) {
            lightBuffer = device.create_buffer(
                sizeof(GPULight) * size,
                vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                vk::BufferUsageFlagBits::eTransferDst,
                VMA_MEMORY_USAGE_CPU_TO_GPU
                );

            lightBufferSize = size * sizeof(GPULight);
            vk::BufferDeviceAddressInfo bdaInfo(lightBuffer.handle);
            lightBufferAddress = device.get_handle().getBufferAddress(bdaInfo);
        }
    }

    void SceneManager::update_light_buffer() {
        if (lightBufferSize > 0) {
            update_lights();
            auto* lightData = static_cast<GPULight*>(lightBuffer.get_mapped_data());
            for (u32 i = 0; i < lights.size(); i++) {
                GPULight gpuLight;
                gpuLight.direction = lights[i].direction;
                gpuLight.colour = lights[i].colour;
                gpuLight.intensity = lights[i].intensity;
                gpuLight.range = lights[i].range;
                gpuLight.type = lights[i].type;
                gpuLight.innerAngle = lights[i].innerAngle;
                gpuLight.outerAngle = lights[i].outerAngle;
                lightData[i] = gpuLight;
            }
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
        assert(lights[index].magicNumber == metaData && "Handle metadata does not match an existing light");
    }

    SceneManager::~SceneManager() {
        vmaDestroyBuffer(device.get_allocator(), lightBuffer.handle, lightBuffer.allocation);
    }
}
