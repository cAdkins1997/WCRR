#include "device.h"


namespace vulkan {

    void frame_buffer_resize_callback(GLFWwindow *window, i32 width, i32 height) {
        const auto result = static_cast<Device*>(glfwGetWindowUserPointer(window));
        result->resizeRequested = true;
    }

    Device::Device(eastl::string_view appName, const u32 _width, const u32 _height, u32 countFramesInFlight)
    : width(_width), height(_height), MAX_FRAMES_IN_FLIGHT(countFramesInFlight) {

        init_window();
        init_instance();
        init_debug_messenger();
        init_surface();
        select_gpu();
        init_device();
        init_swapchain();
        init_allocator();
        init_commands();
        init_sync_objects();
        init_draw_images();
        init_depth_images();
    }

    Device::~Device() {
    }

    Buffer Device::create_buffer(
        size_t allocationSize,
        vk::BufferUsageFlags usage,
        vma::MemoryUsage memoryUsage,
        vma::AllocationCreateFlags flags) const {

        vk::BufferCreateInfo bufferInfo;
        bufferInfo.pNext = nullptr;
        bufferInfo.size = allocationSize;
        bufferInfo.usage = usage;

        vma::AllocationCreateInfo vmaallocInfo;
        vmaallocInfo.usage = memoryUsage;
        vmaallocInfo.flags = flags;

        Buffer newBuffer{};
        vk_check(
            allocator.createBuffer(&bufferInfo, &vmaallocInfo, &newBuffer.handle, &newBuffer.allocation, &newBuffer.info),
            "Failed to create buffer"
        );
        return newBuffer;
    }

    Image Device::create_image(vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, u32 mipLevels, bool mipmapped) const {
        Image newImage{};
        newImage.format = format;
        newImage.extent = size;

        vk::ImageCreateInfo imageCI;
        imageCI.format = format;
        imageCI.usage = usage;
        imageCI.extent = size;
        imageCI.arrayLayers = 1;
        imageCI.imageType = vk::ImageType::e2D;
        imageCI.tiling = vk::ImageTiling::eOptimal;
        imageCI.sharingMode = vk::SharingMode::eExclusive;
        imageCI.mipLevels = mipLevels;
        imageCI.imageType = vk::ImageType::e2D;

        if (mipmapped) {
            imageCI.mipLevels = mipLevels;
        }
        else
            imageCI.mipLevels = 1;

        vma::AllocationCreateInfo allocInfo{};
        allocInfo.usage = vma::MemoryUsage::eGpuOnly;
        allocInfo.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;

        vk_check(
            allocator.createImage(&imageCI, &allocInfo, &newImage.handle, &newImage.allocation, nullptr),
            "Failed to create image"
        );

        vk::ImageAspectFlags aspectFlag = vk::ImageAspectFlagBits::eColor;
        if (format  == vk::Format::eD32Sfloat)
            aspectFlag = vk::ImageAspectFlagBits::eDepth;

        vk::ComponentMapping components {
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity
        };

        vk::ImageViewCreateInfo viewInfo({}, newImage.handle, vk::ImageViewType::e2D, format, components, aspectFlag);
        viewInfo.subresourceRange.aspectMask = aspectFlag;
        viewInfo.subresourceRange.levelCount = imageCI.mipLevels;
        viewInfo.subresourceRange.layerCount = 1;

        newImage.view = handle.createImageView(viewInfo, nullptr);

        return newImage;
    }

    Sampler Device::create_sampler(const vk::Filter minFilter, const vk::Filter magFilter, const vk::SamplerMipmapMode mipmapMode) const {

        vk::SamplerCreateInfo samplerCI;
        samplerCI.minFilter = minFilter;
        samplerCI.magFilter = magFilter;
        samplerCI.mipmapMode = mipmapMode;
        vk::Sampler newSampler;

        vk_check(
            handle.createSampler(&samplerCI, nullptr, &newSampler),
            "failed to create sampler"
            );

        return Sampler{magFilter, minFilter, newSampler};
    }

