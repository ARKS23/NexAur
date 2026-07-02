#include "scene_test.h"

#include "Core/Module/engine_module.h"
#include "Function/File/file_system.h"
#include "Function/Global/global_context.h"
#include "Function/Resource/model.h"
#include "Function/Resource/procedural_model_builder.h"
#include "Function/Scene/scene_service.h"

#include <algorithm>

namespace NexAur {
    namespace {
        MaterialImportData makeSolidMaterial(
            const std::string& name,
            const glm::vec4& base_color,
            float roughness = 0.75f,
            const glm::vec3& emissive = glm::vec3{ 0.0f }) {
            MaterialImportData material;
            material.name = name;
            material.base_color_factor = base_color;
            material.roughness_factor = roughness;
            material.emissive_factor = emissive;
            return material;
        }
    } // namespace

    SceneTestClass::SceneTestClass()
        : m_asset_manager(AssetManager::getInstance()) {
        ModuleRegistry* registry = g_runtime_global_context.getModuleRegistry();
        std::shared_ptr<SceneService> scene_service = registry ? registry->getService<SceneService>() : nullptr;
        m_scene = scene_service ? scene_service->getActiveScene() : nullptr;
    }

    Entity SceneTestClass::addSphereEntity(
        std::string name,
        std::string material_type,
        glm::vec3 position,
        glm::vec3 rotation,
        glm::vec3 scale) {
        MaterialImportData material;
        setMaterial(material, material_type);
        return addRuntimeModelEntity(
            name,
            ProceduralModelBuilder::createSphereModel(64, 64, material),
            position,
            rotation,
            scale);
    }

    Entity SceneTestClass::addCubeEntity(
        std::string name,
        std::string material_type,
        glm::vec3 position,
        glm::vec3 rotation,
        glm::vec3 scale) {
        MaterialImportData material;
        setMaterial(material, material_type);
        return addRuntimeModelEntity(
            name,
            ProceduralModelBuilder::createCubeModel(material),
            position,
            rotation,
            scale);
    }

    void SceneTestClass::addCornellBox(glm::vec3 center, glm::vec3 size) {
        if (!m_scene) {
            NX_CORE_ERROR("SceneTestClass has no active scene.");
            return;
        }

        constexpr float kWallThickness = 0.08f;
        const MaterialImportData white_wall = makeSolidMaterial(
            "Cornell.White",
            glm::vec4{ 0.78f, 0.74f, 0.68f, 1.0f },
            0.82f);
        const MaterialImportData red_wall = makeSolidMaterial(
            "Cornell.Red",
            glm::vec4{ 0.72f, 0.12f, 0.08f, 1.0f },
            0.84f);
        const MaterialImportData green_wall = makeSolidMaterial(
            "Cornell.Green",
            glm::vec4{ 0.12f, 0.48f, 0.18f, 1.0f },
            0.84f);
        const MaterialImportData light_panel = makeSolidMaterial(
            "Cornell.Light",
            glm::vec4{ 1.0f, 0.92f, 0.78f, 1.0f },
            0.45f,
            glm::vec3{ 7.0f, 6.2f, 4.6f });

        const float half_width = size.x * 0.5f;
        const float half_height = size.y * 0.5f;
        const float half_depth = size.z * 0.5f;
        const float floor_y = center.y - half_height;
        const float ceiling_y = center.y + half_height;
        const glm::vec3 zero_rotation{ 0.0f };

        addSolidCubeEntity(
            "CornellBox Floor",
            white_wall,
            { center.x, floor_y, center.z },
            zero_rotation,
            { size.x, kWallThickness, size.z });
        addSolidCubeEntity(
            "CornellBox Ceiling",
            white_wall,
            { center.x, ceiling_y, center.z },
            zero_rotation,
            { size.x, kWallThickness, size.z });
        addSolidCubeEntity(
            "CornellBox BackWall",
            white_wall,
            { center.x, center.y, center.z - half_depth },
            zero_rotation,
            { size.x, size.y, kWallThickness });
        addSolidCubeEntity(
            "CornellBox LeftWall",
            red_wall,
            { center.x - half_width, center.y, center.z },
            zero_rotation,
            { kWallThickness, size.y, size.z });
        addSolidCubeEntity(
            "CornellBox RightWall",
            green_wall,
            { center.x + half_width, center.y, center.z },
            zero_rotation,
            { kWallThickness, size.y, size.z });
        addSolidCubeEntity(
            "CornellBox LightPanel",
            light_panel,
            { center.x, ceiling_y - kWallThickness, center.z - half_depth * 0.15f },
            zero_rotation,
            { size.x * 0.35f, kWallThickness, size.z * 0.26f });

        const glm::vec3 light_position{
            center.x,
            ceiling_y - kWallThickness - 0.02f,
            center.z - half_depth * 0.15f
        };
        const glm::vec2 light_size{ size.x * 0.35f, size.z * 0.26f };

        Entity rect_light_entity = m_scene->createEntity("CornellBox RectLight");
        auto& rect_light = rect_light_entity.addComponent<RectLightComponent>();
        rect_light.color = glm::vec3{ 1.0f, 0.92f, 0.78f };
        rect_light.intensity = 9.0f;
        rect_light.size = light_size;
        rect_light.range = std::max(size.x, std::max(size.y, size.z)) * 1.25f;
        rect_light.two_sided = false;

        TransformComponent& rect_light_transform = rect_light_entity.getComponent<TransformComponent>();
        rect_light_transform.translation = light_position;

        addSolidCubeEntity(
            "CornellBox ShortBlock",
            white_wall,
            { center.x - size.x * 0.24f, floor_y + size.y * 0.14f, center.z + size.z * 0.18f },
            { 0.0f, glm::radians(16.0f), 0.0f },
            { size.x * 0.20f, size.y * 0.28f, size.z * 0.20f });
        addSolidCubeEntity(
            "CornellBox TallBlock",
            white_wall,
            { center.x + size.x * 0.23f, floor_y + size.y * 0.22f, center.z - size.z * 0.08f },
            { 0.0f, glm::radians(-12.0f), 0.0f },
            { size.x * 0.20f, size.y * 0.44f, size.z * 0.20f });

        Entity light_entity = m_scene->createEntity("CornellBox PointLight");
        auto& point_light = light_entity.addComponent<PointLightComponent>();
        point_light.color = glm::vec3{ 1.0f, 0.92f, 0.78f };
        point_light.intensity = 2.0f;
        point_light.constant = 1.0f;
        point_light.linear = 0.18f;
        point_light.quadratic = 0.04f;
        point_light.cast_shadow = true;
        point_light.shadow_range = std::max(size.x, std::max(size.y, size.z)) * 1.2f;
        point_light.shadow_strength = 0.9f;

        TransformComponent& light_transform = light_entity.getComponent<TransformComponent>();
        light_transform.translation = light_position;
    }

