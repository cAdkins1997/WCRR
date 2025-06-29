#pragma once
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <ktx.h>

#include "scenemanager.h"
#include "../common.h"
#include "../rendererresources.h"

enum class BufferHandle : u32 { Invalid = 0 };
enum class TextureHandle : u32 { Invalid = 0 };
enum class MaterialHandle : u32 { Invalid = 0 };
enum class MeshHandle : u32 { Invalid = 0 };
enum class SceneHandle : u32 { Invalid = 0 };
enum class NodeHandle : u32 { Invalid = 0 };
enum class LightHandle : u32 { Invalid = 0 };

typedef ktx_uint64_t ku64;
typedef ktx_uint32_t ku32;
typedef ktx_uint16_t ku16;
typedef ktx_uint8_t ku8;

typedef ktx_int64_t ki64;
typedef ktx_int32_t ki32;
typedef ktx_int16_t ki16;

struct Vertex {
    glm::vec3 position{};
    float uvX{};
    glm::vec3 normal{};
    float uvY{};
    glm::vec4 colour{};
};

struct VertexIndexData
{
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
};

alignas(16) struct AABB {
    glm::vec3 min{};
    glm::vec3 max{};
};

struct BoundingSphere {
    glm::vec3 center{};
    f32 radius{};
};

alignas(16) struct Surface {
    AABB boundingVolume;
    u32 initialIndex{};
    u32 indexCount{};
    MaterialHandle material{};
};

alignas(16) struct Material {
    glm::vec4 baseColorFactor{};
    f32 metalnessFactor{}, roughnessFactor{}, emissiveStrength{};

    TextureHandle baseColorTexture{};
    TextureHandle mrTexture{};
    TextureHandle normalTexture{};
    TextureHandle occlusionTexture{};
    TextureHandle emissiveTexture{};
    u32 bufferOffset{};
};

struct Mesh {
    std::vector<Surface> surfaces;
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


alignas(16) struct Node {
    std::vector<NodeHandle> children;
    glm::mat4 localMatrix{};
    glm::mat4 worldMatrix{};

    NodeHandle parent{};
    MeshHandle mesh{};
    NodeHandle light{};

    void refresh(const glm::mat4& parentMatrix);
};

alignas(16) struct Scene {
    std::vector<NodeHandle> nodes;
    std::vector<NodeHandle> renderableNodes;
    std::vector<NodeHandle> opaqueNodes;
    std::vector<NodeHandle> transparentNodes;
    std::vector<NodeHandle> lightNodes;
    std::vector<MeshHandle> meshes;
    std::vector<MaterialHandle> materials;
    std::vector<renderer::SamplerHandle> samplers;
    std::vector<TextureHandle> textures;
    std::vector<LightHandle> lights;
};

alignas(16) struct LoadedGlobalData
{
    VertexIndexData vertexIndexData;
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<vulkan::Sampler> samplers;
    std::vector<vulkan::Image> textures;
    std::vector<Light> lights;
};