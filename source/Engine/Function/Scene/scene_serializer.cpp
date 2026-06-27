#include "pch.h"
#include "scene_serializer.h"

#include "Core/Log/log_system.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Scene/collision_component.h"
#include "Function/Scene/component.h"
#include "Function/Scene/entity.h"
#include "Function/Scene/gameplay_component.h"
#include "Function/Scene/scene_v2.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace NexAur {
    namespace {
        using json = nlohmann::json;

        constexpr int kSceneVersion = 1;
        constexpr const char* kSceneFormat = "NexAurScene";

        SceneSerializationResult makeSaveResult(bool success, std::string message, size_t entity_count = 0) {
            SceneSerializationResult result;
            result.success = success;
            result.message = std::move(message);
            result.entity_count = entity_count;
            return result;
        }

        SceneLoadResult makeLoadResult(
            bool success,
            std::string message,
            std::shared_ptr<SceneV2> scene = nullptr,
            size_t entity_count = 0) {
            SceneLoadResult result;
            result.success = success;
            result.message = std::move(message);
            result.scene = std::move(scene);
            result.entity_count = entity_count;
            return result;
        }

        json writeVec3(const glm::vec3& value) {
            return json::array({ value.x, value.y, value.z });
        }

        glm::vec3 readVec3(const json& value, const glm::vec3& fallback = glm::vec3{ 0.0f }) {
            if (!value.is_array() || value.size() != 3) {
                return fallback;
            }

            return {
                value[0].get<float>(),
                value[1].get<float>(),
                value[2].get<float>(),
            };
        }

        json writeMat4(const glm::mat4& value) {
            json columns = json::array();
            for (int column = 0; column < 4; ++column) {
                columns.push_back(json::array({
                    value[column][0],
                    value[column][1],
                    value[column][2],
                    value[column][3],
                }));
            }
            return columns;
        }

        glm::mat4 readMat4(const json& value, const glm::mat4& fallback = glm::mat4{ 1.0f }) {
            if (!value.is_array() || value.size() != 4) {
                return fallback;
            }

            glm::mat4 matrix{ 1.0f };
            for (int column = 0; column < 4; ++column) {
                if (!value[column].is_array() || value[column].size() != 4) {
                    return fallback;
                }

                for (int row = 0; row < 4; ++row) {
                    matrix[column][row] = value[column][row].get<float>();
                }
            }

            return matrix;
        }

        const char* writeRuntimeCameraMode(RuntimeCameraMode mode) {
            switch (mode) {
            case RuntimeCameraMode::ThirdPersonFollow:
                return "ThirdPersonFollow";
            case RuntimeCameraMode::FirstPerson:
                return "FirstPerson";
            default:
                return "ThirdPersonFollow";
            }
        }

        RuntimeCameraMode readRuntimeCameraMode(const std::string& value, RuntimeCameraMode fallback) {
            if (value == "ThirdPersonFollow") {
                return RuntimeCameraMode::ThirdPersonFollow;
            }
            if (value == "FirstPerson") {
                return RuntimeCameraMode::FirstPerson;
            }
            return fallback;
        }

        json writeAssetReference(AssetHandle handle, const AssetManager& asset_manager) {
            json asset;
            asset["uuid"] = std::to_string(static_cast<uint64_t>(handle.id));

            const AssetMetadata* metadata = asset_manager.getMetadata(handle);
            if (!metadata) {
                asset["asset_type"] = assetTypeToString(AssetType::Unknown);
                asset["path"] = "";
                asset["debug_name"] = "";
                asset["runtime_generated"] = false;
                return asset;
            }

            asset["asset_type"] = assetTypeToString(metadata->type);
            asset["path"] = metadata->path;
            asset["debug_name"] = metadata->debug_name;
            asset["runtime_generated"] = metadata->runtime_generated;
            return asset;
        }

        AssetType readAssetType(const json& asset) {
            if (!asset.is_object()) {
                return AssetType::Unknown;
            }

            const std::string type = asset.value("asset_type", "Unknown");
            if (type == assetTypeToString(AssetType::Model)) return AssetType::Model;
            if (type == assetTypeToString(AssetType::Texture2D)) return AssetType::Texture2D;
            if (type == assetTypeToString(AssetType::TextureCube)) return AssetType::TextureCube;
            if (type == assetTypeToString(AssetType::Material)) return AssetType::Material;
            if (type == assetTypeToString(AssetType::Shader)) return AssetType::Shader;
            if (type == assetTypeToString(AssetType::EnvironmentMap)) return AssetType::EnvironmentMap;
            if (type == assetTypeToString(AssetType::ProceduralModel)) return AssetType::ProceduralModel;
            return AssetType::Unknown;
        }

        AssetHandle importAssetReference(
            const json& asset,
            AssetType expected_type,
            AssetManager& asset_manager) {
            if (!asset.is_object()) {
                return AssetHandle();
            }

            const AssetType serialized_type = readAssetType(asset);
            if (serialized_type != AssetType::Unknown && serialized_type != expected_type) {
                NX_CORE_WARN(
                    "Scene asset reference type mismatch. Expected {}, got {}.",
                    assetTypeToString(expected_type),
                    assetTypeToString(serialized_type));
            }

            const std::string path = asset.value("path", "");
            if (path.empty()) {
                const bool runtime_generated = asset.value("runtime_generated", false);
                if (runtime_generated) {
                    NX_CORE_WARN(
                        "Scene asset reference is runtime generated and has no path: {}.",
                        asset.value("debug_name", ""));
                }
                return AssetHandle();
            }

            switch (expected_type) {
            case AssetType::Model:
                return asset_manager.importModelAsset(path);
            case AssetType::EnvironmentMap:
                return asset_manager.importEnvironmentMapAsset(path);
            default:
                NX_CORE_WARN(
                    "Scene serializer cannot import asset type {} from path {}.",
                    assetTypeToString(expected_type),
                    path);
                return AssetHandle();
            }
        }

        json writeTagComponent(const TagComponent& tag) {
            return json{ { "name", tag.name } };
        }

        json writeTransformComponent(const TransformComponent& transform) {
            return json{
                { "translation", writeVec3(transform.translation) },
                { "rotation", writeVec3(transform.rotation) },
                { "scale", writeVec3(transform.scale) },
            };
        }

        json writeCameraComponent(const CameraComponent& camera) {
            return json{
                { "position", writeVec3(camera.position) },
                { "near_clip", camera.nearClip },
                { "far_clip", camera.farClip },
                { "view_matrix", writeMat4(camera.viewMatrix) },
                { "projection_matrix", writeMat4(camera.projectionMatrix) },
                { "view_projection_matrix", writeMat4(camera.viewProjectionMatrix) },
            };
        }

        json writeDirectionalLightComponent(const DirectionalLightComponent& light) {
            return json{
                { "direction", writeVec3(light.direction) },
                { "color", writeVec3(light.color) },
                { "intensity", light.intensity },
                { "cast_shadow", light.cast_shadow },
                { "shadow_strength", light.shadow_strength },
                { "shadow_bias", light.shadow_bias },
                { "shadow_distance", light.shadow_distance },
            };
        }

        json writePointLightComponent(const PointLightComponent& light) {
            return json{
                { "color", writeVec3(light.color) },
                { "intensity", light.intensity },
                { "constant", light.constant },
                { "linear", light.linear },
                { "quadratic", light.quadratic },
            };
        }

        json writePlayerComponent(const PlayerComponent& player) {
            return json{ { "move_speed", player.move_speed } };
        }

        json writeEnemyComponent(const EnemyComponent& enemy) {
            return json{ { "move_speed", enemy.move_speed } };
        }

        json writeVelocityComponent(const VelocityComponent& velocity) {
            return json{ { "velocity", writeVec3(velocity.velocity) } };
        }

        json writeHealthComponent(const HealthComponent& health) {
            return json{
                { "current", health.current },
                { "max", health.max },
            };
        }

        json writeCollectibleComponent(const CollectibleComponent& collectible) {
            return json{ { "score", collectible.score } };
        }

        json writeLifetimeComponent(const LifetimeComponent& lifetime) {
            return json{ { "seconds", lifetime.seconds } };
        }

        json writeRuntimeCameraControllerComponent(const RuntimeCameraControllerComponent& controller) {
            return json{
                { "mode", writeRuntimeCameraMode(controller.mode) },
                { "follow_offset", writeVec3(controller.follow_offset) },
                { "target_offset", writeVec3(controller.target_offset) },
                { "follow_speed", controller.follow_speed },
                { "fov_degrees", controller.fov_degrees },
                { "aspect_ratio", controller.aspect_ratio },
                { "enabled", controller.enabled },
            };
        }

        json writeSphereColliderComponent(const SphereColliderComponent& collider) {
            return json{
                { "offset", writeVec3(collider.offset) },
                { "radius", collider.radius },
                { "enabled", collider.enabled },
            };
        }

        json writeAABBColliderComponent(const AABBColliderComponent& collider) {
            return json{
                { "offset", writeVec3(collider.offset) },
                { "half_extents", writeVec3(collider.half_extents) },
                { "enabled", collider.enabled },
            };
        }

        json writeTriggerComponent(const TriggerComponent& trigger) {
            return json{ { "enabled", trigger.enabled } };
        }

        json writeCollisionFilterComponent(const CollisionFilterComponent& filter) {
            return json{
                { "layer", filter.layer },
                { "mask", filter.mask },
            };
        }

        void readTransformComponent(const json& component, TransformComponent& transform) {
            transform.translation = readVec3(component.value("translation", json::array()), transform.translation);
            transform.rotation = readVec3(component.value("rotation", json::array()), transform.rotation);
            transform.scale = readVec3(component.value("scale", json::array()), transform.scale);
        }

        void readCameraComponent(const json& component, CameraComponent& camera) {
            camera.position = readVec3(component.value("position", json::array()), camera.position);
            camera.nearClip = component.value("near_clip", camera.nearClip);
            camera.farClip = component.value("far_clip", camera.farClip);
            camera.viewMatrix = readMat4(component.value("view_matrix", json::array()), camera.viewMatrix);
            camera.projectionMatrix = readMat4(component.value("projection_matrix", json::array()), camera.projectionMatrix);
            camera.viewProjectionMatrix =
                readMat4(component.value("view_projection_matrix", json::array()), camera.viewProjectionMatrix);
        }

        void readRuntimeCameraControllerComponent(
            const json& component,
            RuntimeCameraControllerComponent& controller) {
            controller.mode =
                readRuntimeCameraMode(
                    component.value("mode", std::string(writeRuntimeCameraMode(controller.mode))),
                    controller.mode);
            controller.follow_offset =
                readVec3(component.value("follow_offset", json::array()), controller.follow_offset);
            controller.target_offset =
                readVec3(component.value("target_offset", json::array()), controller.target_offset);
            controller.follow_speed = component.value("follow_speed", controller.follow_speed);
            controller.fov_degrees = component.value("fov_degrees", controller.fov_degrees);
            controller.aspect_ratio = component.value("aspect_ratio", controller.aspect_ratio);
            controller.enabled = component.value("enabled", controller.enabled);
        }

        void readDirectionalLightComponent(const json& component, DirectionalLightComponent& light) {
            light.direction = readVec3(component.value("direction", json::array()), light.direction);
            light.color = readVec3(component.value("color", json::array()), light.color);
            light.intensity = component.value("intensity", light.intensity);
            light.cast_shadow = component.value("cast_shadow", light.cast_shadow);
            light.shadow_strength = component.value("shadow_strength", light.shadow_strength);
            light.shadow_bias = component.value("shadow_bias", light.shadow_bias);
            light.shadow_distance = component.value("shadow_distance", light.shadow_distance);
        }

        void readPointLightComponent(const json& component, PointLightComponent& light) {
            light.color = readVec3(component.value("color", json::array()), light.color);
            light.intensity = component.value("intensity", light.intensity);
            light.constant = component.value("constant", light.constant);
            light.linear = component.value("linear", light.linear);
            light.quadratic = component.value("quadratic", light.quadratic);
        }

        void readPlayerComponent(const json& component, PlayerComponent& player) {
            player.move_speed = component.value("move_speed", player.move_speed);
        }

        void readEnemyComponent(const json& component, EnemyComponent& enemy) {
            enemy.move_speed = component.value("move_speed", enemy.move_speed);
        }

        void readVelocityComponent(const json& component, VelocityComponent& velocity) {
            velocity.velocity = readVec3(component.value("velocity", json::array()), velocity.velocity);
        }

        void readHealthComponent(const json& component, HealthComponent& health) {
            health.current = component.value("current", health.current);
            health.max = component.value("max", health.max);
        }

        void readCollectibleComponent(const json& component, CollectibleComponent& collectible) {
            collectible.score = component.value("score", collectible.score);
        }

        void readLifetimeComponent(const json& component, LifetimeComponent& lifetime) {
            lifetime.seconds = component.value("seconds", lifetime.seconds);
        }

        void readSphereColliderComponent(const json& component, SphereColliderComponent& collider) {
            collider.offset = readVec3(component.value("offset", json::array()), collider.offset);
            collider.radius = component.value("radius", collider.radius);
            collider.enabled = component.value("enabled", collider.enabled);
        }

        void readAABBColliderComponent(const json& component, AABBColliderComponent& collider) {
            collider.offset = readVec3(component.value("offset", json::array()), collider.offset);
            collider.half_extents = readVec3(component.value("half_extents", json::array()), collider.half_extents);
            collider.enabled = component.value("enabled", collider.enabled);
        }

        void readTriggerComponent(const json& component, TriggerComponent& trigger) {
            trigger.enabled = component.value("enabled", trigger.enabled);
        }

        void readCollisionFilterComponent(const json& component, CollisionFilterComponent& filter) {
            filter.layer = component.value("layer", filter.layer);
            filter.mask = component.value("mask", filter.mask);
        }

        json writeEntity(
            entt::entity entity,
            const entt::registry& registry,
            const AssetManager& asset_manager) {
            const TagComponent& tag = registry.get<TagComponent>(entity);

            json components = json::object();
            components["Tag"] = writeTagComponent(tag);

            if (const auto* transform = registry.try_get<TransformComponent>(entity)) {
                components["Transform"] = writeTransformComponent(*transform);
            }

            if (const auto* camera = registry.try_get<CameraComponent>(entity)) {
                components["Camera"] = writeCameraComponent(*camera);
            }

            if (const auto* active_camera = registry.try_get<ActiveCameraComponent>(entity)) {
                components["ActiveCamera"] = json{ { "enabled", active_camera->enabled } };
            }

            if (const auto* mesh_renderer = registry.try_get<MeshRendererComponent>(entity)) {
                components["MeshRenderer"] = json{
                    { "model", writeAssetReference(mesh_renderer->getModelHandle(), asset_manager) },
                    { "is_transparent", mesh_renderer->is_transparent },
                };
            }

            if (const auto* directional_light = registry.try_get<DirectionalLightComponent>(entity)) {
                components["DirectionalLight"] = writeDirectionalLightComponent(*directional_light);
            }

            if (const auto* point_light = registry.try_get<PointLightComponent>(entity)) {
                components["PointLight"] = writePointLightComponent(*point_light);
            }

            if (const auto* environment = registry.try_get<EnvironmentComponent>(entity)) {
                components["Environment"] = json{
                    { "environment", writeAssetReference(environment->getEnvironmentHandle(), asset_manager) },
                    { "background_color", writeVec3(environment->background_color) },
                    { "intensity", environment->intensity },
                };
            }

            if (const auto* player = registry.try_get<PlayerComponent>(entity)) {
                components["Player"] = writePlayerComponent(*player);
            }

            if (const auto* enemy = registry.try_get<EnemyComponent>(entity)) {
                components["Enemy"] = writeEnemyComponent(*enemy);
            }

            if (const auto* velocity = registry.try_get<VelocityComponent>(entity)) {
                components["Velocity"] = writeVelocityComponent(*velocity);
            }

            if (const auto* health = registry.try_get<HealthComponent>(entity)) {
                components["Health"] = writeHealthComponent(*health);
            }

            if (const auto* collectible = registry.try_get<CollectibleComponent>(entity)) {
                components["Collectible"] = writeCollectibleComponent(*collectible);
            }

            if (const auto* lifetime = registry.try_get<LifetimeComponent>(entity)) {
                components["Lifetime"] = writeLifetimeComponent(*lifetime);
            }

            if (const auto* runtime_camera = registry.try_get<RuntimeCameraControllerComponent>(entity)) {
                components["RuntimeCameraController"] = writeRuntimeCameraControllerComponent(*runtime_camera);
            }

            if (const auto* sphere_collider = registry.try_get<SphereColliderComponent>(entity)) {
                components["SphereCollider"] = writeSphereColliderComponent(*sphere_collider);
            }

            if (const auto* aabb_collider = registry.try_get<AABBColliderComponent>(entity)) {
                components["AABBCollider"] = writeAABBColliderComponent(*aabb_collider);
            }

            if (const auto* trigger = registry.try_get<TriggerComponent>(entity)) {
                components["Trigger"] = writeTriggerComponent(*trigger);
            }

            if (const auto* collision_filter = registry.try_get<CollisionFilterComponent>(entity)) {
                components["CollisionFilter"] = writeCollisionFilterComponent(*collision_filter);
            }

            return json{
                { "name", tag.name },
                { "components", std::move(components) },
            };
        }

        std::vector<entt::entity> collectSerializableEntities(const entt::registry& registry) {
            std::vector<entt::entity> entities;
            auto view = registry.view<TagComponent>();
            for (entt::entity entity : view) {
                entities.push_back(entity);
            }

            std::sort(
                entities.begin(),
                entities.end(),
                [&](entt::entity lhs, entt::entity rhs) {
                    const std::string& lhs_name = registry.get<TagComponent>(lhs).name;
                    const std::string& rhs_name = registry.get<TagComponent>(rhs).name;
                    if (lhs_name != rhs_name) {
                        return lhs_name < rhs_name;
                    }
                    return static_cast<uint32_t>(lhs) < static_cast<uint32_t>(rhs);
                });

            return entities;
        }

        std::string readEntityName(const json& entity, const json& components) {
            if (components.contains("Tag") && components["Tag"].is_object()) {
                const std::string tag_name = components["Tag"].value("name", "");
                if (!tag_name.empty()) {
                    return tag_name;
                }
            }

            return entity.value("name", "NexAur_Entity");
        }
    } // namespace

    SceneSerializer::SceneSerializer(AssetManager& asset_manager)
        : m_asset_manager(asset_manager) {
    }

    SceneSerializationResult SceneSerializer::save(
        const SceneV2& scene,
        const std::filesystem::path& path) const {
        try {
            const entt::registry& registry = scene.getRegistry();
            std::vector<entt::entity> entities = collectSerializableEntities(registry);

            json document;
            document["format"] = kSceneFormat;
            document["version"] = kSceneVersion;
            document["entities"] = json::array();

            for (entt::entity entity : entities) {
                document["entities"].push_back(writeEntity(entity, registry, m_asset_manager));
            }

            if (path.has_parent_path()) {
                std::filesystem::create_directories(path.parent_path());
            }

            std::ofstream output(path);
            if (!output.is_open()) {
                return makeSaveResult(false, "Failed to open scene file for writing: " + path.string());
            }

            output << document.dump(4) << '\n';
            if (!output.good()) {
                return makeSaveResult(false, "Failed to write scene file: " + path.string());
            }

            return makeSaveResult(true, "Scene saved: " + path.string(), entities.size());
        } catch (const std::exception& error) {
            return makeSaveResult(false, std::string("Failed to save scene: ") + error.what());
        }
    }

    SceneLoadResult SceneSerializer::load(const std::filesystem::path& path) const {
        try {
            std::ifstream input(path);
            if (!input.is_open()) {
                return makeLoadResult(false, "Failed to open scene file for reading: " + path.string());
            }

            json document;
            input >> document;

            if (!document.is_object() || document.value("format", "") != kSceneFormat) {
                return makeLoadResult(false, "Invalid NexAur scene file: " + path.string());
            }

            const int version = document.value("version", 0);
            if (version != kSceneVersion) {
                return makeLoadResult(false, "Unsupported NexAur scene version: " + std::to_string(version));
            }

            const json& entities = document["entities"];
            if (!entities.is_array()) {
                return makeLoadResult(false, "Scene file has no entity array: " + path.string());
            }

            std::shared_ptr<SceneV2> scene = std::make_shared<SceneV2>();
            size_t entity_count = 0;

            for (const json& serialized_entity : entities) {
                if (!serialized_entity.is_object()) {
                    continue;
                }

                const json components =
                    serialized_entity.value("components", json::object());
                if (!components.is_object()) {
                    continue;
                }

                Entity entity = scene->createEntity(readEntityName(serialized_entity, components));

                if (components.contains("Tag") && components["Tag"].is_object()) {
                    entity.getComponent<TagComponent>().name =
                        components["Tag"].value("name", entity.getComponent<TagComponent>().name);
                }

                if (components.contains("Transform") && components["Transform"].is_object()) {
                    readTransformComponent(
                        components["Transform"],
                        entity.getComponent<TransformComponent>());
                }

                if (components.contains("Camera") && components["Camera"].is_object()) {
                    CameraComponent& camera = entity.addOrReplaceComponent<CameraComponent>();
                    readCameraComponent(components["Camera"], camera);
                }

                if (components.contains("ActiveCamera") && components["ActiveCamera"].is_object()) {
                    ActiveCameraComponent& active_camera =
                        entity.addOrReplaceComponent<ActiveCameraComponent>();
                    active_camera.enabled = components["ActiveCamera"].value("enabled", active_camera.enabled);
                }

                if (components.contains("MeshRenderer") && components["MeshRenderer"].is_object()) {
                    const json& mesh_json = components["MeshRenderer"];
                    MeshRendererComponent& mesh_renderer =
                        entity.addOrReplaceComponent<MeshRendererComponent>();
                    mesh_renderer.model_asset =
                        importAssetReference(mesh_json.value("model", json::object()), AssetType::Model, m_asset_manager);
                    mesh_renderer.is_transparent =
                        mesh_json.value("is_transparent", mesh_renderer.is_transparent);
                }

                if (components.contains("DirectionalLight") && components["DirectionalLight"].is_object()) {
                    DirectionalLightComponent& light =
                        entity.addOrReplaceComponent<DirectionalLightComponent>();
                    readDirectionalLightComponent(components["DirectionalLight"], light);
                }

                if (components.contains("PointLight") && components["PointLight"].is_object()) {
                    PointLightComponent& light = entity.addOrReplaceComponent<PointLightComponent>();
                    readPointLightComponent(components["PointLight"], light);
                }

                if (components.contains("Environment") && components["Environment"].is_object()) {
                    const json& environment_json = components["Environment"];
                    EnvironmentComponent& environment =
                        entity.addOrReplaceComponent<EnvironmentComponent>();
                    environment.environment_asset =
                        importAssetReference(
                            environment_json.value("environment", json::object()),
                            AssetType::EnvironmentMap,
                            m_asset_manager);
                    environment.background_color =
                        readVec3(environment_json.value("background_color", json::array()), environment.background_color);
                    environment.intensity = environment_json.value("intensity", environment.intensity);
                }

                if (components.contains("Player") && components["Player"].is_object()) {
                    PlayerComponent& player = entity.addOrReplaceComponent<PlayerComponent>();
                    readPlayerComponent(components["Player"], player);
                }

                if (components.contains("Enemy") && components["Enemy"].is_object()) {
                    EnemyComponent& enemy = entity.addOrReplaceComponent<EnemyComponent>();
                    readEnemyComponent(components["Enemy"], enemy);
                }

                if (components.contains("Velocity") && components["Velocity"].is_object()) {
                    VelocityComponent& velocity = entity.addOrReplaceComponent<VelocityComponent>();
                    readVelocityComponent(components["Velocity"], velocity);
                }

                if (components.contains("Health") && components["Health"].is_object()) {
                    HealthComponent& health = entity.addOrReplaceComponent<HealthComponent>();
                    readHealthComponent(components["Health"], health);
                }

                if (components.contains("Collectible") && components["Collectible"].is_object()) {
                    CollectibleComponent& collectible = entity.addOrReplaceComponent<CollectibleComponent>();
                    readCollectibleComponent(components["Collectible"], collectible);
                }

                if (components.contains("Lifetime") && components["Lifetime"].is_object()) {
                    LifetimeComponent& lifetime = entity.addOrReplaceComponent<LifetimeComponent>();
                    readLifetimeComponent(components["Lifetime"], lifetime);
                }

                if (components.contains("RuntimeCameraController") && components["RuntimeCameraController"].is_object()) {
                    RuntimeCameraControllerComponent& runtime_camera =
                        entity.addOrReplaceComponent<RuntimeCameraControllerComponent>();
                    readRuntimeCameraControllerComponent(components["RuntimeCameraController"], runtime_camera);
                }

                if (components.contains("SphereCollider") && components["SphereCollider"].is_object()) {
                    SphereColliderComponent& sphere_collider =
                        entity.addOrReplaceComponent<SphereColliderComponent>();
                    readSphereColliderComponent(components["SphereCollider"], sphere_collider);
                }

                if (components.contains("AABBCollider") && components["AABBCollider"].is_object()) {
                    AABBColliderComponent& aabb_collider =
                        entity.addOrReplaceComponent<AABBColliderComponent>();
                    readAABBColliderComponent(components["AABBCollider"], aabb_collider);
                }

                if (components.contains("Trigger") && components["Trigger"].is_object()) {
                    TriggerComponent& trigger = entity.addOrReplaceComponent<TriggerComponent>();
                    readTriggerComponent(components["Trigger"], trigger);
                }

                if (components.contains("CollisionFilter") && components["CollisionFilter"].is_object()) {
                    CollisionFilterComponent& collision_filter =
                        entity.addOrReplaceComponent<CollisionFilterComponent>();
                    readCollisionFilterComponent(components["CollisionFilter"], collision_filter);
                }

                entity_count++;
            }

            return makeLoadResult(true, "Scene loaded: " + path.string(), scene, entity_count);
        } catch (const std::exception& error) {
            return makeLoadResult(false, std::string("Failed to load scene: ") + error.what());
        }
    }
} // namespace NexAur
