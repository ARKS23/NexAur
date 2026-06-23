#include "scene_test.h"

#include "Core/Module/engine_module.h"
#include "Function/Global/global_context.h"
#include "Function/Scene/scene_service.h"
#include "Function/Renderer/Resources/procedural_model_factory.h"
#include "Function/Renderer/Resources/render_resource_cache.h"
#include "Function/File/file_system.h"

namespace NexAur {
    SceneTestClass::SceneTestClass() : m_asset_manager(AssetManager::getInstance()) {
        // Sandbox 是应用层示例，仍可从组合根拿服务，但不直接访问 legacy public pointer。
        ModuleRegistry* registry = g_runtime_global_context.getModuleRegistry();
        std::shared_ptr<SceneService> scene_service = registry ? registry->getService<SceneService>() : nullptr;
        m_scene = scene_service ? scene_service->getActiveScene() : nullptr;
    }

    Entity SceneTestClass::addSphereEntity(std::string name, std::string material_type, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) {
        if (!m_scene) {
            NX_CORE_ERROR("SceneTestClass has no active scene.");
            return Entity();
        }

        std::shared_ptr<RenderModelData> sphere_model = ProceduralModelFactory::createSphereModel(64, 64);

        setMaterial(sphere_model->meshes[0].material, material_type);

        AssetHandle procedural_model = RenderResourceCache::getInstance().registerProceduralModel(sphere_model, m_asset_manager, name);
        if (!procedural_model) {
            return Entity();
        }

        Entity sphere_entity = m_scene->createEntity(name);
        sphere_entity.addComponent<MeshRendererComponent>(procedural_model);

        TransformComponent& transform = sphere_entity.getComponent<TransformComponent>();
        transform.translation = position;
        transform.rotation = rotation;
        transform.scale = scale;

        return sphere_entity;
    }

    Entity SceneTestClass::addCubeEntity(std::string name, std::string material_type, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) {
        if (!m_scene) {
            NX_CORE_ERROR("SceneTestClass has no active scene.");
            return Entity();
        }

        std::shared_ptr<RenderModelData> cube_model = ProceduralModelFactory::createCubeModel();

        setMaterial(cube_model->meshes[0].material, material_type);

        AssetHandle procedural_model = RenderResourceCache::getInstance().registerProceduralModel(cube_model, m_asset_manager, name);
        if (!procedural_model) {
            return Entity();
        }

        Entity cube_entity = m_scene->createEntity(name);
        cube_entity.addComponent<MeshRendererComponent>(procedural_model);

        TransformComponent& transform = cube_entity.getComponent<TransformComponent>();
        transform.translation = position;
        transform.rotation = rotation;
        transform.scale = scale;

        return cube_entity;
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

    void SceneTestClass::setMaterial(RendererMaterialData& material_data, const std::string& material_type) {
        RenderResourceCache& resource_cache = RenderResourceCache::getInstance();
        auto resolve_texture = [&](const std::string& texture_path) {
            AssetHandle texture_asset = m_asset_manager.importTextureAsset(texture_path);
            if (!texture_asset) {
                return std::shared_ptr<Texture2D>();
            }

            return resource_cache.getOrCreateTexture2D(texture_asset, m_asset_manager);
        };

        // Sandbox 的过程模型也走 Renderer 资源缓存，避免示例代码绕回 AssetManager 创建 GPU 贴图。
        material_data.albedo_map = resolve_texture(NX_ASSET("assets/textures/PBR/" + material_type + "/albedo.png"));
        material_data.metallic_map = resolve_texture(NX_ASSET("assets/textures/PBR/" + material_type + "/metallic.png"));
        material_data.roughness_map = resolve_texture(NX_ASSET("assets/textures/PBR/" + material_type + "/roughness.png"));
        material_data.ao_map = resolve_texture(NX_ASSET("assets/textures/PBR/" + material_type + "/ao.png"));
        material_data.normal_map = resolve_texture(NX_ASSET("assets/textures/PBR/" + material_type + "/normal.png"));
    }

} // namespace NexAur
