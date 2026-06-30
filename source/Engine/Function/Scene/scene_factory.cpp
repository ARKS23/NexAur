#include "pch.h"
#include "scene_factory.h"

#include "component.h"
#include "entity.h"
#include "scene_v2.h"
#include "Function/File/file_system.h"
#include "Function/Resource/asset_manager.h"

#include <filesystem>

namespace NexAur {
    namespace {
        constexpr float kDefaultCameraFov = 45.0f;
        constexpr float kDefaultAspectRatio = 1280.0f / 720.0f;
        constexpr float kDefaultNearClip = 0.1f;
        constexpr float kDefaultFarClip = 100.0f;

        void addDefaultCamera(const std::shared_ptr<SceneV2>& scene) {
            Entity camera_entity = scene->createEntity("MainCamera");
            auto& camera = camera_entity.addComponent<CameraComponent>();
            camera.position = { 0.0f, 0.0f, 5.0f };
            camera.nearClip = kDefaultNearClip;
            camera.farClip = kDefaultFarClip;
            camera.viewMatrix = glm::lookAt(
                camera.position,
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));
            camera.projectionMatrix = glm::perspective(
                glm::radians(kDefaultCameraFov),
                kDefaultAspectRatio,
                kDefaultNearClip,
                kDefaultFarClip);
            camera.viewProjectionMatrix = camera.projectionMatrix * camera.viewMatrix;
            camera_entity.addComponent<ActiveCameraComponent>();
        }

        std::string defaultEnvironmentPath() {
            const std::string lightweight_hdr = NX_ASSET("assets/textures/HDR/2k.hdr");
            if (std::filesystem::exists(lightweight_hdr)) {
                return lightweight_hdr;
            }

            return NX_ASSET("assets/textures/HDR/warm_restaurant_8k.hdr");
        }

        void addDefaultEnvironment(const std::shared_ptr<SceneV2>& scene) {
            AssetManager& asset_manager = AssetManager::getInstance();
            AssetHandle environment_asset = asset_manager.importEnvironmentMapAsset(defaultEnvironmentPath());
            if (!environment_asset) return;

            Entity environment_entity = scene->createEntity("Environment");
            auto& environment = environment_entity.addComponent<EnvironmentComponent>(environment_asset);
            environment.intensity = 0.65f;
            environment.skybox_intensity = 0.75f;
            environment.ibl_intensity = 0.65f;
        }

        void addDefaultLights(const std::shared_ptr<SceneV2>& scene) {
            Entity directional_light_entity = scene->createEntity("DirectionalLight");
            auto& directional_light = directional_light_entity.addComponent<DirectionalLightComponent>();
            directional_light.direction = glm::normalize(glm::vec3(-0.6f, -0.6f, 0.6f));
            directional_light.color = glm::vec3(1.0f);
            directional_light.intensity = 2.0f;
            directional_light.cast_shadow = true;
            directional_light.shadow_strength = 0.7f;
            directional_light.shadow_bias = 0.003f;
            directional_light.shadow_distance = 35.0f;

            Entity point_light_entity = scene->createEntity("PointLight");
            auto& point_light = point_light_entity.addComponent<PointLightComponent>();
            point_light.color = glm::vec3(1.0f);
            point_light.intensity = 1.0f;
            point_light.constant = 1.0f;
            point_light.linear = 0.09f;
            point_light.quadratic = 0.032f;

            auto& point_light_transform = point_light_entity.getComponent<TransformComponent>();
            point_light_transform.translation = glm::vec3(-5.0f, 0.0f, 0.0f);
        }
    } // namespace

    std::shared_ptr<SceneV2> SceneFactory::createDefaultScene() {
        auto scene = std::make_shared<SceneV2>();

        // 默认场景只描述运行时对象；编辑器相机由 EditorContext 独立维护。
        addDefaultCamera(scene);
        addDefaultEnvironment(scene);
        addDefaultLights(scene);

        return scene;
    }
} // namespace NexAur
