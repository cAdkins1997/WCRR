
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

    typedef glm::vec4 Plane;
    typedef std::array<Plane, 5> Frustum;

    struct AABB {
        glm::vec3 min{};
        glm::vec3 max{};
    };

    struct BoundingSphere {
        glm::vec4 center{};
        f32 radius{};
    };

    struct Surface {
        AABB boundingVolume;
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

    struct Colour { f32 r, g, b; };
    struct Pos3 { f32 x, y, z; };

    struct Light {
        alignas(16) glm::vec3 position{};
        alignas(16) glm::vec3 colour{};
        f32 intensity{};
        f32 range{};
        f32 innerAngle{};
        f32 outerAngle{};
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

    struct Renderable {
        Surface surface;
        glm::mat4 worldMatrix{};
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
        std::vector<Renderable> renderables;
        u16 magicNumber{};
    };

    class SceneManager {
        public:
        SceneManager(Device& _device, UploadContext& _context);

        void release_gpu_resources();

        void draw_scene(const GraphicsContext& graphicsContext, SceneHandle handle, const glm::mat4& viewProjectionMatrix);
        void draw_aabb(const GraphicsContext& graphics_context, SceneHandle handle);
        void cpu_frustum_culling(Scene& scene, const glm::mat4& viewProjectionMatrix);

        void update_nodes(const glm::mat4& rootMatrix, SceneHandle handle);

        NodeHandle create_node(fastgltf::Node gltfNode);
        SceneHandle create_scene(fastgltf::Asset& asset);
        LightHandle create_light(const fastgltf::Light& gltfLight);

        void build_light_buffer(u64 size);
        void update_light_buffer();

        void remove_node(NodeHandle handle);
        void remove_scene(SceneHandle handle);

        Node& get_node(NodeHandle handle);
        Scene& get_scene(SceneHandle handle);
        Light& get_light(LightHandle handle);
        [[nodiscard]] u32 get_num_lights() const { return lights.size(); }

        Buffer& get_light_buffer() { return lightBuffer; }
        [[nodiscard]] u32 get_light_buffer_size() const { return lightBufferSize; }
        [[nodiscard]] vk::DeviceAddress get_light_buffer_address() const { return lightBufferAddress; }
        Light* get_lights() { return lights.data(); }
        std::string& get_light_names() { return lightNames; }

        MeshHandle create_mesh(const fastgltf::Mesh& gltfMesh, const VertexBuffer& vertexBuffer);
        std::vector<MeshHandle> create_meshes(const fastgltf::Asset &asset);
        std::vector<MeshHandle> create_aabb_meshes(const std::vector<NodeHandle>& nodes);

        Mesh& get_mesh(MeshHandle handle);
        VertexBuffer& get_vertex_buffer(u32 index);

        u16 get_metadata_at_index(u32 index) const;
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
        std::vector<Light> lights;
        Buffer lightBuffer;
        vk::DeviceAddress lightBufferAddress{};
        std::string lightNames;
        u32 lightBufferSize = 0;
        u32 lightCount = 0;
        u16 currentLight = 0;

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
        void assert_handle(LightHandle handle) const;
        void assert_handle(MaterialHandle handle) const;
        void assert_handle(MeshHandle handle) const;
        void assert_handle(VertexBufferHandle vertexBuffer) const;
        void assert_handle(TextureHandle handle) const;
        void assert_handle(SamplerHandle handle) const;
    };

    Frustum compute_frustum(const glm::mat4& viewProjection);
    AABB recompute_aabb(const AABB& oldAABB, const glm::mat4& transform);
    std::vector<glm::vec3> get_aabb_vertices(const AABB& aabb);
}
