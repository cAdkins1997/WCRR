#pragma once
#include "../commands.h"
#include "../pipelines/descriptors.h"
#include "../resources/scenemanager.h"
#include "../glmdefines.h"

#include <filesystem>
#include <fstream>
#include <EASTL/set.h>
#include <EASTL/optional.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <functional>
#include <iostream>

namespace vulkan {
    struct Buffer;
    struct Sampler;

    struct GraphicsContext;
    struct ComputeContext;
    struct UploadContext;
    struct RaytracingContext;

    constexpr u8 MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameData {
        vk::CommandPool commandPool;
        CommandBuffer commandBuffer;
        vk::Semaphore swapchainSemaphore;
        vk::Semaphore renderSemaphore;
        vk::Fence renderFence;
    };

#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

    const eastl::vector deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
        //VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
    };

    void populate_debug_messenger_CI(vk::DebugUtilsMessengerCreateInfoEXT& debugMessengerCI);
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* debugCI,
        const VkAllocationCallbacks* allocator,
        VkDebugUtilsMessengerEXT* debugMessenger);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    struct QueueFamilyIndices {
        eastl::optional<u32> graphicsFamily;
        eastl::optional<u32> presentFamily;
        eastl::optional<u32> computeFamily;
        eastl::optional<u32> transferFamily;

        [[nodiscard]] bool is_complete() const {
            return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value() && transferFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        eastl::vector<vk::SurfaceFormatKHR> formats;
        eastl::vector<vk::PresentModeKHR> presentModes;
    };

    class Device {
    public:
        Device(eastl::string_view appName, u32 _width, u32 _height, u32 countFramesInFlight);
        ~Device();

        [[nodiscard]] vk::Device get_handle() const { return handle; }
        [[nodiscard]] vk::PhysicalDevice get_gpu() const { return gpu; }
        [[nodiscard]] vk::Queue get_graphics_queue() const { return graphicsQueue; }
        [[nodiscard]] vk::Queue get_present_queue() const { return presentQueue; }
        [[nodiscard]] vk::Queue get_transferQueue() const { return transferQueue; }

        [[nodiscard]] Buffer create_buffer(
            size_t allocationSize,
            vk::BufferUsageFlags usage,
            vma::MemoryUsage memoryUsage,
            vma::AllocationCreateFlags flags = vma::AllocationCreateFlagBits::eMapped) const;
        [[nodiscard]] Image create_image(vk::Extent3D size, vk::Format format, vk::ImageUsageFlags usage, u32 mipLevels, bool mipmapped) const;
        [[nodiscard]] Sampler create_sampler(vk::Filter minFilter, vk::Filter magFilter, vk::SamplerMipmapMode mipmapMode) const;
        [[nodiscard]] Shader create_shader(eastl::string_view filePath) const;

        void submit_graphics_work(const GraphicsContext& context, vk::PipelineStageFlagBits2 wait, vk::PipelineStageFlagBits2 signal);
        void submit_compute_work(const ComputeContext& context, vk::PipelineStageFlagBits2 wait, vk::PipelineStageFlagBits2 signal);
        void submit_raytracing_work(const RaytracingContext& context, vk::PipelineStageFlagBits2 wait, vk::PipelineStageFlagBits2 signal);
        void submit_upload_work(const UploadContext& context, vk::PipelineStageFlagBits2 wait, vk::PipelineStageFlagBits2 signal) const;
        void submit_upload_work(const UploadContext& context) const;
        void submit_immediate_work(std::function<void(vk::CommandBuffer)>&& function) const;

        void wait_on_work();
        void present();

        Image& get_draw_image() { return drawImage; }
        Image& get_depth_image() { return depthImage; }
        vk::Image& get_swapchain_image();
        vk::Extent3D get_swapchain_extent() const { return {swapchainExtent.width, swapchainExtent.height, 1}; }
        [[nodiscard]] GLFWwindow* get_window() const { return window; }

        [[nodiscard]] u32 get_width() const { return width; }
        [[nodiscard]] u32 get_height() const { return height; }

        [[nodiscard]] vma::Allocator get_allocator() const { return allocator; }
        u32 get_swapchain_image_index();

    public:
        [[nodiscard]] FrameData& get_current_frame() { return frames[frameNumber % MAX_FRAMES_IN_FLIGHT]; }

    private:
        void init_instance();
        void init_debug_messenger();
        void init_window();
        void init_surface();
        void select_gpu();
        void init_device();
        void init_swapchain();
        void init_image_views();
        void init_commands();
        void init_sync_objects();
        void init_allocator();
        void init_descriptors();
        void init_draw_images();
        void init_depth_images();

    private:
        eastl::vector<const char*> get_required_extensions();
        bool gpu_is_suitable(vk::PhysicalDevice gpu);
        [[nodiscard]] QueueFamilyIndices find_queue_families(vk::PhysicalDevice gpu) const;
        bool check_device_extension_support(vk::PhysicalDevice gpu);
        SwapChainSupportDetails query_swapchain_support(vk::PhysicalDevice gpu);
        vk::SurfaceFormatKHR choose_swap_surface_format(const eastl::vector<vk::SurfaceFormatKHR>& availableFormats);
        vk::PresentModeKHR choose_swap_present_mode(const eastl::vector<vk::PresentModeKHR>& availablePresentModes);
        vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities);

    private:
        FrameData frames[MAX_FRAMES_IN_FLIGHT];
        std::vector<const char*> validationLayers {"VK_LAYER_KHRONOS_validation"};
        std::string applicationName;
        vk::Instance instance;
        vk::DebugUtilsMessengerEXT debugMessenger;

        vk::Device handle;
        vk::PhysicalDevice gpu;

        u32 MAX_FRAMES_IN_FLIGHT;

    public:
        bool resizeRequested = false;
        vk::Fence immediateFence;
        vk::CommandPool immediateCommandPool;
        vk::CommandBuffer immediateCommandBuffer;
        std::vector<vk::Image> swapchainImages{};

    private:
        u32 width{}, height{};
        u32 frameNumber = 0;
        GLFWwindow* window = nullptr;
        vk::SurfaceKHR surface{};
        vk::SwapchainKHR swapchain{};
        vk::Format swapchainFormat{};
        vk::Extent2D swapchainExtent;
        std::vector<vk::ImageView> swapchainImageViews;
        u32 swapchainImageIndex = 0;

        Image drawImage;
        Image depthImage;

        u32 graphicsQueueIndex{}, computeQueueIndex{}, presentQueueIndex{}, transferQueueIndex{};
        vk::Queue graphicsQueue, computeQueue, presentQueue, transferQueue;

        vma::Allocator allocator{};
    };
}
