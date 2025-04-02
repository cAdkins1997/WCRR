#pragma once
#include "resourcehelpers.h"
#include "../device/device.h"
#include "../commands.h"
#include "../glmdefines.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>

namespace vulkan {
    class Device;
    struct  UploadContext;

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

    class MeshManager {
    public:
        MeshManager(vulkan::Device& _device, UploadContext& _context, u64 initialCount);

        MeshHandle create_mesh(const fastgltf::Mesh& gltfMesh, const VertexBuffer& vertexBuffer);
        std::vector<MeshHandle> create_meshes(const fastgltf::Asset &asset);

        Mesh& get_mesh(MeshHandle handle);
        VertexBuffer& get_vertex_buffer(u32 index);

        u16 get_metadata_at_index(u32 index) const;

    private:
        Device& device;
        UploadContext& context;
        std::vector<VertexBuffer> vertexBuffers;
        std::vector<Mesh> meshes;
        u32 currentMesh = 0;
        u32 currentSurface = 0;
        u32 currentVertexBuffer = 0;

        std::vector<u32> indices;
        std::vector<Vertex> vertices;

        void upload_vertex_buffer(VertexBuffer& vertexBuffer) const;

        void assert_handle(const MeshHandle handle) const;
        void assert_handle(const VertexBufferHandle vertexBuffer) const;
    };
}
