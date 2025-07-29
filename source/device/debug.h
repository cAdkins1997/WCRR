#pragma once
#include "../common.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>
#include <mutex>

#include "../../include/aftermath/include/GFSDK_Aftermath.h"
#include "../../include/aftermath/include/GFSDK_Aftermath_Defines.h"
#include "../../include/aftermath/include/GFSDK_Aftermath_GpuCrashDump.h"
#include "../../include/aftermath/include/GFSDK_Aftermath_GpuCrashDumpDecoding.h"

void vk_check(vk::Result result, const std::string& outputString);

namespace std
{
        template<typename T>
        std::string to_hex_string(T n) {
                std::stringstream stream;
                stream << std::setfill('0') << std::setw(2 * sizeof(T)) << std::hex << n;
                return stream.str();
        }

        inline std::string to_string(GFSDK_Aftermath_Result result) {
                return std::string("0x") + to_hex_string(static_cast<uint32_t>(result));
        }

        inline std::string to_string(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier) {
                return to_hex_string(identifier.id[0]) + "-" + to_hex_string(identifier.id[1]);
        }

        inline std::string to_string(const GFSDK_Aftermath_ShaderBinaryHash& hash) {
                return to_hex_string(hash.hash);
        }
}

inline bool operator<(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& lhs, const GFSDK_Aftermath_ShaderDebugInfoIdentifier& rhs) {
        if (lhs.id[0] == rhs.id[0])
                return lhs.id[1] < rhs.id[1];
        return lhs.id[0] < rhs.id[0];
}

inline bool operator<(const GFSDK_Aftermath_ShaderBinaryHash& lhs, const GFSDK_Aftermath_ShaderBinaryHash& rhs) {
        return lhs.hash < rhs.hash;
}

inline bool operator<(const GFSDK_Aftermath_ShaderDebugName& lhs, const GFSDK_Aftermath_ShaderDebugName& rhs) {
        return strncmp(lhs.name, rhs.name, sizeof(lhs.name)) < 0;
}

namespace vulkan {

        class ShaderDatabase {

        public:
                ShaderDatabase();
                ~ShaderDatabase();

                void init(bool useStrippedShaders);
                bool find_shader_binary(const GFSDK_Aftermath_ShaderBinaryHash& shaderHash, std::vector<uint8_t>& shader) const;
                bool find_shader_binary_with_debug_data(const GFSDK_Aftermath_ShaderDebugName& shaderDebugName, std::vector<u8>& shader) const;

        private:
                void add_shader_binary(const char* shaderPath);
                void add_shader_binary_with_debug_info(const char* strippedShaderPath, const char* shaderPath);

                static bool read_file(const char* filename, std::vector<u8>& data);

        private:
                std::map<GFSDK_Aftermath_ShaderBinaryHash, std::vector<u8>> shaderBinaries;
                std::map<GFSDK_Aftermath_ShaderDebugName, std::vector<u8>> shaderBinariesWithDebugInfo;
        };

        class GpuCrashTracker {
        public:
                static constexpr u32 MARKER_FRAME_HISTORY = 4;
                typedef std::array<std::map<u64, std::string>, MARKER_FRAME_HISTORY> MarkerMap;

                explicit GpuCrashTracker(const MarkerMap& _markerMap);
                ~GpuCrashTracker();

                void init(bool useStrippedShaders);

        private:
                void on_crash_dump(const void* pGpuCrashDump, u32 size);
                void on_shader_debug_info(const void* pShaderDebugInfo, u32 shaderDebugInfoSize);
                void on_description(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription);
                void on_resolve_marker(
                        const void* pMarkerData,
                        const u32 size,
                        void** ppResolvedMarkerData,
                        u32* pResolvedMarkerDataSize);

                void write_gpu_crash_dump_to_file(const void* pGpuCrashDump, u32 size);
                void write_shader_debug_info_to_file(
                        GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier,
                        const void* pShaderDebugInfo,
                        u32 shaderDebugInfoSize);
                void on_shader_debug_info_lookup(
                        const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier,
                        PFN_GFSDK_Aftermath_SetData setShaderDebugInfo
                        ) const;
                void on_shader_lookup(
                        const GFSDK_Aftermath_ShaderBinaryHash& shaderHash,
                        PFN_GFSDK_Aftermath_SetData setShaderBinary
                        ) const;
                void on_shader_source_debug_info_lookup(
                        const GFSDK_Aftermath_ShaderDebugName& shaderDebugName,
                        PFN_GFSDK_Aftermath_SetData setShaderBinary
                ) const;

                static void gpu_crash_dump_callback(const void* pGpuCrashDUmp, u32 size, void* pUserData);
                static void shader_debug_info_callback(const void* pShaderDebugInfo, u32 size, void* pUserData);
                static void crash_dump_description_callback(
                        PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription,
                        void* pUserData
                        );
                static void resolve_marker_callback(
                        const void* pMarkerData,
                        u32 markerDataSize,
                        void* pUserData,
                        void** ppResolvedMarkerData,
                        u32* pResolvedMarkerDataSize
                        );
                static void shader_debug_info_lookup_callback(
                        const GFSDK_Aftermath_ShaderDebugInfoIdentifier* pIdentifier,
                        PFN_GFSDK_Aftermath_SetData setShaderDebugInfo,
                        void* pUserData
                        );
                static void shader_lookup_callback(
                        const GFSDK_Aftermath_ShaderBinaryHash* pShaderHash,
                        PFN_GFSDK_Aftermath_SetData setShaderBinary,
                        void* pUserData
                        );
                static void shader_source_debug_info_lookup_callback(
                        const GFSDK_Aftermath_ShaderDebugName* pShaderDebugName,
                        PFN_GFSDK_Aftermath_SetData setShaderBinary,
                        void* pUserData
                );
        private:
                bool initialized = false;
                mutable std::mutex mutex;

                std::map<GFSDK_Aftermath_ShaderDebugInfoIdentifier, std::vector<u8>> shaderDebugInfos;
                ShaderDatabase shaderDatabase;
                const MarkerMap& markerMap;
        };


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