
#pragma once
#include <filesystem>

#include "../glmdefines.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <ktx.h>

#include "../commands.h"
#include "../resources.h"

namespace vulkan {
    class DescriptorBuilder;
    class Device;

    struct GraphicsContext;
    struct UploadContext;
    class SceneManager;

    typedef ktx_uint64_t ku64;
    typedef ktx_uint32_t ku32;
    typedef ktx_uint16_t ku16;
    typedef ktx_uint8_t ku8;

    typedef ktx_int64_t ki64;
    typedef ktx_int32_t ki32;
    typedef ktx_int16_t ki16;

    struct Sampler {
        vk::Filter magFilter;
        vk::Filter minFilter;
        vk::Sampler sampler;
        u16 magicNumber;
    };

    struct Surface {
        u32 initialIndex{};
        u32 indexCount{};
        MaterialHandle material{};
    };

    struct Mesh {
        std::vector<Surface> surfaces;
        u16 magicNumber{};
    };

    struct VertexBuffer {
        std::vector<MeshHandle> meshes;
        Buffer indexBuffer{};
        Buffer vertexBuffer{};
        vk::DeviceAddress vertexBufferAddress{};
        u16 magicNumber{};
    };

    struct Vertex {
        glm::vec3 position{};
        float uvX{};
        glm::vec3 normal{};
        float uvY{};
        glm::vec4 colour{};
    };

    enum class MaterialPass : u8 {
        Opaque, Transparent
    };

    enum MaterialType : u8 {
        transparent, opaque, invalid
    };

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

    struct DirectionalLight {
        glm::vec3 direction{};
        glm::vec3 colour{};
        f32 intensity{}, quadratic{};
        NodeHandle node{};
        u16 magicNumber{};
    };

    struct PointLight {
        glm::vec3 position{};
        glm::vec3 colour{};
        f32 intensity{}, quadratic{};
        NodeHandle node{};
        u16 magicNumber{};
    };

    struct SpotLight {
        glm::vec3 position{};
        glm::vec3 direction{};
        glm::vec3 colour{};
        f32 intensity{}, quadratic{};
        f32 innerAngle{}, outerAngle{};
        NodeHandle node{};
        u16 magicNumber{};
    };

    struct GPUDirectionalLight {
        glm::vec3 direction{};
        glm::vec3 colour{};
    };

    struct GPUPointLight {
        glm::vec3 position{};
        glm::vec3 colour{};
        f32 intensity{}, quadratic{};
    };

    struct GPUSpotLight {
        glm::vec3 position{};
        glm::vec3 direction{};
        glm::vec3 colour{};
        f32 intensity{}, quadratic{};
        f32 innerAngle{}, outerAngle{};
    };

    struct PushConstants {
        glm::mat4 renderMatrix;
        vk::DeviceAddress vertexBuffer;
        vk::DeviceAddress materialBuffer;
        vk::DeviceAddress dirLightBuffer;
        vk::DeviceAddress pointLightBuffer;
        vk::DeviceAddress spotLightBuffer;
        u32 materialIndex;
        u32 numDirLights;
        u32 numPointLights;
        u32 numSpotLights;
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
        std::vector<LightHandle> dirLights;
        std::vector<LightHandle> pointLights;
        std::vector<LightHandle> spotLights;
        u16 magicNumber{};
    };

    class SceneManager {
        public:
        SceneManager(Device& _device, UploadContext& _context);

        void release_gpu_resources();

        void draw_scene(const GraphicsContext &graphicsContext, SceneHandle handle);

        void update_nodes(const glm::mat4& rootMatrix, SceneHandle handle);

        NodeHandle create_node(fastgltf::Node gltfNode);
        SceneHandle create_scene(fastgltf::Asset& asset);
        void create_directional_light(const fastgltf::Light& gltfLight);
        void create_point_light(const fastgltf::Light& gltfLight);
        void create_spot_light(const fastgltf::Light& gltfLight);

        std::optional<vk::DeviceAddress> build_light_buffer(Buffer& buffer, LightType type, const u64 size) const;
        void update_light_buffer(Buffer& lightBuffer, LightType type, u64 size);
        void update_light_buffers();

        void remove_node(NodeHandle handle);
        void remove_scene(SceneHandle handle);

        Node& get_node(NodeHandle handle);
        Scene& get_scene(SceneHandle handle);
        //Light& get_light(LightHandle handle);
        [[nodiscard]] u32 get_num_lights() const { return directionalLights.size() + spotLights.size() + pointLights.size(); }
        [[nodiscard]] u32 get_num_dirlights() const { return directionalLights.size(); }
        [[nodiscard]] u32 get_num_spotlights() const { return spotLights.size(); }
        [[nodiscard]] u32 get_num_pointlights() const { return pointLights.size(); }

