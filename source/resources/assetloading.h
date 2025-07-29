#pragma once
#include "../device/device.h"
#include "../resources/resourcehelpers.h"

#include "../glmdefines.h"
#include <glm/glm.hpp>

#include <simdjson.h>
#include <print>
#include <execution>

namespace vulkan
{
    class Device;

    namespace assetloading
    {
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
            u16 material{};
        };

        struct Mesh {
            std::vector<Surface> surfaces;
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
            transparentBlend, transparentMask, opaque
        };

        struct Material {
            glm::vec4 baseColorFactor{};
            f32 metalnessFactor{}, roughnessFactor{}, emissiveStrength{};

            u32 bufferOffset{};
            u16 baseColorTexture{};
            u16 mrTexture{};
            u16 normalTexture{};
            u16 occlusionTexture{};
            u16 emissiveTexture{};

            MaterialType type{};
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

        struct TextureData
        {
            std::vector<Image> images;
            std::vector<ktxTexture*> ktxTextures;
            std::vector<std::vector<vk::BufferImageCopy>> copyRegions;
            u64 stagingBufferSize{};
        };

        struct GeometryData
        {
            std::vector<Vertex> vertices;
            std::vector<u32> indices;
        };

        struct GeometryBuffer
        {
            Buffer vertexBuffer;
            Buffer indexBuffer;
        };

        struct ChildlessNode
        {
            glm::mat4 localMatrix{};
            glm::mat4 worldMatrix{};

            u16 mesh{};
            u16 light{};
            u16 parent{};
        };

        enum LightType : u8 {
            Directional, Point, Spot
        };

        struct Light {
            alignas(16) glm::vec3 position{};
            alignas(16) glm::vec3 colour{};
            f32 intensity{};
            f32 range{};
            f32 innerAngle{};
            f32 outerAngle{};
        };

        struct SceneDescription;

        struct Node {
            std::vector<u16> children;
            glm::mat4 localMatrix{};
            glm::mat4 worldMatrix{};

            u16 mesh{};
            u16 light{};
            u16 parent{};
        };

        struct SceneDescription
        {
            TextureData textureData;
            GeometryData geometryData;
            GeometryBuffer geometryBuffer;
            std::vector<assetloading::Sampler> samplers;
            std::vector<assetloading::Mesh> meshes;
            std::vector<assetloading::Node> nodes;
            std::vector<assetloading::Material> materials;
            std::vector<assetloading::Light> lights;
            Buffer materialBuffer;
            Buffer lightBuffer;

            u64 lightBufferSize = 0;
        };

        SceneDescription load_scene(const UploadContext& context, Device& device, const std::filesystem::path& jsonPath, const std::filesystem::path& binPath);
        SceneDescription load_pak(const Device& device, const std::filesystem::path& jsonPath, const std::filesystem::path& binPath);
        std::vector<u32> read_indices(std::fstream& bin, const simdjson::dom::element& json);
        std::vector<assetloading::Vertex> read_vertices(std::fstream& bin, const simdjson::dom::element& json);
        std::vector<assetloading::Mesh> read_meshes(std::fstream& bin, const simdjson::dom::element& json);
        std::vector<assetloading::Material> read_materials(std::fstream& bin, const simdjson::dom::element& json);
        std::vector<assetloading::Node> read_nodes(std::fstream& bin, const simdjson::dom::element& json);
        std::vector<assetloading::Light> read_lights(std::fstream& bin, const simdjson::dom::element& json);
        std::vector<assetloading::Sampler> read_samplers(const Device& device, std::fstream& bin, const simdjson::dom::element& json);
        TextureData read_images(const Device& device, const simdjson::dom::element& json);

        namespace upload
        {
            void upload_scene_description_data(const UploadContext& context, Device& device, SceneDescription& sceneDescription);
            Buffer prepare_vertex_index_staging(Device& device, const GeometryData& geoData);
            Buffer prepare_image_staging(Device& device, const TextureData& textureData);

            GeometryBuffer prepare_geo_buffers(Device& device, const GeometryData& geoData);
            Buffer prepare_material_buffer(const Device& device, const std::vector<assetloading::Material>& materials);
            Buffer prepare_light_buffer(const Device& device, const std::vector<assetloading::Light>& lights);
        }
    }
}