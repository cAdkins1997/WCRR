#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <print>
#include <EASTL/vector.h>
#include <EASTL/string.h>

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

typedef float f32;
typedef double f64;

void vk_check(vk::Result result, const std::string& outputString);
void vk_check(VkResult result, const std::string& outputString);

#if defined(min)
#undef min
#endif

#ifndef WIN64_LEAN_AND_MEAN
#define WIN64_LEAN_AND_MEAN
#endif
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <D3D12MemAlloc.h>
#include <directxmath.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <shellapi.h>

void* __cdecl operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line);

namespace D3D {
    using namespace Microsoft::WRL;

#define SAFE_RELEASE(p) if (p) (p)->Release()

    std::string hr_to_string(HRESULT hresult);
    void throw_if_failed(HRESULT hresult);

    class HrException : public std::runtime_error {
    public:
        explicit HrException(HRESULT hr) : std::runtime_error(hr_to_string(hr)), m_hr(hr) {}
        [[nodiscard]] HRESULT Error() const { return m_hr; }
    private:
        const HRESULT m_hr;
    };
}