#pragma once
#include "Function/Scene/scene_v2.h"
#include "Function/Scene/component.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Resource/procedural_model_factory.h"
#include "Function/Global/global_context.h"
#include "Function/Scene/entity.h"
#include "Core/UUID.h"

namespace NexAur {
    class SceneTestClass {
    public:
        SceneTestClass();
        ~SceneTestClass() = default;

        Entity addSphereEntity();
        Entity addCubeEntity();

    private:
        void setMaterial(RendererMaterialData& materail_data, const std::string& material_type);

    private:
        std::shared_ptr<SceneV2> m_scene;
        AssetManager& m_asset_manager;
    };
} // namespace NexAur
