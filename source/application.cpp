
#include "application.h"

#include "pipelines/descriptors.h"
#include "pipelines/pipelineBuilder.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn) {
    auto xpos = static_cast<float>(xposIn);
    auto ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.process_mouse_movement(xoffset, yoffset, false);
}

void process_scroll(GLFWwindow *window, double xoffset, double yoffset) {
    camera.process_mouse_scroll(static_cast<float>(yoffset));
}

void process_input(GLFWwindow *window, f32 deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.process_keyboard(vulkan::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.process_keyboard(vulkan::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.process_keyboard(vulkan::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.process_keyboard(vulkan::RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        camera.process_keyboard(vulkan::UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        camera.process_keyboard(vulkan::DOWN, deltaTime);
}

Application::Application(vulkan::Device& _device) : device(_device) {
    glfwMakeContextCurrent(device.get_window());
    glfwSetCursorPosCallback(device.get_window(), mouse_callback);
    glfwSetScrollCallback(device.get_window(),  process_scroll);
    glfwSetInputMode(device.get_window(), GLFW_CURSOR, GLFW_CURSOR_CAPTURED);

    vulkan::UploadContext uploadContext(device.get_handle(), device.immediateCommandBuffer, device.get_allocator());
    sceneManager = std::make_unique<vulkan::SceneManager>(device, uploadContext);
    init();
    run();
}

Application::~Application() {
    vkDeviceWaitIdle(device.get_handle());
    sceneManager->release_gpu_resources();
    descriptorBuilder->release_descriptor_resources();
    device.get_handle().destroyPipeline(opaquePipeline.pipeline);
    device.get_handle().destroyPipelineLayout(opaquePipeline.pipelineLayout);
    device.get_handle().destroyDescriptorSetLayout(opaquePipeline.setLayout);
    vmaDestroyBuffer(device.get_allocator(), sceneDataBuffer.handle, sceneDataBuffer.allocation);
}

void Application::run() {
    while (!glfwWindowShouldClose(device.get_window())) {
        glfwPollEvents();
        draw();
    }
}

void Application::draw() {
    const auto currentFrameTime = static_cast<f32>(glfwGetTime());
    deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;

    update();
    device.wait_on_present();
    auto& currentFrame = device.get_current_frame();

    const u32 index = device.get_swapchain_image_index();
    const vk::Image& currentSwapchainImage = device.swapchainImages[index];
    const vk::ImageView& currentSwapchainImageView = device.swapchainImageViews[index];

    vk::CommandBuffer& commandBuffer = currentFrame.commandBuffer;

    process_input(device.get_window(), deltaTime);

    SceneData sceneData{};
    sceneData.view = camera.get_view_matrix();
    sceneData.projection = glm::perspective(
        glm::radians(camera.zoom),
        static_cast<float>(drawImage.extent.width) / static_cast<float>(drawImage.extent.height),
        10000.f,
        0.1f
        );
    sceneData.cameraPosition = glm::vec3(camera.Position.x, camera.Position.y, camera.Position.z);

    vulkan::UploadContext uploadContext(device.get_handle(), commandBuffer, device.get_allocator());
    uploadContext.begin();
    uploadContext.update_uniform(&sceneData, sizeof(SceneData), sceneDataBuffer);

    vulkan::GraphicsContext graphicsContext(commandBuffer);
    graphicsContext.image_barrier(drawImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
    graphicsContext.image_barrier(depthImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

    vk::Extent2D extent = {drawImage.extent.width, drawImage.extent.height};

    graphicsContext.set_up_render_pass(extent, &drawAttachment, &depthAttachment);
    graphicsContext.bind_pipeline(opaquePipeline);
    graphicsContext.set_viewport(extent, 0.0f, 1.0f);
    graphicsContext.set_scissor(extent);

    sceneManager->draw_scene(graphicsContext, testScene);

    graphicsContext._commandBuffer.endRendering();

    graphicsContext.image_barrier(drawImage.handle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
    graphicsContext.image_barrier(currentSwapchainImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    graphicsContext.copy_image(drawImage.handle, currentSwapchainImage, drawImage.extent, device.get_swapchain_extent());
    graphicsContext.image_barrier(currentSwapchainImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
    draw_imgui(graphicsContext, currentSwapchainImage, currentSwapchainImageView, extent);
    graphicsContext.image_barrier(currentSwapchainImage, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
    graphicsContext.end();

    device.submit_graphics_work(graphicsContext, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eAllGraphics);

    device.present();
}

void Application::init() {
    init_imgui();
    init_scene_resources();
    init_descriptors();
    init_opaque_pipeline();
}

void Application::update() const {
    sceneManager->update_nodes(glm::mat4(1.0f), testScene);
}

void Application::handle_dir_lights_imgui() {
    auto numDirLights = sceneManager->get_num_dirlights();
    auto dirLights = sceneManager->get_dir_lights();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Directional Lights");
    ImGui::BeginChild("Scrolling");
    ImGui::InputInt("Selected directional light", &directionalLightIndex);
    if (directionalLightIndex >= numDirLights - 1)
        directionalLightIndex = numDirLights - 1;

    auto* selectedLight = &dirLights[directionalLightIndex];
    ImGui::Text("Directional Light Position");
    if (ImGui::InputFloat("Directional Light X", &selectedLight->direction.x))
        dirLightsDirty = true;
    if (ImGui::InputFloat("Directional Light Y", &selectedLight->direction.y))
        dirLightsDirty = true;
    if (ImGui::InputFloat("Directional Light Z", &selectedLight->direction.z))
        dirLightsDirty = true;
    ImGui::Text("Directional Light Color");
    if (ImGui::ColorEdit3("Directional Light Colour", reinterpret_cast<float*>(&selectedLight->colour)))
        dirLightsDirty = true;
    if (dirLightsDirty) {
        sceneManager->update_light_buffers();
        dirLightsDirty = false;
    }

    ImGui::EndChild();
}

void Application::handle_point_lights_imgui() {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Point Lights");
    ImGui::BeginChild("Scrolling");
    ImGui::InputInt("Selected Point light", &pointLightIndex);

    auto numPointLights = sceneManager->pointLights.size();
    if (pointLightIndex >= numPointLights - 1)
        pointLightIndex = numPointLights - 1;

    std::vector<vulkan::GPUPointLight>& pointLights = sceneManager->pointLights;
    vulkan::GPUPointLight* selectedLight = &pointLights[pointLightIndex];
    ImGui::Text("Point Light Position");
    if (ImGui::InputFloat("Point Light X", &selectedLight->position.x))
        pointLightsDirty = true;
    if (ImGui::InputFloat("Point Light Y", &selectedLight->position.y))
        pointLightsDirty = true;
    if (ImGui::InputFloat("Point Light Z", &selectedLight->position.z))
        pointLightsDirty = true;
    ImGui::Text("Point Light Color");
    if (ImGui::ColorEdit3("Point Light Colour", reinterpret_cast<float*>(&selectedLight->colour)))
        pointLightsDirty = true;
    ImGui::Text("Point Light Intensity");
    if (ImGui::DragFloat("Point Light Intensity", &selectedLight->intensity, 0.01f, 0.0f, 1.0f))
        pointLightsDirty = true;
    ImGui::Text("Point Light Quadratic");
    if (ImGui::DragFloat("Point Light Quadratic", &selectedLight->quadratic, 0.01f, 0.0f, 1.0f))
        pointLightsDirty = true;
    if (pointLightsDirty) {
        sceneManager->update_light_buffers();
        pointLightsDirty = false;
    }
    ImGui::EndChild();

}

void Application::handle_spot_lights_imgui() {
    auto numSpotLights = sceneManager->get_num_spotlights();
    auto spotLights = sceneManager->get_spot_lights();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Spot Lights");
    ImGui::BeginChild("Scrolling");
    ImGui::InputInt("Selected Spot light", &spotLightIndex);
    if (spotLightIndex >= numSpotLights - 1)
        spotLightIndex = numSpotLights - 1;

    auto* selectedLight = &spotLights[spotLightIndex];
    if (selectedLight != nullptr) {
        ImGui::Text("Spot Light Position");
        if (ImGui::InputFloat("Spot Light i", &selectedLight->position.x))
            spotLightsDirty = true;
        if (ImGui::InputFloat("Spot Light j", &selectedLight->position.y))
            spotLightsDirty = true;
        if (ImGui::InputFloat("Spot Light k", &selectedLight->position.z))
            spotLightsDirty = true;
        ImGui::Text("Spot Light Direction");
        if (ImGui::InputFloat("Spot Light X", &selectedLight->direction.x))
            spotLightsDirty = true;
        if (ImGui::InputFloat("Spot Light Y", &selectedLight->direction.y))
            spotLightsDirty = true;
        if (ImGui::InputFloat("Spot Light Z", &selectedLight->direction.z))
            spotLightsDirty = true;
        ImGui::Text("Color");
        if (ImGui::ColorEdit3("Spot Light Colour", reinterpret_cast<float*>(&selectedLight->colour)))
            pointLightsDirty = true;
        ImGui::Text("Spot Light Intensity");
        if (ImGui::DragFloat("Spot Light Intensity", &selectedLight->intensity, 0.01f, 0.0f, 1.0f))
            pointLightsDirty = true;
        ImGui::Text("Spot Light Quadratic");
        if (ImGui::DragFloat("Spot Light Quadratic", &selectedLight->quadratic, 0.01f, 0.0f, 1.0f))
            pointLightsDirty = true;
        ImGui::Text("Spot Light Inner Angle");
        if (ImGui::DragFloat("Spot Light Inner Angle", &selectedLight->innerAngle, 0.01f, 0.0f, 360.0f))
            pointLightsDirty = true;
        ImGui::Text("Spot Light Outer Angle");
        if (ImGui::DragFloat("Spot Light Outer Angle", &selectedLight->outerAngle, 0.01f, 0.0f, 360.0f))
            pointLightsDirty = true;
        if (pointLightsDirty) {
            sceneManager->update_light_buffers();
            pointLightsDirty = false;
        }
    }

    ImGui::EndChild();
}

void Application::draw_imgui(
    const vulkan::GraphicsContext& graphicsContext,
    const vk::Image& swapchainImage,
    const vk::ImageView& swapchainImageView,
    vk::Extent2D& swapchainExtent) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("WCGL Test Renderer Settings");

    ImGui::Text("Light Settings");

    handle_dir_lights_imgui();
    handle_point_lights_imgui();
    handle_spot_lights_imgui();

    ImGui::End();

    ImGui::Render();

    VkRenderingAttachmentInfo ImGUIDrawImage {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    ImGUIDrawImage.imageView = swapchainImageView;
    ImGUIDrawImage.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ImGUIDrawImage.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    ImGUIDrawImage.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .pNext = nullptr};
    vk::Rect2D renderArea{};
    renderArea.extent.height = swapchainExtent.height;
    renderArea.extent.width = swapchainExtent.width;
    renderInfo.renderArea = renderArea;
    renderInfo.pColorAttachments = &ImGUIDrawImage;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.layerCount = 1;
    renderInfo.viewMask = 0;
    vkCmdBeginRendering(graphicsContext._commandBuffer, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), graphicsContext._commandBuffer);
    vkCmdEndRendering(graphicsContext._commandBuffer);
}

void Application::init_descriptors() {
    descriptorBuilder = std::make_unique<vulkan::DescriptorBuilder>(device);
    auto globalSet = descriptorBuilder->build(opaquePipeline.setLayout);
    opaquePipeline.set = globalSet;
    transparentPipeline.set = globalSet;
    descriptorBuilder->write_buffer(sceneDataBuffer.handle, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);

    sceneManager->write_textures(*descriptorBuilder);

    descriptorBuilder->update_set(opaquePipeline.set);

    vk::PushConstantRange pcRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(vulkan::PushConstants));
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;

    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &opaquePipeline.setLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pcRange;

    const auto globalPipelineLayout = device.get_handle().createPipelineLayout(pipelineLayoutInfo, nullptr);
    opaquePipeline.pipelineLayout = globalPipelineLayout;
    transparentPipeline.pipelineLayout = globalPipelineLayout;
}

void Application::init_opaque_pipeline() {
    const vulkan::Shader vertShader = device.create_shader("../shaders/meshbufferBDA.vert.spv");
    const vulkan::Shader fragShader = device.create_shader("../shaders/pbr.frag.spv");

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.pipelineLayout = opaquePipeline.pipelineLayout;
    pipelineBuilder.set_shader(vertShader.module, fragShader.module);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);

    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.enable_depthtest(vk::True, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(drawImage.format);
    pipelineBuilder.set_depth_format(depthImage.format);
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(device);

    device.get_handle().destroyShaderModule(vertShader.module);
    device.get_handle().destroyShaderModule(fragShader.module);

    drawAttachment = VkRenderingAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    drawAttachment.imageView = drawImage.view;
    drawAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    drawAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    drawAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    depthAttachment = VkRenderingAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    depthAttachment.imageView = depthImage.view;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil.depth = 0.f;
}

void Application::init_transparent_pipeline() {
    const vulkan::Shader vertexShader = device.create_shader("../shaders/meshbufferBDA.vert.spv");
    const vulkan::Shader fragShader = device.create_shader("../shaders/transparency.frag.spv");

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.pipelineLayout = transparentPipeline.pipelineLayout;
    pipelineBuilder.set_shader(vertexShader.module, fragShader.module);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.enable_depthtest(vk::True, VK_COMPARE_OP_ALWAYS);
    pipelineBuilder.enable_blending_alphablend();
    pipelineBuilder.set_color_attachment_format(drawImage.format);
    pipelineBuilder.set_depth_format(depthImage.format);
    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(device);

    device.get_handle().destroyShaderModule(vertexShader.module);
    device.get_handle().destroyShaderModule(fragShader.module);
}

void Application::init_raytracing_pipeline() {

    vulkan::Shader raygenShader;
    vulkan::Shader rayClosestHitShader;
    vulkan::Shader rayMissShader;


    const std::vector<vk::PipelineShaderStageCreateInfo> stageInfos {
        {{}, vk::ShaderStageFlagBits::eRaygenKHR, raygenShader.module, "main"},
        {{}, vk::ShaderStageFlagBits::eClosestHitKHR, rayClosestHitShader.module, "main"},
        {{}, vk::ShaderStageFlagBits::eMissKHR, rayMissShader.module, "main"}
    };

    vk::RayTracingPipelineCreateInfoKHR raytracingPipelineCI;
    raytracingPipelineCI.pStages = stageInfos.data();
    raytracingPipelineCI.stageCount = static_cast<uint32_t>(stageInfos.size());
    //raytracingPipelineCI.pGroups = nullptr;
}

void Application::init_imgui() const {
    vk::DescriptorPoolSize poolSizes[] = {
            { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 }
        };

        vk::DescriptorPoolCreateInfo poolCI;
        poolCI.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        poolCI.maxSets = 1000;
        poolCI.poolSizeCount = static_cast<u32>(std::size(poolSizes));
        poolCI.pPoolSizes = poolSizes;

        vk::DescriptorPool imguiPool;
        vk_check(
            device.get_handle().createDescriptorPool(&poolCI, nullptr, &imguiPool),
            "Failed to create descriptor pool"
        );

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(device.get_window(), true);
        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.Instance = device.get_instance();
        initInfo.PhysicalDevice = device.get_gpu();
        initInfo.Device = device.get_handle();
        initInfo.Queue = device.get_graphics_queue();
        initInfo.DescriptorPool = imguiPool;
        initInfo.MinImageCount = 3;
        initInfo.ImageCount = 3;
        initInfo.UseDynamicRendering = true;

        initInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        constexpr VkFormat colorAttachFormat = VK_FORMAT_B8G8R8A8_SRGB;
        initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorAttachFormat;
        initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ImGui_ImplVulkan_Init(&initInfo);

        ImGui_ImplVulkan_CreateFontsTexture();

        device.deviceDeletionQueue.push_lambda([&] {
            ImGui_ImplVulkan_Shutdown();
            device.get_handle().destroyDescriptorPool(imguiPool);
        });
}

void Application::init_scene_resources() {
    vk_check(device.get_handle().resetFences(1, &device.immediateFence), "failed to reset immediate fence");
    drawImage = device.get_draw_image();
    drawImageExtent.height = drawImage.extent.height;
    drawImageExtent.width = drawImage.extent.width;

    depthImage = device.get_depth_image();

    sceneDataBuffer = device.create_buffer(
        sizeof(SceneData),
        vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT
        );

    if (auto gltf = sceneManager->load_gltf("../assets/NewSponza_Main_glTF_003.gltf"); gltf.has_value()) {
        testScene = sceneManager->create_scene(gltf.value());
        update();
    }

    vulkan::UploadContext uploadContext(device.get_handle(), device.immediateCommandBuffer, device.get_allocator());

    SceneData sceneData{};
    sceneData.view = camera.get_view_matrix();
    sceneData.projection = glm::perspective(
        glm::radians(camera.zoom),
        static_cast<float>(drawImage.extent.width) / static_cast<float>(drawImage.extent.height),
        10000.f,
        0.1f
        );
    sceneData.cameraPosition = glm::vec3(camera.Position.x, camera.Position.y, camera.Position.z);

    device.submit_immediate_work(
        [&](vk::CommandBuffer commandBuffer) {
            uploadContext.upload_uniform(&sceneData, sizeof(SceneData), sceneDataBuffer);
        }
    );
}
