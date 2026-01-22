#pragma once
#include <cstdint>

namespace NexAur {
    // 填充模式枚举
    enum class FillMode : int8_t {
        Undefined = 0,
        Wireframe,  // 线框模式，调试常用
        Solid,      // 实体模式，正常渲染用
        NumModes
    };

    // 剔除模式枚举
    enum class CullMode : int8_t {
        Undefined = 0,
        None,           // 不剔除, 双面渲染，画树叶草丛等
        FrontFace,     // 剔除正面
        BackFace,      // 剔除背面，最常用
        NumModes
    };

    // 光栅化状态结构体
    struct RasterizerStateDesc {
        FillMode fil_mode = FillMode::Solid;    // 填充模式
        CullMode cull_mode = CullMode::BackFace; // 剔除模式
        bool front_counter_clockwise = false;   // 是否逆时针为正面, 默认顺时针为正面，不同API的正面定义可能不同
        bool depth_clip_enable = true;        // 是否启用深度裁剪
        bool scissor_enable = false;       // 是否启用裁剪矩形,ui系统开
        bool antialiased_line_enable = false; // 是否启用线段抗锯齿
        int32_t depth_bias = 0;               // 深度偏移量
        float depth_bias_clamp = 0.0f;       // 深度偏移量最大值
        float slope_scaled_depth_bias = 0.0f; // 深度偏移比例

        constexpr RasterizerStateDesc() noexcept {}

        constexpr RasterizerStateDesc(FillMode _FillMode,
                                    CullMode _CullMode,
                                    bool      _FrontCounterClockwise = RasterizerStateDesc{}.front_counter_clockwise,
                                    bool      _DepthClipEnable       = RasterizerStateDesc{}.depth_clip_enable,
                                    bool      _ScissorEnable         = RasterizerStateDesc{}.scissor_enable,
                                    bool      _AntialiasedLineEnable = RasterizerStateDesc{}.antialiased_line_enable,
                                    int32_t     _DepthBias             = RasterizerStateDesc{}.depth_bias,
                                    float   _DepthBiasClamp        = RasterizerStateDesc{}.depth_bias_clamp,
                                    float   _SlopeScaledDepthBias  = RasterizerStateDesc{}.slope_scaled_depth_bias)noexcept :
            fil_mode              {_FillMode             },
            cull_mode              {_CullMode             },
            front_counter_clockwise {_FrontCounterClockwise},
            depth_clip_enable       {_DepthClipEnable      },
            scissor_enable         {_ScissorEnable        },
            antialiased_line_enable {_AntialiasedLineEnable},
            depth_bias             {_DepthBias            },
            depth_bias_clamp        {_DepthBiasClamp       },
            slope_scaled_depth_bias  {_SlopeScaledDepthBias }
        {}

        // 比较运算符重载，RHI状态对象需要比较以避免重复调用进行优化
        bool operator==(const RasterizerStateDesc& other) const {
            return fil_mode == other.fil_mode &&
                   cull_mode == other.cull_mode &&
                   front_counter_clockwise == other.front_counter_clockwise &&
                   depth_clip_enable == other.depth_clip_enable &&
                   scissor_enable == other.scissor_enable &&
                   antialiased_line_enable == other.antialiased_line_enable &&
                   depth_bias == other.depth_bias &&
                   depth_bias_clamp == other.depth_bias_clamp &&
                   slope_scaled_depth_bias == other.slope_scaled_depth_bias;
        }

        bool operator!=(const RasterizerStateDesc& other) const {
            return !(*this == other);
        }
    };
} // namespace NexAur