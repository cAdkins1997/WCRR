
#pragma once
#include <filesystem>

#include "../glmdefines.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include "../resources/texturemanager.h"
#include "../resources/materialmanager.h"
#include "../resources/scenemanager.h"

namespace vulkan {
    class MeshManager;
    class TextureManager;
    class MaterialManager;
    struct GraphicsContext;
    struct  UploadContext;
    class SceneManager;

    enum class MaterialPass : u8 {
        Opaque, Transparent
    };

    enum LightType : u8 {
        Directional, Point, Spot
    };

    struct Light {
        std::string name;
        glm::vec3 direction{};
        glm::vec3 colour;
        f32 intensity;
        f32 range;
        f32 innerAngle;
        f32 outerAngle;
        NodeHandle node;
        u16 magicNumber;
        LightType type;
    };

    struct GPULight {
        glm::vec3 direction{};
        glm::vec3 colour;
        f32 intensity;
        f32 range;
        f32 innerAngle;
        f32 outerAngle;
        LightType type;
    };

    struct PushConstants {
        glm::mat4 renderMatrix;
        vk::DeviceAddress vertexBuffer;
        vk::DeviceAddress materialBuffer;
        vk::DeviceAddress lightBuffer;
        u32 materialIndex;
        u32 numLights;
    };

    struct Node {
        std::vector<NodeHandle> children;
        glm::mat4 localMatrix{};
        glm::mat4 worldMatrix{};

        NodeHandle parent{};
        MeshHandle mesh{};
        NodeHandle light{};
        u16 magicNumber{};

        void refresh(const glm::mat4& parentMatrix, SceneManager& sceneManager);
    };

    struct Scene {
        std::vector<NodeHandle> nodes;
        std::vector<NodeHandle> renderableNodes;
        std::vector<NodeHandle> opaqueNodes;
        std::vector<NodeHandle> transparentNodes;
        std::vector<NodeHandle> lightNodes;
        std::vector<MeshHandle> meshes;
        std::vector<MaterialHandle> materials;
        std::vector<SamplerHandle> samplers;
        std::vector<TextureHandle> textures;
        std::vector<LightHandle> lights;
        u16 magicNumber{};
    };

    class SceneManager {
        public:
        SceneManager(Device& _device, UploadContext& _context);
        void release_gpu_resources();

        void draw_scene(const GraphicsContext& context, MeshManager& meshManager, const MaterialManager& materialManager, SceneHandle handle) const;

        void update_nodes(const glm::mat4& rootMatrix, SceneHandle handle);

        NodeHandle create_node(fastgltf::Node gltfNode, const MeshManager& meshManager);
        SceneHandle create_scene(
            fastgltf::Asset& asset,
            MeshManager& meshManager,
            TextureManager& textureManager,
            MaterialManager& materialManager);
        LightHandle create_light(const fastgltf::Light& gltfLight);
        LightHandle create_point_light(glm::vec3 direction, glm::vec3 colour, f32 intensity, f32 range);

        void build_light_buffer(u64 size);
        void update_light_buffer();

        void remove_node(NodeHandle handle);
        void remove_scene(SceneHandle handle);

        Node& get_node(NodeHandle handle);
        Scene& get_scene(SceneHandle handle);
        Light& get_light(LightHandle handle);
        u32 get_num_lights() const { return lights.size(); }

        Buffer& get_light_buffer() { return lightBuffer; }
        u32 get_light_buffer_size() const { return lightBufferSize; }
        vk::DeviceAddress get_light_buffer_address() const { return lightBufferAddress; }


        std::optional<fastgltf::Asset> load_gltf(const std::filesystem::path& path) const;

    private:

        Device& device;
        std::vector<Scene> scenes;
        std::vector<Node> nodes;
        std::vector<Light> lights;

        UploadContext& context;

        u16 sceneCount = 0;
        u16 currentNode = 0;
        u32 nodeCount = 0;
        u16 currentLight = 0;
        u32 lightCount = 0;

        Buffer lightBuffer;
        vk::DeviceAddress lightBufferAddress{};
        u32 lightBufferSize = 0;

        void assert_handle(SceneHandle handle) const;
        void assert_handle(NodeHandle handle) const;
        void assert_handle(LightHandle handle) const;
    };
}
