#include "pch.h"
#include "vulkan_draw_list_builder.h"

#include "Function/Resource/asset_manager.h"
#include "Function/Renderer/Vulkan/resources/vulkan_model_resource.h"
#include "Function/Renderer/Vulkan/vulkan_render_resource_cache.h"

#include <algorithm>

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
    } // namespace

    VulkanDrawList VulkanDrawListBuilder::buildDrawList(
        const RenderSceneFrame& scene_frame,
        VulkanRenderResourceCache& resource_cache,
        AssetManager& asset_manager) const {
        VulkanDrawList draw_list;
        draw_list.view = scene_frame.view;
        draw_list.directional_light = scene_frame.directional_light;
        draw_list.point_lights = scene_frame.point_lights;
        draw_list.environment_asset = scene_frame.environment_asset;
        draw_list.environment = resource_cache.getOrCreateEnvironment(scene_frame.environment_asset, asset_manager);
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
