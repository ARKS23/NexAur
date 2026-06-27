#pragma once

#include <glm/glm.hpp>

namespace NexAur {
    struct PlayerComponent {
        float move_speed = 5.0f;
    };

    struct EnemyComponent {
        float move_speed = 2.0f;
    };

    struct VelocityComponent {
        glm::vec3 velocity{ 0.0f };
    };

    struct HealthComponent {
        int current = 1;
        int max = 1;
    };

    struct CollectibleComponent {
        int score = 1;
    };

    struct LifetimeComponent {
        float seconds = 1.0f;
    };
} // namespace NexAur
