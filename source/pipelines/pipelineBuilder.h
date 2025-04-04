
#pragma once
#include "../glmdefines.h"

#include <vector>
#include "../common.h"
#include "../resources.h"
#include "../device/device.h"

class PipelineBuilder {
public:
    PipelineBuilder() { clear(); }

    void clear();
    VkPipeline build_pipeline(vulkan::Device& device);

    void set_shader(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void set_multisampling_none();
    void set_color_attachment_format(VkFormat format);
    void set_depth_format(VkFormat format);
    void enable_depthtest(VkBool32 depthWriteEnable, VkCompareOp op);
    void disable_depthtest();
    void enable_blending_additive();
    void enable_blending_alphablend();
    void disable_blending();

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo rasterizer{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineMultisampleStateCreateInfo multisampling{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineLayout pipelineLayout{};
    VkPipelineDepthStencilStateCreateInfo depthStencil{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineRenderingCreateInfo renderInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    VkFormat colorAttachmentformat{};
};
