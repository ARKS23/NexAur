#include "scene_test.h"

#include "Core/Module/engine_module.h"
#include "Function/File/file_system.h"
#include "Function/Global/global_context.h"
#include "Function/Resource/model.h"
#include "Function/Resource/procedural_model_builder.h"
#include "Function/Scene/scene_service.h"

namespace NexAur {
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

    Entity SceneTestClass::addModelEntity(std::string name, const std::string& model_path, glm::vec3 position) {
        if (!m_scene) {
            NX_CORE_ERROR("SceneTestClass has no active scene.");
            return Entity();
        }

        AssetHandle model_asset = m_asset_manager.importModelAsset(model_path);
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
