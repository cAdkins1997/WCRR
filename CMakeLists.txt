cmake_minimum_required(VERSION 3.28)
project(TESTWCGL)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME}
        source/common.h
        source/device/device.h
        source/resources.h
        source/common.cpp
        source/commands.h
        source/device/device.cpp
        source/commands.cpp
        source/device/device.cpp
        source/main.cpp
        source/glmdefines.h
        source/pipelines/pipelineBuilder.cpp
        source/pipelines/pipelineBuilder.h
        source/application.cpp
        source/application.h
        source/scene/camera.cpp
        source/scene/camera.h
        source/resources/resourcehelpers.cpp
        source/resources/resourcehelpers.h
        source/resources/scenemanager.cpp
        source/resources/scenemanager.h
        source/pipelines/descriptors.cpp
        source/pipelines/descriptors.h
        source/device/debug.h
        source/device/debug.cpp
)

if (WIN32)
    find_package(Vulkan REQUIRED)
    find_package(VulkanMemoryAllocator HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/vulkan-memory-allocator_x64-windows")
    find_package(zstd CONFIG REQUIRED HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/zstd_x64-windows")
    find_package(Ktx CONFIG REQUIRED HINTS  "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/ktx_x64-windows")

    add_subdirectory(include/glfw)
    add_subdirectory(include/fastgltf)

    set(IMGUI_PATH ${CMAKE_CURRENT_LIST_DIR}/include/imgui)
    file(GLOB IMGUI_GLOB
            ${IMGUI_PATH}/imgui.h
            ${IMGUI_PATH}/imgui.cpp
            ${IMGUI_PATH}/imconfig.h
            ${IMGUI_PATH}/imgui_demo.cpp
            ${IMGUI_PATH}/imgui_draw.cpp
            ${IMGUI_PATH}/imgui_internal.h
            ${IMGUI_PATH}/imstb_rectpack.h
            ${IMGUI_PATH}/imstb_textedit.h
            ${IMGUI_PATH}/imstb_truetype.h
            ${IMGUI_PATH}/imgui_tables.cpp
            ${IMGUI_PATH}/imgui_widgets.cpp

            ${IMGUI_PATH}/backends/imgui_impl_glfw.h
            ${IMGUI_PATH}/backends/imgui_impl_glfw.cpp
            ${IMGUI_PATH}/imgui_impl_vulkan.h
            ${IMGUI_PATH}/imgui_impl_vulkan.cpp)

    add_library("imgui" STATIC ${IMGUI_GLOB})
    target_include_directories("imgui" PUBLIC ${IMGUI_PATH})
    target_link_libraries("imgui" PRIVATE glfw Vulkan::Vulkan)

    set(EASTL_LIBRARY debug ${z}/build/Debug/EASTL.lib optimized ${EASTL_ROOT_DIR}/build/Release/EASTL.lib)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    target_link_libraries(
            ${PROJECT_NAME}
            Vulkan::Vulkan
            glfw
            fastgltf::fastgltf
            imgui
            KTX::ktx
    )

elseif (UNIX AND NOT APPLE)
    find_package(Vulkan REQUIRED)
    find_package(unofficial-spirv-reflect CONFIG REQUIRED)
    find_package(zstd CONFIG REQUIRED)
    find_package(Ktx CONFIG REQUIRED)

    add_subdirectory(include/glfw)
    add_subdirectory(include/fastgltf)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    target_link_libraries(
            ${PROJECT_NAME}
            Vulkan::Vulkan
            glfw
            fastgltf::fastgltf
            unofficial::spirv-reflect
            KTX::ktx
    )
endif ()

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()
file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

file(GLOB SLANG_PROGRAM_FILES "${PROJECT_SOURCE_DIR}/shaders/src/slang/*.slang")
file(GLOB SLANG_MODULE_FILES "${PROJECT_SOURCE_DIR}/shaders/src/slang/modules/*.slang")

foreach(SLANG_MODULE ${SLANG_MODULE_FILES})
    message(STATUS "BUILDING SLANG MODULES")
    get_filename_component(MODULE_NAME ${SLANG_MODULE} NAME)
    set(SLANG "${PROJECT_SOURCE_DIR}/shaders/bin/slang/${MODULE_NAME}.slang-module")
    message(STATUS ${SLANG_MODULE})
    add_custom_command(
            OUTPUT ${SLANG}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/bin/slang/"
            COMMAND ${SLANGC} ${SLANG_MODULE} -g -o ${SLANG}
            DEPENDS ${SLANG_MODULE}
    )
    list(APPEND SLANG_MODULE_FILES ${SPIRV})
endforeach(SLANG_MODULE)

foreach(SLANG_PROGRAM ${SLANG_PROGRAM_FILES})
    message(STATUS "BUILDING SLANG PROGRAMS")
    get_filename_component(PROGRAM_NAME ${SLANG_PROGRAM} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/bin/slang/${FILE_NAME}.spv")
    message(STATUS ${SLANG_PROGRAM})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/bin/slang/"
            COMMAND ${SLANGC} {SLANG_PROGRAM} -g -o ${SPIRV}
            DEPENDS ${SLANG_PROGRAM}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(SLANG_PROGRAM)

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/bin/glsl${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/bin/glsl"
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -g -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
        Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/shaders/bin/glsl"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/shaders/bin/glsl"
        "${PROJECT_SOURCE_DIR}/shaders/bin/glsl"

        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/shaders/bin/slang"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/shaders/bin/slang"
        "${PROJECT_SOURCE_DIR}/shaders/bin/slang"
)