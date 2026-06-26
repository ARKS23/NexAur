#pragma once

#include <memory>
#include <string>

#include "Function/Resource/asset_manager.h"
#include "Function/Resource/mesh.h"
#include "Function/Scene/component.h"
#include "Function/Scene/entity.h"
#include "Function/Scene/scene_v2.h"

namespace NexAur {
    class Model;

    class SceneTestClass {
    public:
        SceneTestClass();
        ~SceneTestClass() = default;

        Entity addSphereEntity(std::string name = "Sphere", std::string material_type = "gold", glm::vec3 position = glm::vec3(0.0f), glm::vec3 rotation = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f));
        Entity addCubeEntity(std::string name = "Cube", std::string material_type = "gold", glm::vec3 position = glm::vec3(0.0f), glm::vec3 rotation = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f));
        Entity addModelEntity(std::string name, const std::string& model_path, glm::vec3 position = glm::vec3(0.0f));

    private:
        Entity addRuntimeModelEntity(
            const std::string& name,
            const std::shared_ptr<Model>& model,
            const glm::vec3& position,
            const glm::vec3& rotation,
            const glm::vec3& scale);
        void setMaterial(MaterialImportData& material_data, const std::string& material_type);

    private:
        std::shared_ptr<SceneV2> m_scene;
        AssetManager& m_asset_manager;
    };
} // namespace NexAur
