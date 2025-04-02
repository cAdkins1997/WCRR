
#pragma once
#include "device/device.h"
#include "resources.h"
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

inline auto camera = vulkan::Camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, -1.0f, 0.0f),-90.0f, 0.0f);;
inline bool firstMouse = true;
inline f32 lastX = 400, lastY = 300;

void mouse_callback(GLFWwindow* window, f64 xposIn, f64 yposIn);
void process_scroll(GLFWwindow* window, double xoffset, double yoffset);
void process_input(GLFWwindow *window, f32 deltaTime);

class Application {
public:
    explicit Application(vulkan::Device& device);
    ~Application();

    void run();
    void init();
    void update() const;
    void draw();

private:
    void init_descriptors();
    void init_opaque_pipeline();
    void init_transparent_pipeline();
    void init_scene_resources();

    f32 deltaTime = 0.0f;
    f32 lastFrameTime = 0.0f;

    vulkan::Pipeline opaquePipeline;
    vulkan::Pipeline transparentPipeline;

    vulkan::Image drawImage;
    VkImage drawHandle{};
    vk::Extent2D drawImageExtent;

    vulkan::Image depthImage;
    VkImage depthHandle{};

    vk::RenderingAttachmentInfo drawAttachment;
    vk::RenderingAttachmentInfo depthAttachment;

private:
    vulkan::Buffer sceneDataBuffer{};

private:
    vulkan::Device& device;
    vulkan::SceneHandle testScene;

    std::unique_ptr<vulkan::SceneManager> sceneManager;
    std::unique_ptr<vulkan::TextureManager> textureManager;
    std::unique_ptr<vulkan::MaterialManager> materialManager;
    std::unique_ptr<vulkan::MeshManager> meshManager;
};
