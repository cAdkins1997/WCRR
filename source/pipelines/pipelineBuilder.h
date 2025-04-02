
#pragma once
#include "../glmdefines.h"

#include <vector>
#include "../common.h"
#include "../resources.h"

class PipelineBuilder {
public:
    PipelineBuilder() { clear(); }

    void clear();
    vk::Pipeline build_pipeline(vk::Device device);

    void set_shader(vk::ShaderModule vertexShader, vk::ShaderModule fragmentShader);
    void set_shader(vk::ShaderModule taskShader, vk::ShaderModule meshShader, vk::ShaderModule fragmentShader);
    void set_input_topology(vk::PrimitiveTopology topology);
    void set_polygon_mode(vk::PolygonMode mode);
    void set_cull_mode(vk::CullModeFlags cullMode, vk::FrontFace frontFace);
    void set_multisampling_none();
    void set_color_attachment_format(vk::Format format);
    void set_depth_format(vk::Format format);
    void enable_depthtest(vk::Bool32 depthWriteEnable, vk::CompareOp op);
    void disable_depthtest();
    void enable_blending_additive();
    void enable_blending_alphablend();
    void disable_blending();

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    vk::PipelineMultisampleStateCreateInfo multisampling;
    vk::PipelineLayout pipelineLayout;
    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    vk::PipelineRenderingCreateInfo renderInfo;
    vk::Format colorAttachmentformat{};
};
