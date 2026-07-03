#pragma once

#include <string>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Resource/procedural_primitive.h"
#include "Function/Scene/entity.h"

namespace NexAur {
    class AssetManager;
    class SceneV2;
    struct ProceduralPrimitiveComponent;

    struct ProceduralPrimitiveCreateInfo {
        ProceduralPrimitiveType type = ProceduralPrimitiveType::Cube;
        uint32_t segments = 32;
        uint32_t rings = 16;
        std::string name;
        glm::vec3 translation{ 0.0f };
        glm::vec3 rotation{ 0.0f };
        glm::vec3 scale{ 1.0f };
    };

    NEXAUR_API AssetHandle registerProceduralPrimitiveModel(
        AssetManager& asset_manager,
        const ProceduralPrimitiveComponent& primitive);

    NEXAUR_API bool refreshProceduralPrimitiveMesh(
        Entity entity,
        AssetManager& asset_manager);

    NEXAUR_API Entity createProceduralPrimitiveEntity(
        SceneV2& scene,
        AssetManager& asset_manager,
        const ProceduralPrimitiveCreateInfo& create_info);
} // namespace NexAur
