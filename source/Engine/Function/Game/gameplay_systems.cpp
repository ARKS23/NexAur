#include "pch.h"
#include "gameplay_systems.h"

#include "Function/Input/input_action_system.h"
#include "Function/Scene/component.h"
#include "Function/Scene/entity.h"
#include "Function/Scene/gameplay_component.h"
#include "Function/Scene/scene_v2.h"

#include <vector>

namespace NexAur {
    namespace {
        void destroyEntities(SceneV2& scene, const std::vector<entt::entity>& entities) {
            entt::registry& registry = scene.getRegistry();
            for (entt::entity entity : entities) {
                if (registry.valid(entity)) {
                    scene.destroyEntity(Entity(entity, &scene));
                }
            }
        }

        bool findPlayerPosition(const entt::registry& registry, glm::vec3& position) {
            auto view = registry.view<PlayerComponent, TransformComponent>();
            for (entt::entity entity : view) {
                position = view.get<TransformComponent>(entity).translation;
                return true;
            }

            return false;
        }
    } // namespace

    void PlayerControlSystem::update(SceneV2& scene, const InputActionService& input_actions, TimeStep delta_time) {
        (void)delta_time;

        const glm::vec2 move = input_actions.getAxis2D(DefaultInputActions::Move);
        entt::registry& registry = scene.getRegistry();
        auto view = registry.view<PlayerComponent, VelocityComponent>();

        view.each([&](const PlayerComponent& player, VelocityComponent& velocity) {
            velocity.velocity.x = move.x * player.move_speed;
            velocity.velocity.z = -move.y * player.move_speed;
        });
    }

    void EnemySystem::update(SceneV2& scene, TimeStep delta_time) {
        (void)delta_time;

        entt::registry& registry = scene.getRegistry();
        glm::vec3 player_position{ 0.0f };
        if (!findPlayerPosition(registry, player_position)) {
            return;
        }

        auto view = registry.view<EnemyComponent, TransformComponent, VelocityComponent>();
        view.each([&](const EnemyComponent& enemy, const TransformComponent& transform, VelocityComponent& velocity) {
            glm::vec3 direction = player_position - transform.translation;
            direction.y = 0.0f;

            const float distance_squared = glm::dot(direction, direction);
            if (distance_squared <= 0.0001f) {
                velocity.velocity.x = 0.0f;
                velocity.velocity.z = 0.0f;
                return;
            }

            const glm::vec3 planar_velocity = glm::normalize(direction) * enemy.move_speed;
            velocity.velocity.x = planar_velocity.x;
            velocity.velocity.z = planar_velocity.z;
        });
    }

    void MovementSystem::update(SceneV2& scene, TimeStep delta_time) {
        entt::registry& registry = scene.getRegistry();
        const float seconds = static_cast<float>(delta_time);
        auto view = registry.view<TransformComponent, VelocityComponent>();

        view.each([seconds](TransformComponent& transform, const VelocityComponent& velocity) {
            transform.translation += velocity.velocity * seconds;
        });
    }

    void LifetimeSystem::update(SceneV2& scene, TimeStep delta_time) {
        entt::registry& registry = scene.getRegistry();
        const float seconds = static_cast<float>(delta_time);
        std::vector<entt::entity> expired_entities;

        auto view = registry.view<LifetimeComponent>();
        for (entt::entity entity : view) {
            LifetimeComponent& lifetime = view.get<LifetimeComponent>(entity);
            lifetime.seconds -= seconds;
            if (lifetime.seconds <= 0.0f) {
                expired_entities.push_back(entity);
            }
        }

        destroyEntities(scene, expired_entities);
    }

    void HealthSystem::update(SceneV2& scene) {
        entt::registry& registry = scene.getRegistry();
        std::vector<entt::entity> dead_entities;

        auto view = registry.view<HealthComponent>();
        for (entt::entity entity : view) {
            const HealthComponent& health = view.get<HealthComponent>(entity);
            if (health.current <= 0) {
                dead_entities.push_back(entity);
            }
        }

        destroyEntities(scene, dead_entities);
    }
} // namespace NexAur
