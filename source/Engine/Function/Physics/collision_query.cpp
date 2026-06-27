#include "pch.h"
#include "collision_query.h"

namespace NexAur::CollisionQuery {
    namespace {
        bool overlapsSphereSphere(const WorldCollider& lhs, const WorldCollider& rhs) {
            const glm::vec3 delta = lhs.center - rhs.center;
            const float radius = lhs.radius + rhs.radius;
            return glm::dot(delta, delta) <= radius * radius;
        }

        bool overlapsAABBAABB(const WorldCollider& lhs, const WorldCollider& rhs) {
            const glm::vec3 lhs_min = lhs.center - lhs.half_extents;
            const glm::vec3 lhs_max = lhs.center + lhs.half_extents;
            const glm::vec3 rhs_min = rhs.center - rhs.half_extents;
            const glm::vec3 rhs_max = rhs.center + rhs.half_extents;

            return lhs_min.x <= rhs_max.x && lhs_max.x >= rhs_min.x
                && lhs_min.y <= rhs_max.y && lhs_max.y >= rhs_min.y
                && lhs_min.z <= rhs_max.z && lhs_max.z >= rhs_min.z;
        }

        bool overlapsSphereAABB(const WorldCollider& sphere, const WorldCollider& aabb) {
            const glm::vec3 aabb_min = aabb.center - aabb.half_extents;
            const glm::vec3 aabb_max = aabb.center + aabb.half_extents;
            const glm::vec3 closest = glm::clamp(sphere.center, aabb_min, aabb_max);
            const glm::vec3 delta = sphere.center - closest;
            return glm::dot(delta, delta) <= sphere.radius * sphere.radius;
        }
    } // namespace

    bool passesFilter(const WorldCollider& lhs, const WorldCollider& rhs) {
        return (lhs.filter.mask & rhs.filter.layer) != 0u
            && (rhs.filter.mask & lhs.filter.layer) != 0u;
    }

    bool overlaps(const WorldCollider& lhs, const WorldCollider& rhs) {
        if (!passesFilter(lhs, rhs)) {
            return false;
        }

        if (lhs.shape == PhysicsColliderShape::Sphere && rhs.shape == PhysicsColliderShape::Sphere) {
            return overlapsSphereSphere(lhs, rhs);
        }

        if (lhs.shape == PhysicsColliderShape::AABB && rhs.shape == PhysicsColliderShape::AABB) {
            return overlapsAABBAABB(lhs, rhs);
        }

        if (lhs.shape == PhysicsColliderShape::Sphere && rhs.shape == PhysicsColliderShape::AABB) {
            return overlapsSphereAABB(lhs, rhs);
        }

        return overlapsSphereAABB(rhs, lhs);
    }
} // namespace NexAur::CollisionQuery
