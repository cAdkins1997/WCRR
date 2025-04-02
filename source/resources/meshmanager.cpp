

#include "meshmanager.h"

namespace vulkan {
    MeshManager::MeshManager(vulkan::Device &_device, u64 initialCount = 0) : device(_device) {
        meshes.reserve(initialCount);
    }

    MeshHandle MeshManager::create_mesh(const fastgltf::Mesh& gltfMesh, const VertexBuffer& vertexBuffer) {
        Mesh newMesh;
        newMesh.magicNumber = currentMesh;

        const auto handle = static_cast<MeshHandle>(newMesh.magicNumber << 16 | meshes.size());
        meshes.push_back(newMesh);
        currentMesh++;

        return handle;
    }

    std::vector<MeshHandle> MeshManager::create_meshes(const fastgltf::Asset &asset) {
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

                newMesh.surfaces.push_back(newSurface);
            }

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

    Mesh& MeshManager::get_mesh(MeshHandle handle) {
        assert_handle(handle);
        const u32 index = get_handle_index(handle);
        return meshes[index];
    }

    VertexBuffer & MeshManager::get_vertex_buffer(const u32 index) {
        return vertexBuffers[index];
    }

    u16 MeshManager::get_metadata_at_index(const u32 index) const {
        return meshes[index].magicNumber;
    }

    void MeshManager::upload_vertex_buffer(VertexBuffer& vertexBuffer) const {
        const u64 vertexBufferSize = vertices.size() * sizeof(Vertex);
        const u64 indexBufferSize = indices.size() * sizeof(u32);

        const vma::Allocator allocator = device.get_allocator();

        constexpr vk::BufferUsageFlags vertexBufferFlags =
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferDst |
                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                    vk::BufferUsageFlagBits::eVertexBuffer;

        vertexBuffer.vertexBuffer = create_device_buffer(vertexBufferSize, vertexBufferFlags, allocator);

        constexpr vk::BufferUsageFlags indexBufferFlags = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        vertexBuffer.indexBuffer = create_device_buffer(indexBufferSize, indexBufferFlags, allocator);

        const Buffer stagingBuffer = make_staging_buffer(vertexBufferSize + indexBufferSize, allocator);

        void* data = stagingBuffer.info.pMappedData;

        vk_check(
            allocator.mapMemory(stagingBuffer.allocation, &data),
            "Failed to map staging buffer"
            );
        memcpy(data, vertices.data(), vertexBufferSize);
        memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);
        allocator.unmapMemory(stagingBuffer.allocation);

        vk::BufferDeviceAddressInfo bdaInfo(vertexBuffer.vertexBuffer.handle);
        bdaInfo.pNext = nullptr;
        bdaInfo.setBuffer(vertexBuffer.vertexBuffer.handle);
        vk::DeviceAddress bda = device.get_handle().getBufferAddress(bdaInfo);
        vertexBuffer.vertexBufferAddress = bda;

        /*device.submit_immediate_work([&](vk::CommandBuffer commandBuffer) {

        });*/
        copy_buffer(device.immediateCommandBuffer, stagingBuffer, vertexBuffer.vertexBuffer, 0, 0, vertexBufferSize);
        copy_buffer(device.immediateCommandBuffer, stagingBuffer, vertexBuffer.indexBuffer, vertexBufferSize, 0, indexBufferSize);
        device.get_handle().destroyBuffer(stagingBuffer.handle);
    }

    void MeshManager::assert_handle(const MeshHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < meshes.size() && "Mesh index out of range");
        assert(meshes[index].magicNumber == metaData && "Mesh metadata doesn't match");
    }

    void MeshManager::assert_handle(const VertexBufferHandle handle) const {
        const u32 metaData = get_handle_metadata(handle);
        const u32 index = get_handle_index(handle);

        assert(index < vertexBuffers.size() && "Vertex buffer index out of range");
        assert(vertexBuffers[index].magicNumber == metaData && "Vertex buffer metadata doesn't match");
    }
}
