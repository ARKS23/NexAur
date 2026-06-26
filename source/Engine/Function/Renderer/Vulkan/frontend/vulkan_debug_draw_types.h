#pragma once

#include <glm/glm.hpp>

namespace NexAur {
    struct VulkanDebugDrawVertex {
        glm::vec3 position{ 0.0f };
        glm::vec4 color{ 1.0f };
    };
} // namespace NexAur
