#include "common.h"

#define VMA_IMPLEMENTATION
#ifdef WIN32
#include <vma/vk_mem_alloc.h>
#else
#include <vk_mem_alloc.h>
#endif

struct traced_new_tag_t {};
constexpr traced_new_tag_t traced_new_tag;

#include <iostream>
