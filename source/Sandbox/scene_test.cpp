#include "scene_test.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Resource/procedural_model_factory.h"
#include "Function/File/file_system.h"

namespace NexAur {
    SceneTestClass::SceneTestClass() : m_asset_manager(AssetManager::getInstance()) {
        // 需要等引擎初始化完再进行构造
        m_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
    }

    Entity SceneTestClass::addSphereEntity(std::string name, std::string material_type, glm::vec3 position) {
        std::shared_ptr<RenderModelData> sphere_model = ProceduralModelFactory::createSphereModel(64, 64);

        setMaterial(sphere_model->meshes[0].material, material_type);

        UUID procedual_id = m_asset_manager.registerRenderModel(sphere_model);

        Entity sphere_entity = m_scene->createEntity(name);
        sphere_entity.addComponent<MeshRendererComponent>(procedual_id);

        TransformComponent& transform = sphere_entity.getComponent<TransformComponent>();
        transform.translation = position;
        transform.scale = glm::vec3(0.5f);

        return sphere_entity;
    }

    Entity SceneTestClass::addCubeEntity(std::string name, std::string material_type, glm::vec3 position) {
        std::shared_ptr<RenderModelData> cube_model = ProceduralModelFactory::createCubeModel();

        setMaterial(cube_model->meshes[0].material, material_type);

        UUID procedual_id = m_asset_manager.registerRenderModel(cube_model);

        Entity cube_entity = m_scene->createEntity(name);
        cube_entity.addComponent<MeshRendererComponent>(procedual_id);

        TransformComponent& transform = cube_entity.getComponent<TransformComponent>();
        transform.translation = position;
        transform.scale = glm::vec3(0.5f);

        return cube_entity;
    }

    void SceneTestClass::setMaterial(RendererMaterialData& material_data, const std::string& material_type) {
        material_data.albedo_map = m_asset_manager.getTexture(m_asset_manager.loadTexture(NX_ASSET("assets/textures/PBR/" + material_type + "/albedo.png")));
        material_data.metallic_map = m_asset_manager.getTexture(m_asset_manager.loadTexture(NX_ASSET("assets/textures/PBR/" + material_type + "/metallic.png")));
        material_data.roughness_map = m_asset_manager.getTexture(m_asset_manager.loadTexture(NX_ASSET("assets/textures/PBR/" + material_type + "/roughness.png")));
        material_data.ao_map = m_asset_manager.getTexture(m_asset_manager.loadTexture(NX_ASSET("assets/textures/PBR/" + material_type + "/ao.png")));
        material_data.normal_map = m_asset_manager.getTexture(m_asset_manager.loadTexture(NX_ASSET("assets/textures/PBR/" + material_type + "/normal.png")));
    }

} // namespace NexAur
