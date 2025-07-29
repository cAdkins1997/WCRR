#include "debug.h"

#include <bits/this_thread_sleep.h>


    void vk_check(const vk::Result result, const std::string& outputString) {
#ifdef NDEBUG
        if (result != vk::Result::eSuccess)
            throw std::runtime_error("failed to create debug utils messenger");
/*#else
        if (result == vk::Result::eErrorDeviceLost) {
            const auto trdTerminationTimeout = std::chrono::seconds(3);
            const auto tStart = std::chrono::steady_clock::now();
            auto tElapsed = std::chrono::milliseconds::zero();

            GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
            GFSDK_Aftermath_GetCrashDumpStatus(&status);

            while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
                   status != GFSDK_Aftermath_CrashDump_Status_Finished &&
                   tElapsed < trdTerminationTimeout)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                GFSDK_Aftermath_GetCrashDumpStatus(&status);

                auto tEnd = std::chrono::steady_clock::now();
                tElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart);
            }

            if (status != GFSDK_Aftermath_CrashDump_Status_Finished) {
                std::stringstream errorMessage;
                errorMessage << "Unexpected crash dump status" << status;
                throw std::runtime_error(errorMessage.str());
            }
            exit(1);
        }*/
#endif
    }

namespace vulkan {
/*
    ShaderDatabase::ShaderDatabase() : shaderBinaries(), shaderBinariesWithDebugInfo() {}
    ShaderDatabase::~ShaderDatabase() = default;

    void ShaderDatabase::init(const bool useStrippedShaders) {
        shaderBinaries.clear();
        shaderBinariesWithDebugInfo.clear();

        if (useStrippedShaders) {
            add_shader_binary("");
            add_shader_binary("");

            add_shader_binary_with_debug_info("", "");
            add_shader_binary_with_debug_info("", "");
        }
        else {
            add_shader_binary("");
            add_shader_binary("");
        }
    }

    bool ShaderDatabase::find_shader_binary(
        const GFSDK_Aftermath_ShaderBinaryHash &shaderHash,
        std::vector<uint8_t> &shader) const
    {
        const auto shaderIt = shaderBinaries.find(shaderHash);
        if (shaderIt == shaderBinaries.end())
            return false;

        shader = shaderIt->second;
        return true;
    }

    bool ShaderDatabase::find_shader_binary_with_debug_data(
        const GFSDK_Aftermath_ShaderDebugName &shaderDebugName,
        std::vector<u8> &shader) const
    {
        const auto shaderIt = shaderBinariesWithDebugInfo.find(shaderDebugName);
        if (shaderIt == shaderBinariesWithDebugInfo.end())
            return false;

        shader = shaderIt->second;
        return true;
    }

    void ShaderDatabase::add_shader_binary(const char *shaderPath) {
        std::vector<u8> data;
        if (!read_file(shaderPath, data))
            return;

        const GFSDK_Aftermath_SpirvCode shader { data.data(), u32(data.size()) };
        GFSDK_Aftermath_ShaderBinaryHash shaderHash;
        GFSDK_Aftermath_GetShaderHashSpirv(GFSDK_Aftermath_Version_API, &shader, &shaderHash);
        shaderBinaries[shaderHash].swap(data);
    }

    void ShaderDatabase::add_shader_binary_with_debug_info(const char *strippedShaderPath, const char *shaderPath) {
        std::vector<u8> data;
        if (!read_file(shaderPath, data))
            return;
        std::vector<u8> strippedData;
        if (!read_file(strippedShaderPath, strippedData))
            return;

        GFSDK_Aftermath_ShaderDebugName debugName;
        const GFSDK_Aftermath_SpirvCode shader { data.data(), u32(data.size()) };
        const GFSDK_Aftermath_SpirvCode strippedShader { strippedData.data(), u32(strippedData.size()) };
        GFSDK_Aftermath_GetShaderDebugNameSpirv(GFSDK_Aftermath_Version_API, &shader, &strippedShader, &debugName);
        shaderBinariesWithDebugInfo[debugName].swap(data);
    }

    bool ShaderDatabase::read_file(const char *filename, std::vector<u8> &data) {
        std::ifstream fs(filename, std::ios::in | std::ios::binary);
        if (!fs) return false;

        fs.seekg(0, std::ios::end);
        data.resize(fs.tellg());
        fs.seekg(0, std::ios::beg);
        fs.read(reinterpret_cast<char*>(data.data()), data.size());
        fs.close();

        return true;
    }

    GpuCrashTracker::GpuCrashTracker(const MarkerMap &_markerMap) : markerMap(_markerMap) {}

    GpuCrashTracker::~GpuCrashTracker() {
        if (initialized) GFSDK_Aftermath_DisableGpuCrashDumps();
    }

    void GpuCrashTracker::init(bool useStrippedShaders) {
        shaderDatabase.init(useStrippedShaders);

        GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API,
            GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
            gpu_crash_dump_callback,
            shader_debug_info_callback,
            crash_dump_description_callback,
            resolve_marker_callback,
            this
        );

        initialized = true;
    }

    void GpuCrashTracker::on_crash_dump(const void *pGpuCrashDump, const u32 size) {
        std::lock_guard lock(mutex);
        write_gpu_crash_dump_to_file(pGpuCrashDump, size);
    }

    void GpuCrashTracker::on_shader_debug_info(const void *pShaderDebugInfo, const u32 shaderDebugInfoSize) {
        std::lock_guard lock(mutex);

        GFSDK_Aftermath_ShaderDebugInfoIdentifier indentifier{};
        GFSDK_Aftermath_GetShaderDebugInfoIdentifier(
            GFSDK_Aftermath_Version_API,
            pShaderDebugInfo,
            shaderDebugInfoSize,
            &indentifier
        );

        std::vector data((u8*)pShaderDebugInfo, (u8*)pShaderDebugInfo + shaderDebugInfoSize);
        shaderDebugInfos[indentifier].swap(data);

        //    WriteShaderDebugInformationToFile(identifier, pShaderDebugInfo, shaderDebugInfoSize);
    }

    void GpuCrashTracker::on_description(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription) {
        addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "WCRR");
        addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "v0.1");
    }

    void GpuCrashTracker::on_resolve_marker(
        const void *pMarkerData,
        const u32 size,
        void **ppResolvedMarkerData,
        u32 *pResolvedMarkerDataSize)
    {
        for (auto& map : markerMap) {
            const auto& foundMarker = map.find(reinterpret_cast<u64>(pMarkerData));
            if (foundMarker != map.end()) {
                const std::string& foundMarkerDat = foundMarker->second;
                *ppResolvedMarkerData = (void*)foundMarkerDat.data();
                *pResolvedMarkerDataSize = (u32)foundMarkerDat.length();
                return;
            }
        }
    }

    void GpuCrashTracker::write_gpu_crash_dump_to_file(const void *pGpuCrashDump, const u32 size) {
        GFSDK_Aftermath_GpuCrashDump_Decoder decoder{};
        GFSDK_Aftermath_GpuCrashDump_CreateDecoder(
            GFSDK_Aftermath_Version_API,
            pGpuCrashDump,
            size,
            &decoder
        );

        GFSDK_Aftermath_GpuCrashDump_BaseInfo baseInfo{};
        GFSDK_Aftermath_GpuCrashDump_GetBaseInfo(decoder, &baseInfo);
        u32 applicationNameLength = 0;

        GFSDK_Aftermath_GpuCrashDump_GetDescriptionSize(
            decoder,
            GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
            &applicationNameLength
        );

        std::vector<char> applicationName(applicationNameLength, '\0');

        GFSDK_Aftermath_GpuCrashDump_GetDescription(
            decoder,
            GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
            static_cast<uint32_t>(applicationName.size()),
            applicationName.data()
            );

        static u32 count = 0;
        auto baseFileName = std::string(applicationName.data()) + "-" + std::to_string(baseInfo.pid) + "-" + std::to_string(++count);
        const std::string crashDumpFileName = baseFileName + ".nv-gpudump";
        std::ofstream dumpFile(crashDumpFileName, std::ios::out | std::ios::binary);

        if (dumpFile) {
            dumpFile.write(static_cast<const char *>(pGpuCrashDump), size);
            dumpFile.close();
        }

        u32 jsonSize = 0;
        GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
            decoder,
            GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
            GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE,
            shader_debug_info_lookup_callback,
            shader_lookup_callback,
            shader_source_debug_info_lookup_callback,
            this,
            &jsonSize
        );

        std::vector<char> json(jsonSize);
        GFSDK_Aftermath_GpuCrashDump_GetJSON(
            decoder,
            static_cast<u32>(json.size()),
            json.data()
        );

        const std::string jsonFileName = crashDumpFileName + ".json";
        std::ofstream jsonFile(jsonFileName, std::ios::out | std::ios::binary);
        if (jsonFile) {
            jsonFile.write(json.data(), json.size() - 1);
            jsonFile.close();
        }

        GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder);
    }

    void GpuCrashTracker::write_shader_debug_info_to_file(
        GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier,
        const void *pShaderDebugInfo,
        const u32 shaderDebugInfoSize)
    {
        const std::string filePath = "shader-" + std::to_string(identifier) + ".nvdbg";

        std::ofstream file(filePath, std::ios::out | std::ios::binary);
        if (file) file.write(static_cast<const char *>(pShaderDebugInfo), shaderDebugInfoSize);
    }

    void GpuCrashTracker::on_shader_debug_info_lookup(
        const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier,
        const PFN_GFSDK_Aftermath_SetData setShaderDebugInfo) const
    {
        auto debugInfoIt = shaderDebugInfos.find(identifier);
        if (debugInfoIt != shaderDebugInfos.end())
            return;

        setShaderDebugInfo(debugInfoIt->second.data(), static_cast<u32>(debugInfoIt->second.size()));
    }

    void GpuCrashTracker::on_shader_lookup(
        const GFSDK_Aftermath_ShaderBinaryHash &shaderHash,
        PFN_GFSDK_Aftermath_SetData setShaderBinary) const
    {
        std::vector<u8> shaderBinary;
        if (!shaderDatabase.find_shader_binary(shaderHash, shaderBinary))
            return;

        setShaderBinary(shaderBinary.data(), static_cast<u32>(shaderBinary.size()));
    }

    void GpuCrashTracker::on_shader_source_debug_info_lookup(
        const GFSDK_Aftermath_ShaderDebugName &shaderDebugName,
        PFN_GFSDK_Aftermath_SetData setShaderBinary) const
    {
        std::vector<u8> shaderBinary;
        if (!shaderDatabase.find_shader_binary_with_debug_data(shaderDebugName, shaderBinary))
            return;

        setShaderBinary(shaderBinary.data(), static_cast<u32>(shaderBinary.size()));
    }

    void GpuCrashTracker::gpu_crash_dump_callback(const void *pGpuCrashDump, const u32 size, void *pUserData) {
        const auto pGpuCrashTracker = static_cast<GpuCrashTracker*>(pUserData);
        pGpuCrashTracker->on_crash_dump(pGpuCrashDump, size);
    }

    void GpuCrashTracker::shader_debug_info_callback(const void *pShaderDebugInfo, const u32 size, void *pUserData) {
        const auto pGpuCrashTracker = static_cast<GpuCrashTracker*>(pUserData);
        pGpuCrashTracker->on_shader_debug_info(pShaderDebugInfo, size);
    }

    void GpuCrashTracker::crash_dump_description_callback(
        const PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription,
        void *pUserData)
    {
        const auto pGpuCrashTracker = static_cast<GpuCrashTracker*>(pUserData);
        pGpuCrashTracker->on_description(addDescription);
    }

    void GpuCrashTracker::resolve_marker_callback(
        const void *pMarkerData,
        const u32 markerDataSize,
        void *pUserData,
        void **ppResolvedMarkerData,
        u32 *pResolvedMarkerDataSize)
    {
        const auto pGpuCrashTracker = static_cast<GpuCrashTracker*>(pUserData);
        pGpuCrashTracker->on_resolve_marker(pMarkerData, markerDataSize, ppResolvedMarkerData, pResolvedMarkerDataSize);
    }

    void GpuCrashTracker::shader_debug_info_lookup_callback(
        const GFSDK_Aftermath_ShaderDebugInfoIdentifier *pIdentifier,
        const PFN_GFSDK_Aftermath_SetData setShaderDebugInfo,
        void *pUserData)
    {
        auto pGpuCrashTracker = static_cast<GpuCrashTracker*>(pUserData);
        pGpuCrashTracker->on_shader_debug_info_lookup(*pIdentifier, setShaderDebugInfo);
    }

    void GpuCrashTracker::shader_lookup_callback(
        const GFSDK_Aftermath_ShaderBinaryHash *pShaderHash,
        const PFN_GFSDK_Aftermath_SetData setShaderBinary,
        void *pUserData)
    {
        auto pGpuCrashTracker = static_cast<GpuCrashTracker*>(pUserData);
        pGpuCrashTracker->on_shader_lookup(*pShaderHash, setShaderBinary);
    }

    void GpuCrashTracker::shader_source_debug_info_lookup_callback(
        const GFSDK_Aftermath_ShaderDebugName *pShaderDebugName,
        const PFN_GFSDK_Aftermath_SetData setShaderBinary,
        void *pUserData)
    {
        auto pGpuCrashTracker = static_cast<GpuCrashTracker*>(pUserData);
        pGpuCrashTracker->on_shader_source_debug_info_lookup(*pShaderDebugName, setShaderBinary);
    }
*/
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
                            << to_string(callBackData->pObjects[i].objectType) << "\n";
                    message << std::string("\t\t\t") << "objectHandle = " << pCallbackData->pObjects[i].objectHandle
                            << "\n";

                    if (pCallbackData->pObjects[i].pObjectName) {
                        message << std::string("\t\t\t") << "objectName   = <" << pCallbackData->pObjects[i].pObjectName
                                << ">\n";
                    }
                }
            }

            #ifdef _WIN32
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
