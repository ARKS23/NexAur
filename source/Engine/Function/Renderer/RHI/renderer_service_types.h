#pragma once

#include <cstdint>

#include "Core/Base.h"

namespace NexAur {
    enum class RendererBackendType {
        Unknown,
        OpenGLLegacy,
        ArkVulkan,
    };

    enum class ViewportOutputKind {
        None,
        OpenGLTexture,
        VulkanImGuiTexture,
        ExternalSwapchain,
    };

    // RendererService 对外暴露的 viewport 输出描述。
    // 上层只能根据 kind 选择显示策略，不能解释 native_handle 的内部生命周期。
    struct NEXAUR_API ViewportOutput {
        RendererBackendType backend = RendererBackendType::Unknown;
        ViewportOutputKind kind = ViewportOutputKind::None;

        uint32_t width = 0;
        uint32_t height = 0;

        // OpenGL legacy 使用 numeric_handle 存 texture id。
        uint64_t numeric_handle = 0;

        // Vulkan 后续可使用不透明 token 交给 UI bridge 处理。
        void* native_handle = nullptr;

        bool valid() const { return kind != ViewportOutputKind::None && width > 0 && height > 0; }
    };

    struct NEXAUR_API ViewportPickRequest {
        int x = 0;
        int y = 0;
    };

    struct NEXAUR_API ViewportPickResult {
        bool supported = false;
        bool ready = false;
        int entity_id = -1;
    };
} // namespace NexAur
