#pragma once
#include "rasterizer_state.h"
#include "graphics_types.h"
#include "depth_stencil_state.h"
#include "blend_state.h"
#include "input_layout.h"

/* 
    TODO: 
    GraphicsPipelineDesc
    GraphicsPipelineStateCreateInfo
    RHI_PipelineState
*/
namespace NexAur {
    struct GraphicsPipelineDesc {
        // 光栅化信息
        RasterizerStateDesc rasterizer_desc;

        // 图元类型
        PrimitiveTopology privitive_topology{PrimitiveTopology::TriangleList};

        // 模板深度测试信息
        DepthStencilStateDesc depth_stencil_desc;

        // 混合信息
        BlendStateDesc blend_desc;

        // 视口数量
        uint8_t num_viewports = 1;

        // 渲染目标数量，当用Render pass的时候要设置为0
        uint8_t num_render_targets = 0;

        // 输入数据布局
        InputLayoutDesc input_layout;

        // 颜色纹理格式数组
        TextureFormat RTV_formats[RHI::MAX_RENDER_TARGETS]{};

        // 深度纹理格式
        TextureFormat DSV_formats{TextureFormat::TEX_FORMAT_UNKNOWN};

        // 深度只读开关
        bool read_only_DSV = false;

        // TODO: 多重采样描述
        // SampleDesc SmplDesc;

        // TODO: 可变着色率VRS
        // PIPELINE_SHADING_RATE_FLAGS ShadingRateFlags DEFAULT_INITIALIZER(PIPELINE_SHADING_RATE_FLAG_NONE);

        // TODO
        // uint8_t subpass_index = 0;

        // TODO: 采样掩码，控制高阶抗锯齿
        // uint32_t SampleMask = 0xFFFFFFFF;

        // TODO: 显示Render Pass指针
        // IRenderPass* pRenderPass     DEFAULT_INITIALIZER(nullptr);

        // TODO: 多卡交互
        // uint32_t node_mask;
    };
    
} // namespace NexAur