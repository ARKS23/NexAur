#include "pch.h"
#include "vulkan_draw_list_builder.h"

#include "Function/Resource/asset_manager.h"
#include "Function/Renderer/Vulkan/resources/vulkan_model_resource.h"
#include "Function/Renderer/Vulkan/resources/vulkan_environment_resource.h"
#include "Function/Renderer/Vulkan/vulkan_render_resource_cache.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/geometric.hpp>

namespace NexAur {
    namespace {
        uint64_t buildSortKey(AssetHandle model_asset, size_t mesh_index) {
            const uint64_t asset_key = static_cast<uint64_t>(model_asset.id);
            return asset_key ^ (static_cast<uint64_t>(mesh_index) << 32u);
        }

        void appendModelObjects(
            const std::vector<RenderSceneFrameObject>& scene_objects,
            std::vector<VulkanMeshDrawItem>& draw_items,
            VulkanRenderResourceCache& resource_cache,
            AssetManager& asset_manager,
            const char* list_name) {
            size_t skipped_count = 0;

            for (const RenderSceneFrameObject& object : scene_objects) {
                VulkanModelResource* model = resource_cache.getOrCreateModel(object.model_asset, asset_manager);
                if (!model || !model->isReady()) {
                    ++skipped_count;
                    continue;
                }

                const std::vector<VulkanMeshResource>& meshes = model->getMeshes();
                const std::vector<VulkanMaterialResource>& materials = model->getMaterials();
                for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
                    const VulkanMeshResource& mesh = meshes[mesh_index];
                    if (!mesh.isReady()) {
                        ++skipped_count;
                        continue;
                    }

                    VulkanMeshDrawItem draw_item;
                    draw_item.mesh = &mesh;
                    draw_item.material = mesh_index < materials.size() ? &materials[mesh_index] : resource_cache.getFallbackMaterial();
                    draw_item.transform = object.transform;
                    draw_item.entity_id = object.entity_id;
                    draw_item.sort_key = buildSortKey(object.model_asset, mesh_index);
                    draw_items.push_back(draw_item);
                }
            }

            if (skipped_count > 0) {
                NX_CORE_WARN(
                    "VulkanDrawListBuilder skipped {} {} mesh/object item(s) because resources were not ready.",
                    skipped_count,
                    list_name);
            }
        }

        void sortOpaqueItems(std::vector<VulkanMeshDrawItem>& draw_items) {
            std::sort(draw_items.begin(), draw_items.end(), [](const VulkanMeshDrawItem& lhs, const VulkanMeshDrawItem& rhs) {
                return lhs.sort_key < rhs.sort_key;
            });
        }

        float distanceToProbeBox(const RenderFrameReflectionProbe& probe, const glm::vec3& position) {
            const glm::vec3 local_delta = glm::abs(position - probe.position);
            const glm::vec3 outside_delta = glm::max(local_delta - probe.box_extents, glm::vec3{ 0.0f });
            return glm::length(outside_delta);
        }

        const RenderFrameReflectionProbe* selectActiveReflectionProbe(
            const std::vector<RenderFrameReflectionProbe>& probes,
            const glm::vec3& camera_position) {
            const RenderFrameReflectionProbe* best_probe = nullptr;
            float best_distance = std::numeric_limits<float>::max();
            float best_intensity = -1.0f;

            for (const RenderFrameReflectionProbe& probe : probes) {
                if (probe.intensity <= 0.0001f) {
                    continue;
                }

                const float distance = distanceToProbeBox(probe, camera_position);
                const bool better_distance = distance + 0.0001f < best_distance;
                const bool same_distance = std::abs(distance - best_distance) <= 0.0001f;
                if (better_distance || (same_distance && probe.intensity > best_intensity)) {
                    best_probe = &probe;
                    best_distance = distance;
                    best_intensity = probe.intensity;
                }
            }

            return best_probe;
        }

        AssetHandle selectReflectionProbeEnvironmentAsset(
            const RenderFrameReflectionProbe& probe,
            const RenderSceneFrame& scene_frame,
            const AssetManager& asset_manager) {
            if (probe.baked_environment_asset) {
                const AssetMetadata* metadata = asset_manager.getMetadata(probe.baked_environment_asset);
                if (metadata &&
                    metadata->type == AssetType::EnvironmentMap &&
                    !metadata->runtime_generated &&
                    !metadata->path.empty()) {
                    return probe.baked_environment_asset;
                }
            }

            return probe.environment_asset ? probe.environment_asset : scene_frame.environment_asset;
        }

        VulkanActiveReflectionProbe buildActiveReflectionProbe(
            const RenderSceneFrame& scene_frame,
            VulkanRenderResourceCache& resource_cache,
            AssetManager& asset_manager) {
            VulkanActiveReflectionProbe active_probe;

            const RenderFrameReflectionProbe* selected_probe =
                selectActiveReflectionProbe(scene_frame.reflection_probes, scene_frame.view.camera_position);
            if (!selected_probe) {
                return active_probe;
            }

            active_probe.enabled = true;
            active_probe.environment_asset =
                selectReflectionProbeEnvironmentAsset(*selected_probe, scene_frame, asset_manager);
            active_probe.environment =
                resource_cache.getOrCreateEnvironment(active_probe.environment_asset, asset_manager);
            active_probe.position = selected_probe->position;
            active_probe.box_extents = glm::max(selected_probe->box_extents, glm::vec3{ 0.05f });
            active_probe.intensity = std::max(0.0f, selected_probe->intensity);
            active_probe.blend_distance = std::max(0.0f, selected_probe->blend_distance);
            active_probe.entity_id = selected_probe->entity_id;
            active_probe.box_projection = selected_probe->box_projection;
            if (active_probe.environment && active_probe.environment->isReady()) {
                active_probe.prefilter_mip_count = active_probe.environment->getPrefilterMipCount();
            }
            return active_probe;
        }
    } // namespace

    VulkanDrawList VulkanDrawListBuilder::buildDrawList(
        const RenderSceneFrame& scene_frame,
        VulkanRenderResourceCache& resource_cache,
        AssetManager& asset_manager) const {
        VulkanDrawList draw_list;
        draw_list.view = scene_frame.view;
        draw_list.directional_light = scene_frame.directional_light;
        draw_list.point_lights = scene_frame.point_lights;
        draw_list.rect_lights = scene_frame.rect_lights;
        draw_list.reflection_probes = scene_frame.reflection_probes;
        draw_list.environment_asset = scene_frame.environment_asset;
        draw_list.environment = resource_cache.getOrCreateEnvironment(scene_frame.environment_asset, asset_manager);
        draw_list.active_reflection_probe =
            buildActiveReflectionProbe(scene_frame, resource_cache, asset_manager);
        draw_list.environment_color = scene_frame.environment_color;
        draw_list.skybox_intensity = scene_frame.skybox_intensity;
        draw_list.ibl_intensity = scene_frame.ibl_intensity;
        draw_list.debug_draw = scene_frame.debug_draw;

        draw_list.opaque_items.reserve(scene_frame.opaque_objects.size());
        appendModelObjects(
            scene_frame.opaque_objects,
            draw_list.opaque_items,
            resource_cache,
            asset_manager,
            "opaque");
        sortOpaqueItems(draw_list.opaque_items);

        draw_list.transparent_items.reserve(scene_frame.transparent_objects.size());
        appendModelObjects(
            scene_frame.transparent_objects,
            draw_list.transparent_items,
            resource_cache,
            asset_manager,
            "transparent");

        return draw_list;
    }
} // namespace NexAur
