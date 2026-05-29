#pragma once
#include "Function/Scene/scene_v2.h"
#include "Function/Scene/component.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Resource/procedural_model_factory.h"
#include "Function/Global/global_context.h"
#include "Function/Scene/entity.h"
#include "Core/UUID.h"

namespace NexAur {
    struct RendererMaterialData;

    class SceneTestClass {
    public:
        SceneTestClass();
        ~SceneTestClass() = default;

        Entity addSphereEntity(std::string name = "Sphere", std::string material_type = "gold", glm::vec3 position = glm::vec3(0.0f), glm::vec3 rotation = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f));
        Entity addCubeEntity(std::string name = "Cube", std::string material_type = "gold", glm::vec3 position = glm::vec3(0.0f), glm::vec3 rotation = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f));
        Entity addModelEntity(std::string name, const std::string& model_path, glm::vec3 position = glm::vec3(0.0f));

    private:
        void setMaterial(RendererMaterialData& materail_data, const std::string& material_type);

    private:
        std::shared_ptr<SceneV2> m_scene;
        AssetManager& m_asset_manager;
    };
} // namespace NexAur
