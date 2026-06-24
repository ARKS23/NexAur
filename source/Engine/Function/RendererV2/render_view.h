#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "Core/Base.h"

namespace NexAur {
    struct NEXAUR_API RenderView {
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
