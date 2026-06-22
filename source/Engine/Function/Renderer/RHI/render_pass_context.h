#pragma once

#include <cstdint>
#include <memory>

#include "Core/Base.h"

namespace NexAur {
    class Framebuffer;

    // Pass 执行期的最小上下文。
    // 它只描述当前帧的渲染目标和视口尺寸，避免 Pass 自己去全局上下文里寻找 RendererSystem/WindowSystem。
    struct NEXAUR_API RenderPassContext {
        std::shared_ptr<Framebuffer> viewport_framebuffer;
        uint32_t viewport_width = 1;
        uint32_t viewport_height = 1;
    };
} // namespace NexAur
