#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

#include "graphics_types.h"

namespace NexAur {
    static bool SafeStrEqual(const char* Str0, const char* Str1) {
        if ((Str0 == NULL) != (Str1 == NULL))
            return false;

        if (Str0 != NULL && Str1 != NULL)
            return strcmp(Str0, Str1) == 0;

        return true;
    }


    // 着色器语言
    enum class ShaderSourceLanguage: uint32_t {
        Default = 0,
        HLSL,
        GLSL,
        GLSL_VERBATIM, // verbatim原样照搬，引擎不做#version头
        MSL,
        MSL_VERBATIM,
        MTLB,
        WGSL,
        BYTECODE,
        Count
    };


    // 编译器选择
    enum class ShaderCompiler : uint32_t {
        ///     - Direct3D11:      legacy HLSL compiler (FXC)
        ///     - Direct3D12:      legacy HLSL compiler (FXC)
        ///     - OpenGL(ES) GLSL: native compiler
        ///     - OpenGL(ES) HLSL: HLSL2GLSL converter and native compiler
        ///     - Vulkan GLSL:     built-in glslang
        ///     - Vulkan HLSL:     built-in glslang (with limited support for Shader Model 6.x)
        ///     - Metal GLSL/HLSL: built-in glslang (HLSL with limited support for Shader Model 6.x)
        ///     - Metal MSL:       native compiler

        Default = 0,
        GLSLANG,
        DXC,
        FXC,
        Last = FXC,
        Count
    };

    // 着色器编译选项
    enum class ShaderCompileFlags : uint32_t {
        None = 0,

        // 无界数组
        EnableUnboundedArrays = 1u << 0u,

        // 跳过反射，开启后编译速度快
        SkipReflection = 1u << 1u,

        // 异步编译
        AsynChronous = 1u << 2u,

        // 矩阵行优先选项
        PackMatrixRowMajor = 1u << 3u,

        // 跨语言转译
        HLSLToSpirvViaGLSL = 1u << 4u,
        Last = HLSLToSpirvViaGLSL
    };
    ENABLE_ENUM_BITMASK(ShaderCompileFlags);


    // TODO
    enum class CreateShaderSourceInputStreamFlags : uint32_t {
        None = 0x00,
        Slient = 0x01
    };
    ENABLE_ENUM_BITMASK(CreateShaderSourceInputStreamFlags); // 生成位运算


    // Shader编译状态
    enum class ShaderStatus : uint32_t {
        UnInitialized = 0,
        Compiling,
        Ready,
        Failed
    };

    
    // Shader描述符
    struct ShaderDesc : DeviceObjectAttribs{
        // shader类型
        ShaderType shader_type {ShaderType::Unknown};

        // TODO: 纹理与采样器合并开关
        bool use_combine_texture_samplers = false;

        // 后缀名
        const char* combined_sampler_suffix = "_sampler";

        constexpr ShaderDesc() noexcept {}

        constexpr ShaderDesc(const char* _Name,
                         ShaderType _ShaderType,
                         bool        _UseCombinedTextureSamplers = ShaderDesc{}.use_combine_texture_samplers,
                         const char* _CombinedSamplerSuffix      = ShaderDesc{}.combined_sampler_suffix) :
        DeviceObjectAttribs         {_Name},
        shader_type                {_ShaderType},
        use_combine_texture_samplers{_UseCombinedTextureSamplers},
        combined_sampler_suffix     {_CombinedSamplerSuffix}
        {}

        bool operator==(const ShaderDesc& RHS) const noexcept {
            return shader_type                 == RHS.shader_type                 && 
                use_combine_texture_samplers == RHS.use_combine_texture_samplers &&
                SafeStrEqual(combined_sampler_suffix, RHS.combined_sampler_suffix);
        }

        bool operator!=(const ShaderDesc& RHS) const noexcept {
            return !(*this == RHS);
        }
    };
    

    // 抽象接口，解耦读取逻辑和渲染逻辑
    class IShaderSourceFactory {
    public:
        virtual ~IShaderSourceFactory() = default;

        // 核心方法：输入文件名，返回源码字符串
        virtual std::string loadSource(const std::string& filename) = 0;
    };

    // Shader代码中增加宏的结构， ShaderMacro macro = { "ENABLE_SHADOWS", "1" }; -> #define ENABLE_SHADOWS 1
    struct ShaderMacro {
        std::string name = nullptr;
        std::string definition = nullptr;

        constexpr ShaderMacro() noexcept {}

        ShaderMacro(const std::string& _name,
                            const std::string& _def) noexcept :
            name{_name},
            definition{_def}
        {}

        bool operator==(const ShaderMacro& RHS) const noexcept {
            return name == RHS.name && definition == RHS.definition;
        }

        bool operator!=(const ShaderMacro& RHS) const noexcept {
            return !(*this == RHS);
        }
    };

    
    struct ShaderMacroArray {
        std::vector<ShaderMacro> elements;

        ShaderMacroArray() {};
        ShaderMacroArray(const std::initializer_list<ShaderMacro> &init_list) : elements(init_list) {}

        bool operator == (const ShaderMacroArray& rhs) const{
            return elements == rhs.elements;
        }

        bool operator != (const ShaderMacroArray& rhs) const{
            return !(*this == rhs);
        }

        operator bool() const noexcept {
            return !elements.empty();
        }

        const ShaderMacro& operator[](size_t index) const noexcept {
            return elements[index];
        }
    };
} // namespace NexAur
