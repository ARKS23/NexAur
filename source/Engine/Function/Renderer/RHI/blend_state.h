#pragma once
#include <cstdint>

#include "constant.h"

namespace NexAur {
    enum class BlendFactor : int8_t {
        Undefined = 0,
        Zero,
        One,
        SrcColor,
        InvSrcColor,
        SrcAlpha,
        InvSrcAlpha,
        DestAlpha,
        InvDestAlpha,
        SrcAlphaSat,
        BlendFactor,
        InvBlendFactor,
        Src1Color,
        InvSrc1Color,
        Src1Alpha,
        InvSrc1Alpha,
        NumFactors
    };

    enum class BlendOperation : int8_t {
        Undefined = 0,
        Add,
        Substract,
        RevSubstract,
        Min,
        Max,
        NumOperations
    };

    enum class LogicOperation : int8_t {
        Clear = 0,
        Set,
        Copy,
        CopyInverted,
        Noop,
        Invert,
        And,
        Nand,
        Or,
        Nor,
        Xor,
        Equiv,
        AndReverse,
        AndInverted,
        Reverse,
        Inverted,
        NumOperations
    };

    enum class ColorMask : uint8_t {
        None = 0u,
        Red = 1u << 0u,
        Green = 1u << 1u,
        Blue = 1u << 2u,
        Alpha = 1u << 3u,
        RGB = Red | Green | Blue,
        ALL = RGB | Alpha
    };

    struct RenderTargetBlendDesc {
        // 混合总开关
        bool blend_enable = false;
        bool logic_operation_enable = false;

        // RGB混合参数
        BlendFactor src_blend = BlendFactor::One;
        BlendFactor dest_blend = BlendFactor::Zero;
        BlendOperation blend_op = BlendOperation::Add;

        // Alpha混合参数
        BlendFactor src_blend_alpha = BlendFactor::One;
        BlendFactor dest_blend_alpha = BlendFactor::Zero;
        BlendOperation blend_op_alpha = BlendOperation::Add;

        // 不进行加减乘除的颜色混合，进行位运算
        LogicOperation logic_op = LogicOperation::Noop;

        // 颜色写入参数
        ColorMask render_target_write_mask = ColorMask::ALL;

        constexpr RenderTargetBlendDesc() noexcept {}

        explicit constexpr
        RenderTargetBlendDesc(bool            _BlendEnable,
                            bool            _LogicOperationEnable  = RenderTargetBlendDesc{}.logic_operation_enable,
                            BlendFactor    _SrcBlend              = RenderTargetBlendDesc{}.src_blend,
                            BlendFactor    _DestBlend             = RenderTargetBlendDesc{}.dest_blend,
                            BlendOperation _BlendOp               = RenderTargetBlendDesc{}.blend_op,
                            BlendFactor    _SrcBlendAlpha         = RenderTargetBlendDesc{}.src_blend_alpha,
                            BlendFactor    _DestBlendAlpha        = RenderTargetBlendDesc{}.dest_blend_alpha,
                            BlendOperation _BlendOpAlpha          = RenderTargetBlendDesc{}.blend_op_alpha,
                            LogicOperation _LogicOp               = RenderTargetBlendDesc{}.logic_op,
                            ColorMask      _RenderTargetWriteMask = RenderTargetBlendDesc{}.render_target_write_mask) :
            blend_enable          {_BlendEnable          },
            logic_operation_enable {_LogicOperationEnable },
            src_blend             {_SrcBlend             },
            dest_blend            {_DestBlend            },
            blend_op              {_BlendOp              },
            src_blend_alpha        {_SrcBlendAlpha        },
            dest_blend_alpha       {_DestBlendAlpha       },
            blend_op_alpha         {_BlendOpAlpha         },
            logic_op              {_LogicOp              },
            render_target_write_mask{_RenderTargetWriteMask}
        {}

        /// Comparison operator tests if two structures are equivalent

        /// \param [in] rhs - reference to the structure to perform comparison with
        /// \return
        /// - True if all members of the two structures are equal.
        /// - False otherwise
        constexpr bool operator == (const RenderTargetBlendDesc& rhs) const
        {
            return blend_enable           == rhs.blend_enable    &&
                logic_operation_enable  == rhs.logic_operation_enable &&
                src_blend              == rhs.src_blend       &&
                dest_blend             == rhs.dest_blend      &&
                blend_op               == rhs.blend_op        &&
                src_blend_alpha         == rhs.src_blend_alpha  &&
                dest_blend_alpha        == rhs.dest_blend_alpha &&
                blend_op_alpha          == rhs.blend_op_alpha   &&
                logic_op               == rhs.logic_op		   &&
                render_target_write_mask == rhs.render_target_write_mask;
        }

        bool operator==(const RenderTargetBlendDesc& rhs) const {
            return blend_enable == rhs.blend_enable &&
                    logic_operation_enable == rhs.logic_operation_enable &&
                    src_blend == rhs.src_blend && 
                    dest_blend == rhs.dest_blend &&
                    blend_op == rhs.blend_op &&
                    src_blend_alpha == rhs.src_blend_alpha &&
                    dest_blend_alpha == rhs.dest_blend_alpha &&
                    blend_op_alpha == rhs.blend_op_alpha &&
                    logic_op == rhs.logic_op &&
                    render_target_write_mask == rhs.render_target_write_mask;
        }

        bool operator!=(const RenderTargetBlendDesc& rhs) const {
            return !(*this == rhs);
        }
    };

    struct BlendStateDesc {
        // 透明度抗锯齿开关
        bool alpha_to_coverage_enable = false;

        // 独立混合开关
        bool independent_blend_enable = false;

        // 存放RT配置的容器
        RenderTargetBlendDesc render_targets[RHI::MAX_RENDER_TARGETS];

        constexpr BlendStateDesc() noexcept {}

        constexpr BlendStateDesc(bool                         _AlphaToCoverageEnable,
                                bool                         _IndependentBlendEnable,
                                const RenderTargetBlendDesc& RT0 = RenderTargetBlendDesc{}) noexcept :
            alpha_to_coverage_enable   {_AlphaToCoverageEnable },
            independent_blend_enable  {_IndependentBlendEnable},
            render_targets           {RT0}
        {}

        bool operator==(const BlendStateDesc& rhs) const {
            bool b_enbale_equal = alpha_to_coverage_enable == rhs.alpha_to_coverage_enable &&
                        independent_blend_enable == rhs.independent_blend_enable;

            bool b_RTS_equal = true;
            for (size_t i = 0; i < RHI::MAX_RENDER_TARGETS; ++i) {
                if (render_targets[i] != rhs.render_targets[i]) {
                    b_RTS_equal = false;
                    break;
                }
            }

            return b_enbale_equal && b_RTS_equal;
        }

        bool operator!=(const BlendStateDesc& rhs) const {
            return !(*this == rhs);
        }
    };
} // namespace NexAur
