#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace NexAur {
    // Backend-neutral camera contract:
    // - right-handed view space, forward -Z, up +Y;
    // - clip-space Y points up and NDC depth is [-1, 1];
    // - GLM column-major matrices transform as projection * view * position.
    // RenderSceneFrameBuilder owns validation and all derived matrices. Backends
    // convert this view to their native clip convention at their boundary.
    struct RenderView {
        glm::mat4 view_matrix{ 1.0f };
        glm::mat4 projection_matrix{ 1.0f };
        glm::mat4 view_projection_matrix{ 1.0f };

        glm::mat4 inverse_view_matrix{ 1.0f };
        glm::mat4 inverse_projection_matrix{ 1.0f };

        glm::vec3 camera_position{ 0.0f };

        float near_clip = 0.1f;
        float far_clip = 1000.0f;

        uint32_t viewport_width = 1;
        uint32_t viewport_height = 1;
    };
} // namespace NexAur
