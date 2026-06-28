#pragma once

#include <glm/glm.hpp>

namespace NexAur {
    enum class RuntimeCameraMode {
        ThirdPersonFollow,
        FirstPerson
    };

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

    struct ProjectileComponent {
        int damage = 1;
    };

    struct LifetimeComponent {
        float seconds = 1.0f;
    };

    // Runtime camera data stays with gameplay components for v1. Split this
    // into runtime_camera_component.h once the gameplay component set grows.
    struct RuntimeCameraControllerComponent {
        RuntimeCameraMode mode = RuntimeCameraMode::ThirdPersonFollow;
        glm::vec3 follow_offset{ 0.0f, 3.0f, 6.0f };
        glm::vec3 target_offset{ 0.0f, 1.0f, 0.0f };
        float follow_speed = 10.0f;
        float fov_degrees = 45.0f;
        float aspect_ratio = 16.0f / 9.0f;
        bool enabled = true;
    };
} // namespace NexAur
