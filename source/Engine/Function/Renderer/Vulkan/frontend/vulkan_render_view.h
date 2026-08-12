#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace NexAur {
    // Vulkan-native camera contract. View space remains right-handed with
    // forward -Z, while projection matrices use flipped clip Y and [0, 1]
    // NDC depth. All derived matrices are owned by the Vulkan translator.
    struct VulkanRenderView {
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
