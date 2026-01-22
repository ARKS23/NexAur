#pragma once
#include "graphics_types.h"

namespace NexAur {
    // 模板测试操作枚举
    enum class StencilOp : int8_t {
        Undefined = 0,
        Keep,
        Zero,
        Replace,
        IncreaseSta,    // 饱和封顶，最大值255
        DecreaseSta,
        Invert,         // 按位取反
        IncreaseWrap,   // 超过最大值从0开始
        DecreaseWrap,
        NumOps
    };

    
    // 模板测试描述结构体: 记录模板/测试函数，以及对应的操作
    struct StencilOpDesc {
        StencilOp stencil_fail_op = StencilOp::Keep;
        StencilOp stencil_depth_fail_op = StencilOp::Keep;
        StencilOp stencil_pass_op = StencilOp::Keep;
        ComparisonFunction stencil_func = ComparisonFunction::always;

        constexpr StencilOpDesc() noexcept {}

        constexpr StencilOpDesc(StencilOp          _StencilFailOp,
                                StencilOp          _StencilDepthFailOp,
                                StencilOp          _StencilPassOp,
                                ComparisonFunction _StencilFunc) noexcept :
            stencil_fail_op      {_StencilFailOp     },
            stencil_depth_fail_op {_StencilDepthFailOp},
            stencil_pass_op      {_StencilPassOp     },
            stencil_func        {_StencilFunc       }
        {}

        constexpr bool operator==(const StencilOpDesc& rhs) const {
            return stencil_fail_op      == rhs.stencil_fail_op      &&
                    stencil_depth_fail_op == rhs.stencil_depth_fail_op &&
                    stencil_pass_op      == rhs.stencil_pass_op      &&
                    stencil_func        == rhs.stencil_func;
        }

        constexpr bool operator!=(const StencilOpDesc& rhs) const {
            return !(*this == rhs);
        }
    };


    // 模板/深度测试描述结构体
    struct DepthStencilStateDesc {
        bool depth_enable = true;       // 深度测试开关
        bool depth_write_enable = true; // 深度写入开关
        ComparisonFunction depth_func = ComparisonFunction::less;  // 深度比较函数

        bool stencil_enable = false;    // 模板测试开关
        uint8_t stencil_read_mask = 0xff;   // 读掩码
        uint8_t stencil_write_mask = 0xff;  // 写掩码
        StencilOpDesc front_face;   // 模板测试 (正面规则)
        StencilOpDesc back_face;    // 模板测试 (反面规则)

        constexpr DepthStencilStateDesc() noexcept {}

        constexpr DepthStencilStateDesc(bool                _DepthEnable,
                                        bool                _DepthWriteEnable,
                                        ComparisonFunction _DepthFunc        = DepthStencilStateDesc{}.depth_func,
                                        bool                _StencilEnable    = DepthStencilStateDesc{}.stencil_enable,
                                        uint8_t               _StencilReadMask  = DepthStencilStateDesc{}.stencil_read_mask,
                                        uint8_t               _StencilWriteMask = DepthStencilStateDesc{}.stencil_write_mask,
                                        StencilOpDesc       _FrontFace        = StencilOpDesc{},
                                        StencilOpDesc       _BackFace         = StencilOpDesc{}) noexcept :
            depth_enable     {_DepthEnable     },
            depth_write_enable{_DepthWriteEnable},
            depth_func       {_DepthFunc       },
            stencil_enable   {_StencilEnable   },
            stencil_read_mask {_StencilReadMask },
            stencil_write_mask{_StencilWriteMask},
            front_face       {_FrontFace       },
            back_face        {_BackFace        }
        {}

        constexpr bool operator==(const DepthStencilStateDesc& rhs) const {
            return depth_enable == rhs.depth_enable &&
                    depth_write_enable == rhs.depth_write_enable && 
                    depth_func == rhs.depth_func &&
                    stencil_enable == rhs.stencil_enable &&
                    stencil_read_mask == rhs.stencil_read_mask &&
                    stencil_write_mask == rhs.stencil_write_mask && 
                    front_face == rhs.front_face &&
                    back_face == rhs.back_face;
        }

        constexpr bool operator!=(const DepthStencilStateDesc& rhs) const {
            return !(*this == rhs);
        }
    };
    
} // namespace NexAur