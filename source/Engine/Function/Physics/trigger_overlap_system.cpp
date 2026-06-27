#include "pch.h"
#include "trigger_overlap_system.h"

#include "Function/Physics/collision_query.h"
#include "Function/Scene/collision_component.h"
#include "Function/Scene/component.h"
#include "Function/Scene/scene_v2.h"

#include <algorithm>
#include <vector>

namespace NexAur {
    namespace {
        PhysicsCollisionFilter readFilter(const entt::registry& registry, entt::entity entity) {
            PhysicsCollisionFilter filter;
            if (const auto* component = registry.try_get<CollisionFilterComponent>(entity)) {
                filter.layer = component->layer;
                filter.mask = component->mask;
            }
            return filter;
        }

        bool isTriggerEnabled(const entt::registry& registry, entt::entity entity) {
            const auto* trigger = registry.try_get<TriggerComponent>(entity);
            return trigger && trigger->enabled;
        }

        glm::vec3 absScale(const TransformComponent& transform) {
            return glm::abs(transform.scale);
        }

        float sphereRadiusScale(const TransformComponent& transform) {
            const glm::vec3 scale = absScale(transform);
            return std::max(scale.x, std::max(scale.y, scale.z));
        }

        bool tryBuildSphereCollider(
            const entt::registry& registry,
            entt::entity entity,
            const TransformComponent& transform,
            WorldCollider& collider) {
            const auto* sphere = registry.try_get<SphereColliderComponent>(entity);
            if (!sphere || !sphere->enabled) {
                return false;
            }

            collider.entity = entity;
            collider.shape = PhysicsColliderShape::Sphere;
            collider.center = transform.translation + sphere->offset;
            collider.radius = std::max(0.0f, sphere->radius * sphereRadiusScale(transform));
            collider.half_extents = glm::vec3{ 0.0f };
            collider.trigger = isTriggerEnabled(registry, entity);
            collider.filter = readFilter(registry, entity);
            return true;
        }

        bool tryBuildAABBCollider(
            const entt::registry& registry,
            entt::entity entity,
            const TransformComponent& transform,
            WorldCollider& collider) {
            const auto* aabb = registry.try_get<AABBColliderComponent>(entity);
            if (!aabb || !aabb->enabled) {
                return false;
            }

            collider.entity = entity;
            collider.shape = PhysicsColliderShape::AABB;
            collider.center = transform.translation + aabb->offset;
            collider.radius = 0.0f;
            collider.half_extents = glm::max(glm::vec3{ 0.0f }, aabb->half_extents * absScale(transform));
            collider.trigger = isTriggerEnabled(registry, entity);
            collider.filter = readFilter(registry, entity);
            return true;
        }

        std::vector<WorldCollider> collectColliders(SceneV2& scene) {
            std::vector<WorldCollider> colliders;
            entt::registry& registry = scene.getRegistry();
            auto view = registry.view<TransformComponent>();

            for (entt::entity entity : view) {
                const TransformComponent& transform = view.get<TransformComponent>(entity);
                WorldCollider collider;

                if (tryBuildSphereCollider(registry, entity, transform, collider)
                    || tryBuildAABBCollider(registry, entity, transform, collider)) {
                    colliders.push_back(collider);
                }
            }

            return colliders;
        }
    } // namespace

    void TriggerOverlapSystem::update(SceneV2& scene) {
        m_frame.clear();

        const std::vector<WorldCollider> colliders = collectColliders(scene);
        for (size_t lhs_index = 0; lhs_index < colliders.size(); ++lhs_index) {
            for (size_t rhs_index = lhs_index + 1; rhs_index < colliders.size(); ++rhs_index) {
                const WorldCollider& lhs = colliders[lhs_index];
                const WorldCollider& rhs = colliders[rhs_index];

                if (!lhs.trigger && !rhs.trigger) {
                    continue;
                }

                if (!CollisionQuery::overlaps(lhs, rhs)) {
                    continue;
                }

                m_frame.overlaps.push_back(TriggerOverlap{ lhs.entity, rhs.entity });
            }
        }
    }
} // namespace NexAur
