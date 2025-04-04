#include "common.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

struct traced_new_tag_t {};
constexpr traced_new_tag_t traced_new_tag;

#include <iostream>

#ifdef WIN32

void* operator new [](size_t size, const char *name, int flags, unsigned debugFlags, const char *file, int line) {
    return new uint8_t[size];
}


namespace D3D {
    std::string hr_to_string(HRESULT hresult)  {
        char s_str[64]{};
        sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hresult));
        return s_str;
    }

    void throw_if_failed(HRESULT hresult) {
        if(FAILED(hresult)) {
            throw HrException(hresult);
        }
    }
}

/*void vk_check(VkResult result, const std::string &outputString) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(outputString + ' ' + string_VkResult(result) + '\n');
    }
}*/

#endif

void vk_check(const vk::Result result, const std::string& outputString) {
    if (result != vk::Result::eSuccess) {
        throw std::runtime_error((outputString + ' ' + to_string(result) + '\n'));
    }
}
