
#include "application.h"

#include "pipelines/descriptors2.h"
#include "pipelines/pipelineBuilder.h"

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
    glfwSetInputMode(device.get_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    vulkan::UploadContext uploadContext(device.get_handle(), device.immediateCommandBuffer, device.get_allocator());
    sceneManager = std::make_unique<vulkan::SceneManager>(device, uploadContext);
    textureManager = std::make_unique<vulkan::TextureManager>(vulkan::TextureManager(device, uploadContext, 1));
    materialManager = std::make_unique<vulkan::MaterialManager>(vulkan::MaterialManager(device, 1));
    meshManager = std::make_unique<vulkan::MeshManager>(vulkan::MeshManager(device, 1));
    init();
    run();
}

Application::~Application() {
    const vk::Device& d = device.get_handle();
    d.waitIdle();
    d.destroyPipelineLayout(opaquePipeline.pipelineLayout);
    d.destroyPipeline(opaquePipeline.pipeline);
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

    device.wait_on_work();
    update();

    const u32 index = device.get_swapchain_image_index();
    const vk::Image& currentSwapchainImage = device.swapchainImages[index];

    const auto currentFrame = device.get_current_frame();
    const auto commandBuffer = currentFrame.commandBuffer;

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

    vulkan::UploadContext uploadContext(device.get_handle(), commandBuffer.get_handle(), device.get_allocator());
    uploadContext.begin();
    uploadContext.update_uniform(&sceneData, sizeof(SceneData), sceneDataBuffer);

    vulkan::GraphicsContext graphicsContext(commandBuffer);
    graphicsContext.image_barrier(drawHandle, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
    graphicsContext.image_barrier(depthHandle, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

    vk::Extent2D extent = {drawImage.extent.width, drawImage.extent.height};

    graphicsContext.set_up_render_pass(extent, &drawAttachment, &depthAttachment);
    graphicsContext.bind_pipeline(opaquePipeline);
    graphicsContext.set_viewport(extent, 0.0f, 1.0f);
    graphicsContext.set_scissor(extent);

    sceneManager->draw_scene(graphicsContext, *meshManager, *materialManager, testScene);

    graphicsContext.end_render_pass();

    graphicsContext.image_barrier(drawHandle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
    graphicsContext.image_barrier(currentSwapchainImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    graphicsContext.copy_image(drawImage.handle, currentSwapchainImage, drawImage.extent, device.get_swapchain_extent());
    graphicsContext.image_barrier(currentSwapchainImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
    graphicsContext.end();

    device.submit_graphics_work(graphicsContext, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eAllGraphics);

    device.present();
}

void Application::init() {
    init_scene_resources();
    init_descriptors();
    init_opaque_pipeline();
    init_transparent_pipeline();
}

void Application::update() const {
    sceneManager->update_nodes(glm::mat4(1.0f), testScene);
    sceneManager->update_lights();
    sceneManager->update_light_buffer();
}

void Application::init_descriptors() {
    /*std::vector<vulkan::descriptors::DescriptorAllocator::PoolSizeRatio> sizes {
        { vk::DescriptorType::eUniformBuffer, 128 },
        { vk::DescriptorType::eStorageBuffer, 128 },
        { vk::DescriptorType::eCombinedImageSampler, 128 }
    };

    descriptorAllocator.init(device.get_handle(), 10, sizes);

    {
        vulkan::descriptors::DescriptorLayoutBuilder builder;
        builder.add_binding(0, vk::DescriptorType::eUniformBuffer);
        builder.add_binding(1, vk::DescriptorType::eCombinedImageSampler);

        trianglePipeline.setLayout = builder.build(
            device.get_handle(),
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        );


        vulkan::descriptors::DescriptorWriter writer;
        writer.write_buffer(0, sceneDataBuffer.handle, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);

        textureManager->write_textures(writer, 1);

        trianglePipeline.set = descriptorAllocator.allocate(device.get_handle(), trianglePipeline.setLayout);

        writer.update_set(device.get_handle(), trianglePipeline.set);
    }*/

    auto deviceHandle = device.get_handle();
    DescriptorBuilder builder(deviceHandle);
    auto globalSet = builder.build(opaquePipeline.setLayout);
    opaquePipeline.set = globalSet;
    transparentPipeline.set = globalSet;
    builder.write_buffer(sceneDataBuffer.handle, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);

    textureManager->write_textures(builder);

    builder.update_set(opaquePipeline.set);

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
    pipelineBuilder.set_input_topology(vk::PrimitiveTopology::eTriangleList);
    pipelineBuilder.set_polygon_mode(vk::PolygonMode::eFill);

    pipelineBuilder.set_cull_mode(vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.enable_depthtest(vk::True, vk::CompareOp::eGreaterOrEqual);
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(drawImage.format);
    pipelineBuilder.set_depth_format(depthImage.format);
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(device.get_handle());

    device.get_handle().destroyShaderModule(vertShader.module);
    device.get_handle().destroyShaderModule(fragShader.module);

    drawAttachment.imageView = drawImage.view;
    drawAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    drawAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    drawAttachment.storeOp = vk::AttachmentStoreOp::eStore;

    depthAttachment.imageView = depthImage.view;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue.depthStencil.depth = 0.f;
}

void Application::init_transparent_pipeline() {
    const vulkan::Shader vertexShader = device.create_shader("../shaders/meshbufferBDA.vert.spv");
    const vulkan::Shader fragShader = device.create_shader("../shaders/transparency.frag.spv");

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.pipelineLayout = transparentPipeline.pipelineLayout;
    pipelineBuilder.set_shader(vertexShader.module, fragShader.module);
    pipelineBuilder.set_input_topology(vk::PrimitiveTopology::eTriangleList);
    pipelineBuilder.set_polygon_mode(vk::PolygonMode::eFill);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.enable_depthtest(vk::True, vk::CompareOp::eAlways);
    pipelineBuilder.enable_blending_alphablend();
    pipelineBuilder.set_color_attachment_format(drawImage.format);
    pipelineBuilder.set_depth_format(depthImage.format);
    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(device.get_handle());

    device.get_handle().destroyShaderModule(vertexShader.module);
    device.get_handle().destroyShaderModule(fragShader.module);

    drawAttachment.imageView = drawImage.view;
    drawAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    drawAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    drawAttachment.storeOp = vk::AttachmentStoreOp::eStore;

    depthAttachment.imageView = depthImage.view;
    depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue.depthStencil.depth = 0.f;
}

void Application::init_scene_resources() {
    vk_check(device.get_handle().resetFences(1, &device.immediateFence), "failed to reset immediate fence");
    drawImage = device.get_draw_image();
    drawHandle = drawImage.handle;
    drawImageExtent.height = drawImage.extent.height;
    drawImageExtent.width = drawImage.extent.width;

    depthImage = device.get_depth_image();
    depthHandle = depthImage.handle;

    sceneDataBuffer = device.create_buffer(
        sizeof(SceneData),
        vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vma::MemoryUsage::eAuto,
        vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
        vma::AllocationCreateFlagBits::eHostAccessAllowTransferInstead|
        vma::AllocationCreateFlagBits::eMapped
        );


    vulkan::UploadContext uploadContext(device.get_handle(), device.immediateCommandBuffer, device.get_allocator());
    uploadContext.begin();
    if (auto gltf = sceneManager->load_gltf("../assets/NewSponza_Main_glTF_003.gltf"); gltf.has_value()) {
        testScene = sceneManager->create_scene(gltf.value(), *meshManager, *textureManager, *materialManager);
        update();
    }

    SceneData sceneData;
    sceneData.view = camera.get_view_matrix();
    sceneData.projection = glm::perspective(
        glm::radians(camera.zoom),
        static_cast<float>(drawImage.extent.width) / static_cast<float>(drawImage.extent.height),
        10000.f,
        0.1f
        );
    sceneData.cameraPosition = glm::vec3(camera.Position.x, camera.Position.y, camera.Position.z);

    uploadContext.upload_uniform(&sceneData, sizeof(SceneData), sceneDataBuffer);
    uploadContext.end();
    device.submit_upload_work(uploadContext);
}
