#include "debug.h"

namespace vulkan {
    vk::Bool32 debugMessageFunc(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
            std::ostringstream message;

            message << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagsEXT>(messageSeverity))
            << ": " << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(messageTypes)) << ":\n";
            message << std::string("\t") << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
            message << std::string("\t") << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
            message << std::string("\t") << "message         = <" << pCallbackData->pMessage << ">\n";
            if (0 < pCallbackData->queueLabelCount) {
                message << std::string("\t") << "Queue Labels:\n";

                for (u32 i = 0; i < pCallbackData->queueLabelCount; i++) {
                    message << std::string("\t\t") << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
                }
            }
            if (0 < pCallbackData->cmdBufLabelCount) {
                message << std::string("\t") << "CommandBuffer Labels:\n";

                for (u32 i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
                    message << std::string("\t\t") << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName
                            << ">\n";
                }
            }
            if (0 < pCallbackData->objectCount) {
                message << std::string("\t") << "Objects:\n";

                auto callBackData = (vk::DebugUtilsMessengerCallbackDataEXT*)pCallbackData;

                for (u32 i = 0; i < pCallbackData->objectCount; i++) {
                    message << std::string("\t\t") << "Object " << i << "\n";
                    message << std::string("\t\t\t") << "objectType   = "
                            << vk::to_string(callBackData->pObjects[i].objectType) << "\n";
                    message << std::string("\t\t\t") << "objectHandle = " << pCallbackData->pObjects[i].objectHandle
                            << "\n";

                    if (pCallbackData->pObjects[i].pObjectName) {
                        message << std::string("\t\t\t") << "objectName   = <" << pCallbackData->pObjects[i].pObjectName
                                << ">\n";
                    }
                }
            }

            #ifdef _WIN32
            MessageBox( NULL, message.str().c_str(), "Alert", MB_OK );
            #else
            std::cout << message.str() << std::endl;
            #endif
            return false;
    }

    void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                         const VkAllocationCallbacks *pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, messenger, pAllocator);
    }

    VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator,
                                            VkDebugUtilsMessengerEXT *pMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
            return func(instance, pCreateInfo, pAllocator, pMessenger);
        else
            return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}