#include "pch.h"
#include "runtime_camera_controller_system.h"

#include "Function/Scene/component.h"
#include "Function/Scene/gameplay_component.h"
#include "Function/Scene/scene_v2.h"

#include <glm/gtc/matrix_transform.hpp>

namespace NexAur {
    namespace {
        constexpr float kMinimumAspectRatio = 0.01f;
        constexpr float kMinimumClipDistance = 0.001f;

        bool findPlayerTarget(const entt::registry& registry, glm::vec3& target_position) {
            auto view = registry.view<PlayerComponent, TransformComponent>();
            for (entt::entity entity : view) {
                target_position = view.get<TransformComponent>(entity).translation;
                return true;
            }

            return false;
        }

        float sanitizeAspectRatio(float aspect_ratio) {
            return aspect_ratio > kMinimumAspectRatio ? aspect_ratio : 16.0f / 9.0f;
        }

        float sanitizeFarClip(float near_clip, float far_clip) {
            return far_clip > near_clip + kMinimumClipDistance ? far_clip : near_clip + 100.0f;
        }

        float sanitizeFov(float fov_degrees) {
            return glm::clamp(fov_degrees, 1.0f, 170.0f);
        }

        void updateCamera(
            CameraComponent& camera,
            TransformComponent& transform,
            const RuntimeCameraControllerComponent& controller,
            const glm::vec3& player_position,
            TimeStep delta_time) {
            const glm::vec3 target = player_position + controller.target_offset;
            const glm::vec3 desired_position = target + controller.follow_offset;
            const float seconds = static_cast<float>(delta_time);
            const float blend = controller.follow_speed <= 0.0f
                ? 1.0f
                : glm::clamp(controller.follow_speed * seconds, 0.0f, 1.0f);

            camera.position = glm::mix(camera.position, desired_position, blend);
            if (glm::dot(camera.position - target, camera.position - target) <= 0.0001f) {
                camera.position = desired_position;
            }

            const float near_clip = std::max(camera.nearClip, kMinimumClipDistance);
            const float far_clip = sanitizeFarClip(near_clip, camera.farClip);
            camera.nearClip = near_clip;
            camera.farClip = far_clip;
            camera.viewMatrix = glm::lookAt(camera.position, target, glm::vec3{ 0.0f, 1.0f, 0.0f });
            camera.projectionMatrix = glm::perspective(
                glm::radians(sanitizeFov(controller.fov_degrees)),
                sanitizeAspectRatio(controller.aspect_ratio),
                camera.nearClip,
                camera.farClip);
            camera.viewProjectionMatrix = camera.projectionMatrix * camera.viewMatrix;

            transform.translation = camera.position;
        }
    } // namespace

    void RuntimeCameraControllerSystem::update(SceneV2& scene, TimeStep delta_time) {
        entt::registry& registry = scene.getRegistry();

        glm::vec3 player_position{ 0.0f };
        if (!findPlayerTarget(registry, player_position)) {
            return;
        }

        auto active_camera_view =
            registry.view<CameraComponent, ActiveCameraComponent, RuntimeCameraControllerComponent, TransformComponent>();
        for (entt::entity entity : active_camera_view) {
            const auto& active_camera = active_camera_view.get<ActiveCameraComponent>(entity);
            const auto& controller = active_camera_view.get<RuntimeCameraControllerComponent>(entity);
            if (!active_camera.enabled || !controller.enabled) {
                continue;
            }

            updateCamera(
                active_camera_view.get<CameraComponent>(entity),
                active_camera_view.get<TransformComponent>(entity),
                controller,
                player_position,
                delta_time);
            return;
        }

        auto fallback_camera_view = registry.view<CameraComponent, RuntimeCameraControllerComponent, TransformComponent>();
        for (entt::entity entity : fallback_camera_view) {
            const auto& controller = fallback_camera_view.get<RuntimeCameraControllerComponent>(entity);
            if (!controller.enabled) {
                continue;
            }

            updateCamera(
                fallback_camera_view.get<CameraComponent>(entity),
                fallback_camera_view.get<TransformComponent>(entity),
                controller,
                player_position,
                delta_time);
            return;
        }
    }
} // namespace NexAur
