#include "common.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

struct traced_new_tag_t {};
constexpr traced_new_tag_t traced_new_tag;

#include <iostream>

void* operator new [](size_t size, const char *name, int flags, unsigned debugFlags, const char *file, int line) {
    return new uint8_t[size];
}

void vk_check(const vk::Result result, const std::string& outputString) {
    if (result != vk::Result::eSuccess) {
        throw std::runtime_error((outputString + ' ' + to_string(result) + '\n'));
    }
}