    Shader Device::create_shader(eastl::string_view filePath) const {
        std::ifstream file(filePath.data(), std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to find file!\n");
        }

        u64 fileSize = file.tellg();

        std::vector<u32> buffer(fileSize / sizeof(u32));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        vk::ShaderModuleCreateInfo shaderCI;
        shaderCI.codeSize = buffer.size() * sizeof(uint32_t);
        shaderCI.pCode = buffer.data();

        vk::ShaderModule shaderModule;
        vk_check(
            handle.createShaderModule(&shaderCI, nullptr, &shaderModule),
            "Failed to create shader module"
            );

        Shader shader {shaderModule};
        shader.path = filePath.data();
        return shader;
    }

    void Device::submit_graphics_work(
        const GraphicsContext &context,
        const vk::PipelineStageFlagBits2 wait,
        const vk::PipelineStageFlagBits2 signal) {

        const vk::CommandBuffer cmd = context._commandBuffer.get_handle();
        const vk::CommandBufferSubmitInfo commandBufferSI(cmd);

        vk::SemaphoreSubmitInfo waitInfo(get_current_frame().swapchainSemaphore);
        waitInfo.stageMask = wait;
        vk::SemaphoreSubmitInfo signalInfo(get_current_frame().renderSemaphore);
        signalInfo.stageMask = signal;
        const vk::SubmitFlagBits submitFlags{};
        const vk::SubmitInfo2 submitInfo(
            submitFlags,
            1, &waitInfo,
            1, &commandBufferSI,
            1, &signalInfo);

        vk_check(graphicsQueue.submit2(1, &submitInfo, get_current_frame().renderFence), "Failed to submit graphics commands");
    }

    void Device::submit_compute_work(
        const ComputeContext &context,
        const vk::PipelineStageFlagBits2 wait,
        const vk::PipelineStageFlagBits2 signal) {

        const vk::CommandBuffer cmd = context._commandBuffer.get_handle();
        const vk::CommandBufferSubmitInfo commandBufferSI(cmd);

        vk::SemaphoreSubmitInfo waitInfo(get_current_frame().swapchainSemaphore);
        waitInfo.stageMask = wait;
        vk::SemaphoreSubmitInfo signalInfo(get_current_frame().renderSemaphore);
        signalInfo.stageMask = signal;
        constexpr vk::SubmitFlagBits submitFlags{};
        const vk::SubmitInfo2 submitInfo(submitFlags, 1, &waitInfo, 1, &commandBufferSI, 1, &signalInfo);

        vk_check(graphicsQueue.submit2(1, &submitInfo, get_current_frame().renderFence), "Failed to submit compute commands. Error: ");
    }

    void Device::submit_upload_work(const UploadContext &context, vk::PipelineStageFlagBits2 wait, vk::PipelineStageFlagBits2 signal) const {
        const vk::CommandBuffer cmd = context._commandBuffer ;
        vk::CommandBufferSubmitInfo commandBufferSI(cmd);
        const vk::SubmitInfo2 submitInfo({}, nullptr, commandBufferSI);
        vk_check(
            graphicsQueue.submit2(1, &submitInfo, immediateFence),
            "Failed to submit immediate upload commands"
            );

        vk_check(
            handle.waitForFences(1, &immediateFence, true, UINT64_MAX),
            "Failed to wait for fences"
            );
    }

    void Device::submit_upload_work(const UploadContext &context) const {
        const vk::CommandBuffer cmd = context._commandBuffer ;
        vk::CommandBufferSubmitInfo commandBufferSI(cmd);
        const vk::SubmitInfo2 submitInfo({}, nullptr, commandBufferSI);
        vk_check(
            graphicsQueue.submit2(1, &submitInfo, immediateFence),
            "Failed to submit immediate upload commands"
            );

        vk_check(
            handle.waitForFences(1, &immediateFence, true, UINT64_MAX),
            "Failed to wait for fences"
            );
    }