    Entity SceneTestClass::addModelEntity(std::string name, const std::string& model_path, glm::vec3 position) {
        AssetHandle model_asset = m_asset_manager.importModelAsset(model_path);
        return addModelAssetEntity(std::move(name), model_asset, model_path, position);
    }

    Entity SceneTestClass::addImportedModelEntity(std::string name, const std::string& model_path, glm::vec3 position) {
        AssetHandle model_asset = m_asset_manager.importModelAssetFromRegistry(model_path);
        return addModelAssetEntity(std::move(name), model_asset, model_path, position);
    }

    Entity SceneTestClass::addModelAssetEntity(
        std::string name,
        AssetHandle model_asset,
        const std::string& model_path,
        const glm::vec3& position) {
        if (!m_scene) {
            NX_CORE_ERROR("SceneTestClass has no active scene.");
            return Entity();
        }

        if (!model_asset) {
            NX_CORE_ERROR("Failed to load model: {0}", model_path);
            return Entity();
        }

        Entity model_entity = m_scene->createEntity(name);
        model_entity.addComponent<MeshRendererComponent>(model_asset);

        TransformComponent& transform = model_entity.getComponent<TransformComponent>();
        transform.translation = position;

        return model_entity;
    }

    Entity SceneTestClass::addSolidCubeEntity(
        const std::string& name,
        const MaterialImportData& material,
        const glm::vec3& position,
        const glm::vec3& rotation,
        const glm::vec3& scale) {
        return addRuntimeModelEntity(
            name,
            ProceduralModelBuilder::createCubeModel(material),
            position,
            rotation,
            scale);
    }

    Entity SceneTestClass::addRuntimeModelEntity(
        const std::string& name,
        const std::shared_ptr<Model>& model,
        const glm::vec3& position,
        const glm::vec3& rotation,
        const glm::vec3& scale) {
        if (!m_scene || !model || !model->isLoaded()) {
            NX_CORE_ERROR("Failed to create runtime model entity: {}", name);
            return Entity();
        }

        AssetHandle model_asset = m_asset_manager.registerRuntimeModel(model, name);
        if (!model_asset) {
            return Entity();
        }

        Entity entity = m_scene->createEntity(name);
        entity.addComponent<MeshRendererComponent>(model_asset);

        TransformComponent& transform = entity.getComponent<TransformComponent>();
        transform.translation = position;
        transform.rotation = rotation;
        transform.scale = scale;

        return entity;
    }

    void SceneTestClass::setMaterial(MaterialImportData& material_data, const std::string& material_type) {
        material_data.name = material_type;
        material_data.base_color_texture_path = NX_ASSET("assets/textures/PBR/" + material_type + "/albedo.png");
        material_data.metallic_texture_path = NX_ASSET("assets/textures/PBR/" + material_type + "/metallic.png");
        material_data.roughness_texture_path = NX_ASSET("assets/textures/PBR/" + material_type + "/roughness.png");
        material_data.ao_texture_path = NX_ASSET("assets/textures/PBR/" + material_type + "/ao.png");
        material_data.normal_texture_path = NX_ASSET("assets/textures/PBR/" + material_type + "/normal.png");
    }
} // namespace NexAur
