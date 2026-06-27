#pragma once

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace NexAur {
    enum class PhysicsColliderShape {
        Sphere,
        AABB
    };

    struct PhysicsCollisionFilter {
        uint32_t layer = 1u;
        uint32_t mask = 0xffffffffu;
    };

    struct WorldCollider {
        entt::entity entity{ entt::null };
        PhysicsColliderShape shape = PhysicsColliderShape::Sphere;
        glm::vec3 center{ 0.0f };
        glm::vec3 half_extents{ 0.0f };
        float radius = 0.0f;
        bool trigger = false;
        PhysicsCollisionFilter filter;
    };

    struct TriggerOverlap {
        entt::entity first{ entt::null };
        entt::entity second{ entt::null };
    };

    struct TriggerOverlapFrame {
        std::vector<TriggerOverlap> overlaps;

        void clear() {
            overlaps.clear();
        }
    };
} // namespace NexAur
