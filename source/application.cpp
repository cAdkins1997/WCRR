
#include "application.h"

#include "pipelines/descriptors.h"
#include "pipelines/pipelineBuilder.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

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

void process_input(GLFWwindow *window, const f32 deltaTime, u32& inputDelay, bool& mouseLook) {
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

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS &&  inputDelay == 0) {
        if (mouseLook) {
            mouseLook = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
            inputDelay += 120;
        }
        else {
            mouseLook = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            inputDelay += 120;
        }
    }

    if (inputDelay > 0) inputDelay--;
}

Application::Application(vulkan::Device& _device) : device(_device) {
    glfwMakeContextCurrent(device.get_window());
    glfwSetCursorPosCallback(device.get_window(), mouse_callback);
    glfwSetScrollCallback(device.get_window(),  process_scroll);
    glfwSetInputMode(device.get_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
        if (device.resizeRequested == true) {
            device.recreate_swapchain();
        }
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

    const auto currentSwapchainResult = device.get_swapchain_image();
    if (!currentSwapchainResult.has_value()) return;
    auto [currentSwapchainImage, currentSwapchainImageView] = currentSwapchainResult.value();

    vk::CommandBuffer& commandBuffer = currentFrame.commandBuffer;

    process_input(device.get_window(), deltaTime, inputDelay, camera.enableMouseLook);

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

    sceneManager->draw_scene(graphicsContext, testScene, sceneData.projection * sceneData.view);

    graphicsContext._commandBuffer.endRendering();

    graphicsContext.image_barrier(drawImage.handle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
    graphicsContext.image_barrier(currentSwapchainImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    graphicsContext.copy_image(drawImage.handle, currentSwapchainImage, drawImage.extent, device.get_swapchain_extent());
    graphicsContext.image_barrier(currentSwapchainImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
    draw_imgui(graphicsContext, currentSwapchainImageView, extent);
    graphicsContext.image_barrier(currentSwapchainImage, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
    graphicsContext.end();

    device.submit_graphics_work(graphicsContext, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eAllGraphics);

    device.present();
}

void Application::init_imgui() {
    device.init_imgui();
    imguiVariables.lights = sceneManager->get_lights();
    imguiVariables.lightNames = sceneManager->get_light_names().data();
    i32 numLights = sceneManager->get_num_lights();
}

void Application::draw_imgui(const vulkan::GraphicsContext& graphicsContext, const vk::ImageView& imageView, const vk::Extent2D& extent) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Scene Settings");

    imgui_light_info(graphicsContext);

    ImGui::End();

    ImGui::Render();

    VkRenderingAttachmentInfo ImGUIDrawImage {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    ImGUIDrawImage.imageView = imageView;
    ImGUIDrawImage.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ImGUIDrawImage.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    ImGUIDrawImage.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .pNext = nullptr};
    vk::Rect2D renderArea{};
    renderArea.extent = drawImageExtent;
    renderInfo.renderArea = renderArea;
    renderInfo.pColorAttachments = &ImGUIDrawImage;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.layerCount = 1;
    renderInfo.viewMask = 0;
    vkCmdBeginRendering(graphicsContext._commandBuffer, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), graphicsContext._commandBuffer);
    vkCmdEndRendering(graphicsContext._commandBuffer);
}

void Application::imgui_light_info(const vulkan::GraphicsContext& graphicsContext) {
    ImGui::BeginChild("Light Settings");
    ImGui::Text("Light Settings");

    auto label = "Lights";
    ImGui::Combo(label, &imguiVariables.selectedLight, imguiVariables.lightNames, imguiVariables.numLights);
    vulkan::Light* currentLight = &imguiVariables.lights[imguiVariables.selectedLight];

    if (ImGui::InputFloat3("Position", reinterpret_cast<float*>(&currentLight->position))) {
        imguiVariables.lightsDirty = true;
    }

    if (ImGui::ColorPicker3("Colour", reinterpret_cast<float*>(&currentLight->colour), ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float)) {
        imguiVariables.lightsDirty = true;
    }

    if (ImGui::DragFloat("Intensity", &currentLight->intensity, 0.001f, 0.0f, 1.0f)) {
        imguiVariables.lightsDirty = true;
    }


    if (imguiVariables.lightsDirty) {
        sceneManager->update_light_buffer();
        imguiVariables.lightsDirty = false;
    }

    ImGui::EndChild();
}

void Application::init() {
    init_scene_resources();
    init_descriptors();
    init_opaque_pipeline();
    init_imgui();
}

void Application::update() {
    sceneManager->update_nodes(glm::mat4(1.0f), testScene);
    sceneManager->update_light_buffer();
}

void Application::init_descriptors() {
    descriptorBuilder = std::make_unique<vulkan::DescriptorBuilder>(device);
    const auto globalSet = descriptorBuilder->build(opaquePipeline.setLayout);
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
    const vulkan::Shader vertShader = device.create_shader("../shaders/bin/glsl/meshbufferBDA.vert.spv");
    const vulkan::Shader fragShader = device.create_shader("../shaders/bin/glsl/pbr.frag.spv");

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
    const vulkan::Shader vertexShader = device.create_shader("../shaders/bin/glsl/meshbufferBDA.vert.spv");
    const vulkan::Shader fragShader = device.create_shader("../shaders/bin/glsl/transparency.frag.spv");

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

    if (auto gltf = sceneManager->load_gltf("../assets/scenes/sponza/NewSponza_Main_glTF_003.gltf"); gltf.has_value()) {
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
