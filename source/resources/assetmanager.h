#pragma once
#include "assetloading.h"

    namespace vulkan::assetmanager {
        using namespace vulkan::assetloading;

        struct PushConstants {
            glm::mat4 renderMatrix;
            vk::DeviceAddress vertexBuffer;
            vk::DeviceAddress materialBuffer;
            vk::DeviceAddress lightBuffer;
            u32 materialIndex;
            u32 numLights;
        };


        template<typename T, typename Y>
        Y to_handle(const std::pair<T, u16>& item, u16 handleCount) {
            return static_cast<Y>(item.second << 16 | handleCount);
        }

        template<typename T, typename Y>
        void to_handles(const std::vector<std::pair<T, u16>>& items, std::vector<Y>& handles) {
            handles.reserve(handles.size() + items.size());
            for (u16 i = 1; i < items.size(); i++) {
                const u32 index = static_cast<u32>(handles.size());
                const u32 metaData = items[i].second << 16;
                handles.push_back(static_cast<Y>(metaData | index));
            }
        }

        struct Renderable {
            Surface surface;
            glm::mat4 worldMatrix{};
        };

        Frustum compute_frustum(const glm::mat4& viewProjection);
        AABB recompute_aabb(const AABB& oldAABB, const glm::mat4& transform);

        class AssetManager {
        public:
            AssetManager(Device& device, UploadContext& context);

            void draw_scene(const GraphicsContext& graphicsContext, const glm::mat4& viewProjectionMatrix);

            void add_asset(const SceneDescription& desc);

            void assert_handle(NodeHandle handle) const;
            void assert_handle(LightHandle handle) const;
            void assert_handle(MeshHandle handle) const;
            void assert_handle(MaterialHandle handle) const;
            void assert_handle(TextureHandle handle) const;
            void assert_handle(SamplerHandle handle) const;

            Node& get_node(NodeHandle handle);
            Light& get_light(LightHandle handle);
            Mesh& get_mesh(MeshHandle handle);
            Material& get_material(MaterialHandle handle);
            Image& get_texture(TextureHandle handle);
            Sampler& get_sampler(SamplerHandle handle);

            void update_light_buffer();
            void update_material_buffer();

            void write_textures(DescriptorBuilder& builder);

            Light* get_lights() { return lightData.data(); }
            char* get_light_names() { return lightNames.data(); }
            u32 get_num_lights() const { return lights.size(); }
            Buffer& get_light_buffer() { return lightBuffer; }

            void update_nodes(const glm::mat4& rootMatrix);

        private:
            PushConstants pc{};
            void build_asset_metadata(const SceneDescription &desc);
            void build_handles();
            void build_error_assets(Device& device, UploadContext& context);
            Image build_error_image(Device& device, UploadContext& context);
            Mesh build_error_mesh(Device& device, UploadContext& context);

            void bin_nodes();

            void cpu_frustum_culling(const glm::mat4& viewProjectionMatrix);

            Buffer lightBuffer{};
            u64 lightBufferSize{};

            Buffer vertexBuffer{};
            Buffer indexBuffer{};

            Buffer materialBuffer{};

            std::vector<Renderable> renderables;
            std::string lightNames{};

            std::vector<std::pair<Node, u16>> nodes;
            std::vector<std::pair<Mesh, u16>> meshes;
            std::vector<std::pair<Image, u16>> textures;
            std::vector<std::pair<Sampler, u16>> samplers;
            std::vector<std::pair<Material, u16>> materials;
            std::vector<std::pair<Light, u16>> lights;
            std::vector<Light> lightData;

            std::vector<NodeHandle> renderableNodes;
            std::vector<NodeHandle> opaqueNodes;
            std::vector<NodeHandle> blendedNodes;
            std::vector<NodeHandle> maskedNodes;
            std::vector<NodeHandle> lightNodes;

            std::vector<NodeHandle> nodeHandles;
            std::vector<MeshHandle> meshHandles;
            std::vector<TextureHandle> textureHandles;
            std::vector<SamplerHandle> samplerHandles;
            std::vector<MaterialHandle> materialHandles;
            std::vector<LightHandle> lightHandles;
        };

    }

