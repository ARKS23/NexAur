#include "pch.h"
#include "procedural_primitive_entity.h"

#include "Core/Log/log_system.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Resource/material_types.h"
#include "Function/Resource/model.h"
#include "Function/Resource/procedural_model_builder.h"
#include "Function/Scene/component.h"
#include "Function/Scene/scene_v2.h"

#include <algorithm>

namespace NexAur {
    namespace {
        uint32_t sanitizeSegments(uint32_t segments) {
            return std::clamp(segments, 3u, 128u);
        }

        uint32_t sanitizeRings(uint32_t rings) {
            return std::clamp(rings, 2u, 128u);
        }

        MaterialImportData makePrimitiveMaterial(ProceduralPrimitiveType type) {
            MaterialImportData material;
            material.name = std::string("Primitive.") + proceduralPrimitiveTypeToString(type);
            material.base_color_factor = glm::vec4{ 0.82f, 0.84f, 0.88f, 1.0f };
            material.metallic_factor = 0.0f;
            material.roughness_factor = 0.55f;
            material.occlusion_strength = 1.0f;
            return material;
        }
    } // namespace

    AssetHandle registerProceduralPrimitiveModel(
        AssetManager& asset_manager,
        const ProceduralPrimitiveComponent& primitive) {
        const uint32_t segments = sanitizeSegments(primitive.segments);
        const uint32_t rings = sanitizeRings(primitive.rings);
        const MaterialImportData material = makePrimitiveMaterial(primitive.type);

        std::shared_ptr<Model> model = ProceduralModelBuilder::createPrimitiveModel(
            primitive.type,
            segments,
            rings,
            material);
        if (!model || !model->isLoaded()) {
            NX_CORE_ERROR(
                "Failed to build procedural primitive model: {}.",
                proceduralPrimitiveTypeToString(primitive.type));
            return AssetHandle();
        }

        return asset_manager.registerRuntimeModel(
            model,
            std::string("Procedural ") + proceduralPrimitiveTypeToString(primitive.type));
    }

    bool refreshProceduralPrimitiveMesh(
        Entity entity,
        AssetManager& asset_manager) {
        if (!entity || !entity.hasComponent<ProceduralPrimitiveComponent>()) {
            return false;
        }

        ProceduralPrimitiveComponent& primitive =
            entity.getComponent<ProceduralPrimitiveComponent>();
        primitive.segments = sanitizeSegments(primitive.segments);
        primitive.rings = sanitizeRings(primitive.rings);

        const bool was_transparent =
            entity.hasComponent<MeshRendererComponent>() &&
            entity.getComponent<MeshRendererComponent>().is_transparent;
        const AssetHandle model_asset = registerProceduralPrimitiveModel(asset_manager, primitive);
        if (!model_asset) {
            return false;
        }

        MeshRendererComponent& mesh_renderer =
            entity.addOrReplaceComponent<MeshRendererComponent>(model_asset);
        mesh_renderer.is_transparent = was_transparent;
        return true;
    }

    Entity createProceduralPrimitiveEntity(
        SceneV2& scene,
        AssetManager& asset_manager,
        const ProceduralPrimitiveCreateInfo& create_info) {
        const std::string entity_name = create_info.name.empty()
            ? proceduralPrimitiveTypeToString(create_info.type)
            : create_info.name;
        Entity entity = scene.createEntity(entity_name);
        if (!entity) {
            return entity;
        }

        TransformComponent& transform = entity.getComponent<TransformComponent>();
        transform.translation = create_info.translation;
        transform.rotation = create_info.rotation;
        transform.scale = glm::max(create_info.scale, glm::vec3{ 0.001f });

        entity.addComponent<ProceduralPrimitiveComponent>(
            create_info.type,
            create_info.segments,
            create_info.rings);
        if (!refreshProceduralPrimitiveMesh(entity, asset_manager)) {
            NX_CORE_WARN("Created procedural primitive entity without a renderable mesh: {}.", entity_name);
        }
        return entity;
    }
} // namespace NexAur
