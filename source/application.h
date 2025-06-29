
#pragma once
#include "device/device.h"
#include "rendererresources.h"
#include "pipelines/descriptors.h"

#include "scene/camera.h"

#include <chrono>
#include <glm/gtx/string_cast.hpp>

#include "glmdefines.h"

#include "pipelines/descriptors.h"

struct SceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 cameraPosition;
};

struct ImGUIVariables {
    i32 selectedLight = 0;
    i32 numLights = 0;
    vulkan::Light* lights;
    char* lightNames = nullptr;
    bool lightsDirty = false;
};

inline auto camera = vulkan::Camera(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), -90.0f, 0.0f);;
inline bool firstMouse = true;
inline f32 lastX = 400, lastY = 300;
inline u32 inputDelay = 0;

void mouse_callback(GLFWwindow* window, f64 xposIn, f64 yposIn);
void process_scroll(GLFWwindow* window, double xoffset, double yoffset);
void process_input(GLFWwindow *window, f32 deltaTime, bool& mouseLook);

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
    void init_scene_resources();
    void init_imgui();

    f32 deltaTime = 0.0f;
    f32 lastFrameTime = 0.0f;

    vulkan::Pipeline opaquePipeline;
    vulkan::Pipeline transparentPipeline;

    vulkan::Image drawImage;
    vulkan::Image depthImage;
    vk::Extent2D drawImageExtent;

    VkRenderingAttachmentInfo drawAttachment;
    VkRenderingAttachmentInfo depthAttachment;

private:
    vulkan::Buffer sceneDataBuffer{};

private:
    vulkan::Device& device;
    vulkan::SceneHandle testScene;

    std::unique_ptr<vulkan::DescriptorBuilder> descriptorBuilder;
    std::unique_ptr<vulkan::SceneManager> sceneManager;

private:
    ImGUIVariables imguiVariables;
};
