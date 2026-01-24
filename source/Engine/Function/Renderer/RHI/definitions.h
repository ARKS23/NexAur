#pragma once

#include <cstdint>

namespace NexAur {
    // 类型枚举
    enum class ShaderDataType {
        None = 0, 
        Float, 
        Float2, 
        Float3, 
        Float4, 
        Mat3, 
        Mat4, 
        Int, 
        Int2, 
        Int3, 
        Int4, 
        Bool
    };


    // 纹理数据格式
    enum class ImageFormat {
        None = 0,

        R8,         // 8位单通道 (用于字体掩码、简单噪声)
        RGB8,       // 24位颜色 (一般不用，显卡不喜欢3字节对齐，通常会用 RGBA8)
        RGBA8,      // 32位颜色 (最通用的纹理格式)

        RGB16F,     // 半精度浮点
        RGBA16F,    // 半精度浮点带 Alpha (业界标准的 HDR 渲染目标格式)
        RGBA32F,    // 全精度浮点

        Depth24Stencil8, // 最常用的深度+模板格式 (D24S8)
        Depth32F         // 32位浮点深度 (做 VSM 或高精度阴影时用)
    };


    // 帧缓冲纹理
    enum class FramebufferTextureFormat {
        None = 0,

        RGBA8,          // 普通颜色 (Albedo)，范围 [0, 1]
        RED_INTEGER,    // 鼠标拾取实体 ID (int)

        RGBA16F,        // 半精度浮点。用于 HDR 光照积累、Bloom、法线贴图。
        RGBA32F,        // 全精度浮点。用于 G-Buffer 存 World Position (世界坐标需要高精度)。
        
        DEPTH24STENCIL8, // 标准深度+模板

        Depth = DEPTH24STENCIL8 // 默认
    };


    // 纹理过滤模式
    enum class TextureFilter {
        None = 0,
        Linear, // 线性插值
        Nearest, // 邻近采样
        NearestMipmapNearest, // 无论层级还是像素都选最近的 (性能最高，效果最差)
        LinearMipmapNearest,  // 线性插值像素，但层级选最近的
        NearestMipmapLinear,  // 像素选最近，但层级之间插值
        LinearMipmapLinear    // 三线性过滤
    };


    // 纹理环绕模式
    enum class TextureWrap {
        None = 0,
        Repeat,         // 重复
        ClampToEdge,    // 边缘拉伸
        ClampToBorder,  // 超出部分使用纯色(shadow mapping需要用到)
        MirroredRepeat  // 镜像重复 AB BA AB BA
    };


    // 深度比较枚举
    enum class DepthFunc {
        None = 0, 
        Never, 
        Less, 
        Equal, 
        Lequal, 
        Greater, 
        Notequal, 
        Gequal, 
        Always
    };
} // namespace NexAur
