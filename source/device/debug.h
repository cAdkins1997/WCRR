#pragma once
#include "../common.h"

#include <iostream>
#include <sstream>

namespace vulkan {


    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
            const VkAllocationCallbacks *pAllocator,
            VkDebugUtilsMessengerEXT *pMessenger);

    VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
            VkInstance instance,
            VkDebugUtilsMessengerEXT messenger,
            VkAllocationCallbacks const *pAllocator);

    VKAPI_ATTR vk::Bool32 VKAPI_CALL debugMessageFunc(
            vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
            const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
            void *);
}