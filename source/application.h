
#pragma once
#include "device/device.h"
#include "resources.h"
#include "pipelines/descriptors.h"

#include "scene/camera.h"

#include <chrono>
#include <glm/gtx/string_cast.hpp>

#include "glmdefines.h"

struct SceneData {
    vulkan::Frustum frustum;
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 cameraPosition;
    u32 drawCount;
};

struct ImGUIVariables {
    i32 selectedLight = 0;
    i32 numLights = 0;
    vulkan::Light* lights;
    char* lightNames = nullptr;
    bool lightsDirty = false;
};

struct PushConstants {
    glm::mat4 renderMatrix;
    vk::DeviceAddress vertexBuffer;
    vk::DeviceAddress materialBuffer;
    vk::DeviceAddress lightBuffer;
    vk::DeviceAddress indirectBuffer;
    vk::DeviceAddress surfaceBuffer;
    u32 numLights;
};

inline auto camera = vulkan::Camera(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), -90.0f, 0.0f);;
inline bool firstMouse = true;
inline f32 lastX = 400, lastY = 300;

void mouse_callback(GLFWwindow* window, f64 xposIn, f64 yposIn);
void process_scroll(GLFWwindow* window, double xoffset, double yoffset);
void process_input(GLFWwindow *window, f32 deltaTime, bool& mouseLook);

inline vk::PushConstantRange build_push_constant_ranges(const vk::ShaderStageFlags stageFlags)
{
    vk::PushConstantRange pcRange;
    pcRange.offset = 0;
    pcRange.size = sizeof(PushConstants);
    pcRange.stageFlags = stageFlags;
    return pcRange;
}

class Application {
public:
    explicit Application(vulkan::Device& device);
    ~Application();

    void run();
    void init();
    void update();
    void draw();
    void draw_imgui(const vulkan::GraphicsContext& graphicsContext, const vk::ImageView& imageView, const vk::Extent2D& extent);
    void imgui_light_info(const vulkan::GraphicsContext& graphicsContext);

private:
    void init_descriptors();
    void init_opaque_pipeline();
    void init_transparent_pipeline();
    void init_culling_pipeline();
    void init_scene_resources();
    void init_imgui();

    PushConstants build_push_constants(const vulkan::FrameData& frame);

    f32 deltaTime = 0.0f;
    f32 lastFrameTime = 0.0f;

    vulkan::Pipeline opaquePipeline;
    vulkan::Pipeline transparentPipeline;
    vulkan::Pipeline cullingPipeline;

    vulkan::Image drawImage;
    vulkan::Image depthImage;
    vk::Extent2D drawImageExtent;

    VkRenderingAttachmentInfo drawAttachment;
    VkRenderingAttachmentInfo depthAttachment;

private:
    vulkan::Buffer sceneDataBuffer{};
    PushConstants pushConstants;
    vk::PushConstantRange pushConstantRanges;

private:
    vulkan::Device& device;
    vulkan::SceneHandle testScene;

    std::unique_ptr<vulkan::DescriptorBuilder> descriptorBuilder;
    std::unique_ptr<vulkan::SceneManager> sceneManager;

private:
    ImGUIVariables imguiVariables;
};
