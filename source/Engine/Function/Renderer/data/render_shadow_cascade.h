#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

namespace NexAur {
    constexpr uint32_t kMaxRenderShadowCascadeCount = 4;

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
} // namespace NexAur
