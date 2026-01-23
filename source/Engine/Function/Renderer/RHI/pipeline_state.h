#pragma once
#include "rasterizer_state.h"
#include "graphics_types.h"
#include "depth_stencil_state.h"
#include "blend_state.h"
#include "input_layout.h"
#include "render_pass.h"

namespace NexAur {
    // 可变着色率枚举(RTX20系之后的功能)
    enum class PipelineShadingRateFlags : uint8_t {
        // 不使用VRS
        None = 0,

        // 按图元指定
        Primitive = 1u << 0u,

        // 按图片指定
        TextureBased = 1u << 1u,

        // 
        Last = TextureBased
    };

    // 多重采样参数结构体
    struct SampleDesc {
        // sample count
        uint8_t count = 1;

        // sample quality
        uint8_t quality = 0;

        constexpr SampleDesc() noexcept {}

        constexpr SampleDesc(uint8_t _count, uint8_t _quality) noexcept : 
            count{_count}, quality(_quality) {}

        constexpr bool operator==(const SampleDesc& rhs) const {
            return count == rhs.count && quality == rhs.quality;
        }

        constexpr bool operator!=(const SampleDesc& rhs) const {
            return !(*this == rhs);
        }
    };

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

        // 多重采样描述
        SampleDesc sample_desc;

        // 可变着色率VRS
        PipelineShadingRateFlags shading_rate_flag{PipelineShadingRateFlags::None};

        // vulkan专用: 一个pass可以包含多个sub pass
        uint8_t subpass_index = 0;

        // 采样掩码，控制高阶抗锯齿
        uint32_t sample_mask = 0xFFFFFFFF;

        // TODO: 显式Render Pass指针，vulkan风格API，使用这个回忽略掉num_render_targets和RTV_formats，直接使用这里面的信息
        IRenderPass* p_render_pass = nullptr;

        // 多显卡交互
        uint32_t node_mask;

        bool operator==(const GraphicsPipelineDesc& rhs) const noexcept {
            if (!(rasterizer_desc == rhs.rasterizer_desc &&
                    privitive_topology == rhs.privitive_topology &&
                    depth_stencil_desc == rhs.depth_stencil_desc &&
                    blend_desc == rhs.blend_desc && 
                    num_viewports == rhs.num_viewports &&
                    num_render_targets == rhs.num_render_targets &&
                    input_layout == rhs.input_layout &&
                    read_only_DSV == rhs.read_only_DSV && 
                    sample_desc == rhs.sample_desc && 
                    shading_rate_flag == rhs.shading_rate_flag &&
                    subpass_index == rhs.subpass_index && 
                    sample_mask == rhs.sample_mask && 
                    node_mask == rhs.node_mask )) {
                return false;
            }

            for (uint32_t i = 0; i < num_render_targets; ++i) {
                if (RTV_formats[i] != rhs.RTV_formats[i])
                    return false;
            }

            if ((p_render_pass != nullptr) != (p_render_pass != nullptr))
                return false;

            /*
            if (pRenderPass != nullptr) {
                if (!(pRenderPass->GetDesc() == Rhs.pRenderPass->GetDesc()))
                    return false;
            }
            */

            return true;
        }

        bool operator!=(const GraphicsPipelineDesc& rhs) const noexcept {
            return !(*this == rhs);
        }
    };
    
} // namespace NexAur