    void Device::submit_immediate_work(std::function<void(vk::CommandBuffer)>&& function) const {
        vk_check(handle.resetFences(1, &immediateFence), "Failed to reset fences");
        immediateCommandBuffer.reset();

        constexpr vk::CommandBufferBeginInfo commandBufferBI(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk_check(immediateCommandBuffer.begin(&commandBufferBI), "Failed to begin command buffer");

        function(immediateCommandBuffer);

        immediateCommandBuffer.end();

        vk::CommandBufferSubmitInfo commandBufferSI(immediateCommandBuffer);
        vk::SubmitInfo2 submitInfo({}, nullptr, nullptr);
        submitInfo.pCommandBufferInfos = &commandBufferSI;
        vk_check(
            graphicsQueue.submit2(1, &submitInfo, immediateFence),
            "Failed to submit immediate upload commands"
            );

        vk_check(
            handle.waitForFences(1, &immediateFence, true, UINT64_MAX),
            "Failed to wait for fences"
            );
    }

    void Device::wait_on_work() {
        vk_check(handle.waitForFences(1, &get_current_frame().renderFence, true, UINT64_MAX), "Failed to wait for fences");
        vk_check(handle.resetFences(1, &get_current_frame().renderFence), "Failed to reset fences");
    }

    void Device::present() {
        const vk::PresentInfoKHR presentInfo(1, &get_current_frame().renderSemaphore, 1, &swapchain, &swapchainImageIndex);
        vk_check(
            graphicsQueue.presentKHR(&presentInfo),
            "Failed to present");
        frameNumber++;
    }

    vk::Image& Device::get_swapchain_image() {
        return swapchainImages[get_swapchain_image_index()];
    }

    u32 Device::get_swapchain_image_index() {

        vk_check(
            handle.acquireNextImageKHR(swapchain, UINT32_MAX, get_current_frame().swapchainSemaphore, nullptr, &swapchainImageIndex),
            "Failed to acquire next image"
            );

        return swapchainImageIndex;
    }

    void Device::init_commands() {
        vk::CommandPoolCreateInfo commandPoolCI;
        commandPoolCI.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        commandPoolCI.queueFamilyIndex = graphicsQueueIndex;

        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk_check(
                handle.createCommandPool(&commandPoolCI, nullptr, &frames[i].commandPool),
                "Failed to create command pool"
            );

            vk::CommandBufferAllocateInfo allocInfo;
            allocInfo.commandPool = frames[i].commandPool;
            allocInfo.commandBufferCount = 1;
            allocInfo.level = vk::CommandBufferLevel::ePrimary;

            vk_check(
                handle.allocateCommandBuffers(&allocInfo, &frames[i].commandBuffer.handle),
                "Failed to allocate command buffers"
            );
        }

        vk_check(
            handle.createCommandPool(&commandPoolCI, nullptr, &immediateCommandPool),
            "Failed to create immediate command pool"
        );

        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = immediateCommandPool;
        allocInfo.commandBufferCount = 1;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;

        vk_check(
            handle.allocateCommandBuffers(&allocInfo, &immediateCommandBuffer),
            "Failed to allocate immediate command buffer"
        );
    }

    void Device::init_sync_objects() {
        constexpr vk::FenceCreateInfo fenceCI(vk::FenceCreateFlagBits::eSignaled);
        constexpr vk::SemaphoreCreateInfo semaphoreCI;

        for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk_check(
                handle.createFence(&fenceCI, nullptr, &frames[i].renderFence),
                "Failed to create fence"
            );

            vk_check(
                handle.createSemaphore(&semaphoreCI, nullptr, &frames[i].swapchainSemaphore),
                "Failed to create semaphore"
            );

            vk_check(
                handle.createSemaphore(&semaphoreCI, nullptr, &frames[i].renderSemaphore),
                "Failed to create semaphore"
                );
        }