        //[[nodiscard]] Light* get_all_lights() { return lights.data(); }

        //Buffer& get_light_buffer() { return lightBuffer; }
        //[[nodiscard]] u32 get_light_buffer_size() const { return lightBufferSize; }
        //[[nodiscard]] vk::DeviceAddress get_light_buffer_address() const { return lightBufferAddress; }
        GPUDirectionalLight* get_dir_lights() { return directionalLights.data(); }
        GPUPointLight* get_point_lights() { return pointLights.data(); }
        GPUSpotLight* get_spot_lights() { return spotLights.data(); }

        MeshHandle create_mesh(const fastgltf::Mesh& gltfMesh, const VertexBuffer& vertexBuffer);
        std::vector<MeshHandle> create_meshes(const fastgltf::Asset &asset);

        Mesh& get_mesh(MeshHandle handle);
        VertexBuffer& get_vertex_buffer(u32 index);

        [[nodiscard]] u16 get_metadata_at_index(u32 index) const;
        void upload_vertex_buffer(VertexBuffer& vertexBuffer) const;


        [[nodiscard]] std::optional<fastgltf::Asset> load_gltf(const std::filesystem::path& path) const;

        MaterialHandle create_material(const fastgltf::Material& material);
        std::vector<MaterialHandle> create_materials(const fastgltf::Asset& asset);

        void build_material_buffer(u64 size);
        void update_material_buffer();

        [[nodiscard]] MaterialType get_material_type(MaterialHandle handle) const;
        [[nodiscard]] Buffer get_material_buffer() const { return materialBuffer; };
        [[nodiscard]] u32 get_material_buffer_size() const { return materialBufferSize; };
        [[nodiscard]] vk::DeviceAddress get_material_buffer_address() const { return materialBufferAddress; }

        Material& get_material(MaterialHandle handle);

        //TODO This only needs one dynamic array of textures. Rewrite the loop that moves the data to the staging buffer using iterators/indicies into the global texture array
        //TODO alternatively find a way to do it one memcpy
        std::vector<TextureHandle> create_textures(VkFormat format, VkImageUsageFlags usage, fastgltf::Asset& asset);

        SamplerHandle create_sampler(const fastgltf::Sampler& fastgltfSampler);

        void delete_texture(TextureHandle handle);

        Image& get_texture(TextureHandle handle);
        Sampler& get_sampler(SamplerHandle handle);

        void write_textures(DescriptorBuilder& builder);

        void set_texture_sampler(TextureHandle textureHandle, SamplerHandle samplerHandle);

    private:
        Device& device;
        UploadContext& context;
        PushConstants pc;

        std::vector<Scene> scenes;
        u16 sceneCount = 0;

        //node data
        std::vector<Node> nodes;
        u16 currentNode = 0;
        u32 nodeCount = 0;

        //light data
        std::vector<GPUDirectionalLight> directionalLights;
    public:
        std::vector<GPUPointLight> pointLights;
    private:
        std::vector<GPUSpotLight> spotLights;
        Buffer directionalLightBuffer;
        Buffer pointLightBuffer;
        Buffer spotLightBuffer;
        vk::DeviceAddress dirLightBufferAddress{}, pointLightBufferAddress{}, spotLightBufferAddress{};
        u32 spotLightBufferSize = 0;
        u32 pointLightBufferSize = 0;
        u32 directionalLightBufferSize = 0;
        u16 currentSpotLight = 0;
        u16 currentPointLight = 0;
        u16 currentDirectionalLight = 0;

        //mesh data
        std::vector<VertexBuffer> vertexBuffers;
        std::vector<Mesh> meshes;
        u32 currentMesh = 0;
        u32 currentSurface = 0;
        u32 currentVertexBuffer = 0;

        std::vector<u32> indices;
        std::vector<Vertex> vertices;

        //material data
        u32 materialCount = 0;
        std::vector<Material> materials;
        std::vector<MaterialHandle> transparentMaterials;
        std::vector<MaterialHandle> opaqueMaterials;

        Buffer materialBuffer;
        vk::DeviceAddress materialBufferAddress{};
        u32 materialBufferSize = 0;

        //texture data
        std::vector<Image> textures;
        std::vector<Sampler> samplers;
        u32 textureCount = 0;
        u32 samplerCount = 0;

        void assert_handle(SceneHandle handle) const;
        void assert_handle(NodeHandle handle) const;
        //void assert_handle(LightHandle handle) const;
        void assert_handle(MaterialHandle handle) const;
        void assert_handle(MeshHandle handle) const;
        void assert_handle(VertexBufferHandle vertexBuffer) const;
        void assert_handle(TextureHandle handle) const;
        void assert_handle(SamplerHandle handle) const;
    };
}
