
#include "gltfconvertor.h"

namespace engine
{
    Scene GltfLoader::load_gltf_representation(const fastgltf::Asset& gltfAsset)
    {
        Scene newScene;
        LoadedGlobalData data;

        auto& [vertexIndexData, meshes] = load_meshes(gltfAsset);

        data.vertexIndexData = vertexIndexData;
        data.meshes = meshes;

        for (u32 i = 1; i < data.meshes.size() + 1; i++)
        {
            auto handle = static_cast<MeshHandle>(i << 16 | data.meshes.size() - i);
            newScene.meshes.push_back(handle);
        }

        data.samplers = load_samplers(gltfAsset);

        for (u32 i = 1; i < data.samplers.size() + 1; i++)
        {
            auto handle = static_cast<renderer::SamplerHandle>(i << 16 | data.meshes.size() - i);
            newScene.samplers.push_back(handle);
        }

        data.lights = load_lights(gltfAsset);

        for (u32 i = 1; i < data.lights.size() + 1; i++)
        {
            auto handle = static_cast<LightHandle>(i << 16 | data.meshes.size() - i);
            newScene.lights.push_back(handle);
        }
    }

    GeometricData GltfLoader::load_meshes(const fastgltf::Asset& asset)
    {
        VertexIndexData vertexIndexData;
        auto& [vertices, indices] = vertexIndexData;
        std::vector<Mesh> meshes;
        meshes.reserve(asset.meshes.size());

        for (const auto& gltfMesh : asset.meshes)
        {
            Mesh newMesh;

            for (auto&& primitive : gltfMesh.primitives)
            {
                Surface newSurface;
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
                for (int i = initialVertex; i < vertices.size(); i++) {
                    min = glm::min(min, vertices[i].position);
                    max = glm::max(max, vertices[i].position);
                }

                newSurface.boundingVolume = {min, max};

                newMesh.surfaces.push_back(newSurface);
            }

            meshes.push_back(newMesh);
        }

        return {vertexIndexData, meshes};
    }

    std::vector<vulkan::Sampler> GltfLoader::load_samplers(const fastgltf::Asset& asset)
    {
        std::vector<vulkan::Sampler> samplers;
        samplers.reserve(asset.samplers.size());

        for (const auto& gltfSampler : asset.samplers)
            samplers.push_back(create_sampler(gltfSampler));

        return samplers;
    }

    std::vector<Light> GltfLoader::load_lights(const fastgltf::Asset& asset)
    {
        std::vector<Light> lights;
        lights.reserve(asset.lights.size());

        for (const auto& gltfLight : asset.lights)
            lights.push_back(load_light(gltfLight));

        return lights;
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


    vulkan::Sampler GltfLoader::create_sampler(const fastgltf::Sampler& fastgltfSampler)
    {
        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.magFilter = convert_filter(fastgltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = convert_filter(fastgltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.maxLod = vk::LodClampNone;
        samplerInfo.minLod = 0;
        samplerInfo.mipmapMode = convert_mipmap_mode(fastgltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));

        auto magFilter = convert_filter(fastgltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        const auto minFilter = convert_filter(fastgltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
        const auto mipMapMode = convert_mipmap_mode(fastgltfSampler.minFilter.value_or(magFilter));

        return device.create_sampler(magFilter, minFilter, mipMapMode);
    }

    Light GltfLoader::load_light(const fastgltf::Light& gltfLight)
    {
        Light newLight{};

        newLight.colour = {gltfLight.color.x(), gltfLight.color.y(), gltfLight.color.z()};
        newLight.intensity = gltfLight.intensity;

        if (gltfLight.range.has_value()) newLight.range = gltfLight.range.value();
        if (gltfLight.innerConeAngle.has_value()) newLight.innerAngle = gltfLight.innerConeAngle.value();
        if (gltfLight.outerConeAngle.has_value()) newLight.outerAngle = gltfLight.outerConeAngle.value();

        return newLight;
    }

    std::vector<Mesh> GltfLoader::process_meshes(const fastgltf::Asset& asset, Mesh& newMesh, TempMeshDataBuffer& dataBuffer)
    {
        auto& [vertices, indices] = dataBuffer;

        for (const auto& mesh : asset.meshes)
        {
            for (const auto& primitive : mesh.primitives)
            {
                Surface newSurface;
                newSurface.initialIndex = indices.size();

            }
        }
    }

    std::optional<fastgltf::Asset> GltfLoader::load_gltf_asset_from_path(const std::filesystem::path& path)
    {
        fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
        constexpr auto options =
            fastgltf::Options::DontRequireValidAssetMember |
                fastgltf::Options::AllowDouble  |
                    fastgltf::Options::LoadExternalBuffers;


        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) throw std::runtime_error("Failed to read glTF file");

        fastgltf::Asset gltf;

        if (auto type = determineGltfFileType(data.get()); type == fastgltf::GltfType::glTF) {
            auto load = parser.loadGltf(data.get(), path.parent_path(), options);
            if (load) gltf = std::move(load.get());

            else {
                std::cerr << "Failed to load glTF file" << to_underlying(load.error()) << std::endl;
                return{};
            }
        }
        else if (type == fastgltf::GltfType::GLB) {
            auto load = parser.loadGltfBinary(data.get(), path.parent_path(), options);
            if (load) gltf = std::move(load.get());

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

        process_gltf_extensions(gltf);
        return gltf;
    }

    void GltfLoader::process_gltf_extensions(fastgltf::Asset& gltf)
    {
        std::println("Gltf Extensions Used: ");
        std::string extensionsRequested;
        std::string extensionsUsed;
        for (const auto& extension : gltf.extensionsRequired)
            extensionsRequested.append(extension + '\n');
        for (const auto& extension : gltf.extensionsUsed)
            extensionsUsed.append(extension + '\n');

        std::println("{}\n\n, {}\n\n", extensionsRequested, extensionsUsed);
    }
}