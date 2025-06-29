#pragma once
#include <filesystem>
#include <iostream>
#include <print>

#include "../glmdefines.h"


#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <ktx.h>

#include "resourcetypes.h"
#include "../device/device.h"
#include "../commands.h"
#include "../rendererresources.h"

class vulkan::Device;

struct GeometricData
{
    VertexIndexData vertexIndexData;
    std::vector<Mesh> meshes;
};

namespace engine
{
    class GltfLoader
    {
    public:
        explicit GltfLoader(vulkan::Device& _device) : device(_device) {};

        Scene load_gltf_representation(const fastgltf::Asset& gltfAsset);

    private:


        GeometricData load_meshes(const fastgltf::Asset& asset);
        std::vector<vulkan::Sampler> load_samplers(const fastgltf::Asset& asset);
        std::vector<Light> load_lights(const fastgltf::Asset& asset);


        vulkan::Sampler create_sampler(const fastgltf::Sampler& fastgltfSampler);
        Light load_light(const fastgltf::Light& gltfLight);

    private:
        std::optional<fastgltf::Asset> load_gltf_asset_from_path(const std::filesystem::path& path);
        void process_gltf_extensions(fastgltf::Asset& gltf);

    private:
        vulkan::Device& device;
    };

    class GltfConvertor {

    };
}