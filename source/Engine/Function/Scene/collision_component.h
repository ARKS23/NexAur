#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace NexAur {
    struct SphereColliderComponent {
        glm::vec3 offset{ 0.0f };
        float radius = 0.5f;
        bool enabled = true;
    };

    struct AABBColliderComponent {
        glm::vec3 offset{ 0.0f };
        glm::vec3 half_extents{ 0.5f };
        bool enabled = true;
    };

    struct TriggerComponent {
        bool enabled = true;
    };

    struct CollisionFilterComponent {
        uint32_t layer = 1u;
        uint32_t mask = 0xffffffffu;
    };
} // namespace NexAur
