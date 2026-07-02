#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "Function/Renderer/data/render_settings.h"

namespace NexAur {
    constexpr uint32_t kMaxRenderShadowCascadeCount = 4;
    constexpr uint32_t kMaxRenderPointShadowFaceCount =
        kMaxRenderPointShadowLights * kRenderPointShadowCubeFaceCount;

    struct RenderShadowCascadeFrame {
        std::array<glm::mat4, kMaxRenderShadowCascadeCount> light_view_projections{
            glm::mat4{ 1.0f },
            glm::mat4{ 1.0f },
            glm::mat4{ 1.0f },
            glm::mat4{ 1.0f }
        };
        std::array<float, kMaxRenderShadowCascadeCount> split_depths{ 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<glm::vec4, kMaxRenderShadowCascadeCount> debug_colors{
            glm::vec4{ 0.95f, 0.25f, 0.20f, 1.0f },
            glm::vec4{ 0.20f, 0.85f, 0.30f, 1.0f },
            glm::vec4{ 0.20f, 0.45f, 1.00f, 1.0f },
            glm::vec4{ 1.00f, 0.80f, 0.20f, 1.0f }
        };

        uint32_t cascade_count = 1;
        bool cascades_enabled = false;
        bool debug_overlay = false;
    };

    struct RenderPointShadowFrame {
        std::array<glm::mat4, kMaxRenderPointShadowFaceCount> light_view_projections{};
        uint32_t shadowed_light_count = 0;
        uint32_t face_count = 0;
        bool enabled = false;
    };

    struct RenderRectShadowFrame {
        std::array<glm::mat4, kMaxRenderRectShadowLights> light_view_projections{};
        uint32_t shadowed_light_count = 0;
        bool enabled = false;
    };
} // namespace NexAur