        vk_check(handle.createFence(&fenceCI, nullptr, &immediateFence), "Failed to create immediate fence");
    }

    void Device::init_allocator() {
        vma::AllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = vk::ApiVersion13;
        allocatorInfo.physicalDevice = gpu;
        allocatorInfo.device = handle;
        allocatorInfo.instance = instance;
        allocatorInfo.flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress;
        vk_check(
            vma::createAllocator(&allocatorInfo, &allocator),
            "Failed to create allocator"
            );
    }

    void Device::init_descriptors() {
    }

    void Device::init_draw_images() {
        const vk::Extent3D drawImageExtent {width, height, 1};
        drawImage.format = vk::Format::eR32G32B32A32Sfloat;
        drawImage.extent = drawImageExtent;

        vk::ImageUsageFlags drawImageUsages;
        drawImageUsages |= vk::ImageUsageFlagBits::eTransferSrc;
        drawImageUsages |= vk::ImageUsageFlagBits::eTransferDst;
        drawImageUsages |= vk::ImageUsageFlagBits::eStorage;
        drawImageUsages |= vk::ImageUsageFlagBits::eColorAttachment;

        vk::ImageCreateInfo imageCI;
        imageCI.format = drawImage.format;
        imageCI.usage = drawImageUsages;
        imageCI.extent = drawImageExtent;
        imageCI.imageType = vk::ImageType::e2D;
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;

        vma::AllocationCreateInfo imageAI;
        imageAI.usage = vma::MemoryUsage::eGpuOnly;
        imageAI.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;

        vk_check(
            allocator.createImage(&imageCI, &imageAI, &drawImage.handle,  &drawImage.allocation, nullptr),
            "Failed to create image"
        );

        constexpr vk::ComponentMapping components {
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity
        };

        vk::ImageSubresourceRange subresourceRange;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;
        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;

        vk::ImageViewCreateInfo imageViewCI;
        imageViewCI.image = drawImage.handle;
        imageViewCI.viewType = vk::ImageViewType::e2D;
        imageViewCI.format = drawImage.format;
        imageViewCI.components = components;
        imageViewCI.subresourceRange = subresourceRange;

        vk_check(
            handle.createImageView(&imageViewCI, nullptr, &drawImage.view),
            "Failed to create image view"
        );
    }

    void Device::init_depth_images() {
        const vk::Extent3D depthImageExtent = { width, height, 1};
        depthImage.format = vk::Format::eD32Sfloat;
        depthImage.extent = depthImageExtent;

        constexpr vk::ImageUsageFlags depthImageUsages = vk::ImageUsageFlagBits::eDepthStencilAttachment;

        vk::ImageCreateInfo imageCI;
        imageCI.format = depthImage.format;
        imageCI.usage = depthImageUsages;
        imageCI.extent = depthImageExtent;
        imageCI.imageType = vk::ImageType::e2D;
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;

        vma::AllocationCreateInfo allocationCI;
        allocationCI.usage = vma::MemoryUsage::eGpuOnly;
        allocationCI.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;

        vk_check(
            allocator.createImage(&imageCI, &allocationCI, &depthImage.handle, &depthImage.allocation, nullptr),
            "Failed to create image"
        );

        vk::ComponentMapping components {
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity
        };

        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;
        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        vk::ImageViewCreateInfo imageViewCI;
        imageViewCI.image = depthImage.handle;
        imageViewCI.viewType = vk::ImageViewType::e2D;
        imageViewCI.format = vk::Format::eD32Sfloat;
        imageViewCI.components = components;
        imageViewCI.subresourceRange = subresourceRange;

        vk_check(
            handle.createImageView(&imageViewCI, nullptr, &depthImage.view),
            "Failed to create image view"
        );
    }

    void Device::init_instance() {
        vk::ApplicationInfo appinfo;
        appinfo.pApplicationName = applicationName.data();
        appinfo.pEngineName = "Engine";
        appinfo.engineVersion = VK_API_VERSION_1_3;
        appinfo.apiVersion = VK_API_VERSION_1_3;

        vk::InstanceCreateInfo instanceCI;
        instanceCI.pApplicationInfo = &appinfo;

        auto extensions = get_required_extensions();
        instanceCI.enabledExtensionCount = static_cast<u32>(extensions.size());
        instanceCI.ppEnabledExtensionNames = extensions.data();

        vk::DebugUtilsMessengerCreateInfoEXT debugCI;

        if (enableValidationLayers) {
            instanceCI.enabledLayerCount = static_cast<u32>(validationLayers.size());
            instanceCI.ppEnabledLayerNames = validationLayers.data();

            populate_debug_messenger_CI(debugCI);
        }

        vk_check(
    createInstance(&instanceCI, nullptr, &instance),
    "Failed to create instance"
        );
    }

    void Device::init_debug_messenger() {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessengerCreateInfoEXT messengerCI;
        populate_debug_messenger_CI(messengerCI);

        vk_check(
            CreateDebugUtilsMessengerEXT(
                instance,
                (VkDebugUtilsMessengerCreateInfoEXT*)&messengerCI,
                nullptr,
                (VkDebugUtilsMessengerEXT*)&debugMessenger),
            "Failed to set up debug messenger"
        );
    }

    void Device::init_window() {
        glfwInit();

        if (glfwVulkanSupported()) {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            window = glfwCreateWindow(width, height, applicationName.data(), nullptr, nullptr);
        }
    }

    void Device::init_surface() {
        vk_check(glfwCreateWindowSurface(instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)),
            "Failed to create window surface"
        );
    }

    void Device::select_gpu() {
        u32 gpuCount = 0;
        vk_check(
            instance.enumeratePhysicalDevices(&gpuCount, nullptr),
            "Failed to enumerate physical devices"
            );

        if (gpuCount == 0) throw std::runtime_error("Failed to find suitable GPU");

        eastl::vector<vk::PhysicalDevice> gpus(gpuCount);

        vk_check(
            instance.enumeratePhysicalDevices(&gpuCount, gpus.data()),
            "Failed to enumerate physical devices"
            );

        for (const auto& candidate : gpus) {
            if (gpu_is_suitable(candidate)) {
                gpu = candidate;
                break;
            }
        }

        if (gpu == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU");
        }
    }

    void Device::init_device() {
        QueueFamilyIndices indices = find_queue_families(gpu);

        eastl::vector<vk::DeviceQueueCreateInfo> queueCIs;
        eastl::set<u32> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        f32 queuePriority = 1.0f;
        for (u32 queueFamily : uniqueQueueFamilies) {
            vk::DeviceQueueCreateInfo queueCI;
            queueCI.queueFamilyIndex = queueFamily;
            queueCI.queueCount = 1;
            queueCI.pQueuePriorities = &queuePriority;
            queueCIs.push_back(queueCI);
        }

        vk::PhysicalDeviceFeatures2 deviceFeatures;
        deviceFeatures = gpu.getFeatures2();

        vk::PhysicalDeviceBufferDeviceAddressFeatures bdaFeatures;
        bdaFeatures.setBufferDeviceAddress(true);
        deviceFeatures.pNext = &bdaFeatures;

        vk::PhysicalDeviceSynchronization2Features sync2Features;
        sync2Features.synchronization2 = true;
        bdaFeatures.pNext = &sync2Features;

        vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures;
        dynamicRenderingFeatures.dynamicRendering = true;
        sync2Features.pNext = &dynamicRenderingFeatures;

        vk::PhysicalDeviceDescriptorIndexingFeatures descIndexingFeatures;
        descIndexingFeatures.runtimeDescriptorArray = true;
        descIndexingFeatures.descriptorBindingPartiallyBound = true;
        descIndexingFeatures.runtimeDescriptorArray = true;
        descIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = true;
        descIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = true;
        descIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind = true;
        descIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = true;
        descIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = true;
        descIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind = true;
        descIndexingFeatures.descriptorBindingVariableDescriptorCount = true;
        dynamicRenderingFeatures.pNext = &descIndexingFeatures;

        vk::PhysicalDeviceRobustness2FeaturesEXT robustnessFeaturesEXT;
        robustnessFeaturesEXT.robustBufferAccess2 = true;
        descIndexingFeatures.pNext = &robustnessFeaturesEXT;

        /*vk::PhysicalDeviceDescriptorBufferFeaturesEXT descBufferFeatures;
        descBufferFeatures.descriptorBuffer = true;
        descIndexingFeatures.pNext = &descBufferFeatures;*/

        vk::DeviceCreateInfo deviceCI;
        deviceCI.pNext = &deviceFeatures;
        deviceCI.queueCreateInfoCount = static_cast<u32>(queueCIs.size());
        deviceCI.pQueueCreateInfos = queueCIs.data();
        deviceCI.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
        deviceCI.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            deviceCI.enabledLayerCount = static_cast<u32>(validationLayers.size());
            deviceCI.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            deviceCI.enabledLayerCount = 0;
        }

        handle = gpu.createDevice(deviceCI, nullptr);

        graphicsQueue = handle.getQueue(indices.graphicsFamily.value(), graphicsQueueIndex);
        computeQueue = handle.getQueue(indices.computeFamily.value(), computeQueueIndex);
        presentQueue = handle.getQueue(indices.presentFamily.value(), presentQueueIndex);
        transferQueue = handle.getQueue(indices.transferFamily.value(), transferQueueIndex);
    }

    void Device::init_swapchain() {
        SwapChainSupportDetails support = query_swapchain_support(gpu);

        vk::SurfaceFormatKHR surfaceFormat = choose_swap_surface_format(support.formats);
        vk::PresentModeKHR presentMode = choose_swap_present_mode(support.presentModes);
        vk::Extent2D extent = choose_swap_extent(support.capabilities);

        u32 imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
            imageCount = support.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR swapchainCI;
        swapchainCI.surface = surface;
        swapchainCI.minImageCount = imageCount;
        swapchainCI.imageFormat = surfaceFormat.format;
        swapchainCI.imageColorSpace = surfaceFormat.colorSpace;
        swapchainCI.imageExtent = extent;
        swapchainCI.imageArrayLayers = 1;
        swapchainCI.imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment;

        QueueFamilyIndices indices = find_queue_families(gpu);
        u32 queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value(), indices.computeFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            swapchainCI.imageSharingMode = vk::SharingMode::eConcurrent;
            swapchainCI.queueFamilyIndexCount = 4;
            swapchainCI.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            swapchainCI.imageSharingMode = vk::SharingMode::eExclusive;
        }

        swapchainCI.preTransform = support.capabilities.currentTransform;
        swapchainCI.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchainCI.presentMode = presentMode;
        swapchainCI.clipped = vk::True;

        vk_check(
            handle.createSwapchainKHR(&swapchainCI, nullptr, &swapchain),
            "Failed to create swapchain"
        );

        vk_check(
            handle.getSwapchainImagesKHR(swapchain, &imageCount, nullptr),
            "Failed to get swapchain images"
        );

        swapchainImages.resize(imageCount);

        vk_check(
        handle.getSwapchainImagesKHR(swapchain, &imageCount, swapchainImages.data()),
    "Failed to get swapchain images"
        );

        swapchainFormat = surfaceFormat.format;
        swapchainExtent = extent;
    }

    void Device::init_image_views() {
        swapchainImageViews.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); i++) {
            vk::ImageViewCreateInfo imageViewCI;
            imageViewCI.image = swapchainImages[i];
            imageViewCI.viewType = vk::ImageViewType::e2D;
            imageViewCI.format = swapchainFormat;
            imageViewCI.components = {
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity
            };

            imageViewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            imageViewCI.subresourceRange.baseMipLevel = 0;
            imageViewCI.subresourceRange.levelCount = 1;
            imageViewCI.subresourceRange.baseArrayLayer = 0;
            imageViewCI.subresourceRange.layerCount = 1;

            vk_check(
            handle.createImageView(&imageViewCI, nullptr, &swapchainImageViews[i]),
            "Failed to create image view"
            );
        }
    }

    eastl::vector<const char*> Device::get_required_extensions() {
        u32 glfwExtensionCount = 0;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        eastl::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) extensions.push_back(vk::EXTDebugUtilsExtensionName);

        return extensions;
    }

    bool Device::gpu_is_suitable(vk::PhysicalDevice gpu) {
        QueueFamilyIndices indices = find_queue_families(gpu);

        bool extensionsSupported = check_device_extension_support(gpu);

        bool swapchainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = query_swapchain_support(gpu);
            swapchainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.is_complete() && extensionsSupported && swapchainAdequate;
    }

    QueueFamilyIndices Device::find_queue_families(const vk::PhysicalDevice gpu) const {
        QueueFamilyIndices indices;
        u32 count = 0;

        gpu.getQueueFamilyProperties(&count, nullptr);
        std::vector<vk::QueueFamilyProperties> families(count);
        gpu.getQueueFamilyProperties(&count, families.data());

        i32 idx = 0;
        for (const auto& family : families) {
            if (family.queueFlags & vk::QueueFlagBits::eGraphics) indices.graphicsFamily = idx;
            if (family.queueFlags & vk::QueueFlagBits::eCompute) indices.computeFamily = idx;
            if (family.queueFlags & vk::QueueFlagBits::eTransfer) indices.transferFamily = idx;

            vk::Bool32 presentSupport = false;
            vk_check(
                gpu.getSurfaceSupportKHR(idx, surface, &presentSupport),
                "Failed to get GPU present support"
                );

            if (presentSupport) indices.presentFamily = idx;
            if (indices.is_complete()) break;
            idx++;
        }

        return indices;
    }

    bool Device::check_device_extension_support(const vk::PhysicalDevice gpu) {
        u32 extensionCount;
        vk_check(
            gpu.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr),
            "Failed to enumerate device extension properties"
            );

        std::vector<vk::ExtensionProperties> availableExtensions(extensionCount);
        vk_check(
            gpu.enumerateDeviceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()),
            "Failed to enumer device extension properties"
            );



        eastl::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        std::cout << "Required Extensions:" << std::endl;
        for (const auto& extension : requiredExtensions) {
            std::cout << extension << std::endl;
        }

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        std::cout << "Extensions not found:" << std::endl;
        for (const auto& extension : requiredExtensions) {
            std::cout << extension << std::endl;
        }

        return requiredExtensions.empty();
    }

    SwapChainSupportDetails Device::query_swapchain_support(vk::PhysicalDevice gpu) {
        SwapChainSupportDetails details;

        vk_check(
            gpu.getSurfaceCapabilitiesKHR(surface, &details.capabilities),
            "Failed to get surface capabillities"
            );

        u32 formatCount;
        vk_check(
        gpu.getSurfaceFormatsKHR(surface, &formatCount, nullptr),
        "Failed to get GPU surface formats"
        );

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vk_check(
            gpu.getSurfaceFormatsKHR(surface, &formatCount, details.formats.data()),
    "Failed to get GPU surface formats"
            );
        }

        u32 presentModeCount;
        vk_check(
        gpu.getSurfacePresentModesKHR(surface, &presentModeCount, nullptr),
        "Failed to get surface present modes"
                    );

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vk_check(
                gpu.getSurfacePresentModesKHR(surface, &presentModeCount, details.presentModes.data()),
    "Failed to get surface present modes"
            );
        }

        return details;
    }

    vk::SurfaceFormatKHR Device::choose_swap_surface_format(const eastl::vector<vk::SurfaceFormatKHR> &availableFormats) {
        for (const auto& format : availableFormats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return format;
            }
        }

        return availableFormats[0];
    }

    vk::PresentModeKHR Device::choose_swap_present_mode(const eastl::vector<vk::PresentModeKHR> &availablePresentModes) {
        for (const auto& mode : availablePresentModes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                return mode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D Device::choose_swap_extent(const vk::SurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != UINT_MAX) return capabilities.currentExtent;
        i32 width, height;
        glfwGetFramebufferSize(window, &width, &height);

        vk::Extent2D actualExtent {
            static_cast<u32>(width),
            static_cast<u32>(height)
        };

        actualExtent.width = std::clamp(
            actualExtent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width
            );

        actualExtent.height = std::clamp(
            actualExtent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height
            );

        return actualExtent;
    }

    void populate_debug_messenger_CI(vk::DebugUtilsMessengerCreateInfoEXT &debugMessengerCI) {
        debugMessengerCI.messageSeverity =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugMessengerCI.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding;
        debugMessengerCI.pfnUserCallback = debugCallback;
    }

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
