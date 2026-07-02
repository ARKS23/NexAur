#pragma once

#include <cstdint>
#include <string>

#include "Core/Base.h"

namespace NexAur {
    enum class RendererBackendType {
        Unknown,
        Vulkan,
    };

    enum class ViewportOutputKind {
        None,
        VulkanImGuiTexture,
        ExternalSwapchain,
    };

    enum class ViewportCoordinateOrigin {
        TopLeft,
        BottomLeft,
    };

    // RendererService 对外暴露的 viewport 输出描述。
    // 上层只能根据 kind 选择显示策略，不能解释 native_handle 的内部生命周期。
    struct NEXAUR_API ViewportOutput {
        RendererBackendType backend = RendererBackendType::Unknown;
        ViewportOutputKind kind = ViewportOutputKind::None;

        uint32_t width = 0;
        uint32_t height = 0;
        ViewportCoordinateOrigin coordinate_origin = ViewportCoordinateOrigin::TopLeft;

        // 后端拥有的不透明 texture / descriptor token，上层只传递，不解释。
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

    enum class ReflectionProbeCaptureKind {
        Capture,
        Bake
    };

    enum class ReflectionProbeCaptureStatus {
        Idle,
        Pending,
        Capturing,
        Ready,
        Failed
    };

    struct NEXAUR_API ReflectionProbeCaptureRequest {
        int entity_id = -1;
        uint32_t resolution = 128;
        float near_clip = 0.1f;
        float far_clip = 40.0f;
        bool include_skybox = true;
        ReflectionProbeCaptureKind kind = ReflectionProbeCaptureKind::Capture;
    };

    struct NEXAUR_API ReflectionProbeCaptureState {
        ReflectionProbeCaptureStatus status = ReflectionProbeCaptureStatus::Idle;
        uint32_t resolution = 0;
        bool runtime_resource_ready = false;
        bool include_skybox = true;
        ReflectionProbeCaptureKind last_kind = ReflectionProbeCaptureKind::Capture;
        std::string message = "Idle";
    };
} // namespace NexAur
