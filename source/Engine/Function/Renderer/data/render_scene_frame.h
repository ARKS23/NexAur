#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Renderer/data/render_debug_draw.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/data/render_view.h"

namespace NexAur {
    struct RenderFrameDirectionalLight {
        glm::vec3 direction{ -0.2f, -1.0f, -0.3f };
        glm::vec3 color{ 1.0f };
        float intensity = 1.0f;
        bool cast_shadow = true;
        float shadow_strength = 0.65f;
        float shadow_bias = 0.002f;
        float shadow_distance = 30.0f;
    };

    struct RenderFramePointLight {
        glm::vec3 position{ 0.0f };
        glm::vec3 color{ 1.0f };
        float intensity = 1.0f;
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
        bool shadow_requested = false;
        bool cast_shadow = false;
        int32_t shadow_slot = -1;
        float shadow_range = 8.0f;
        float shadow_strength = 0.85f;
    };

    struct RenderFrameRectLight {
        glm::vec3 position{ 0.0f };
        glm::vec3 right{ 1.0f, 0.0f, 0.0f };
        glm::vec3 up{ 0.0f, 0.0f, 1.0f };
        glm::vec3 normal{ 0.0f, -1.0f, 0.0f };
        glm::vec2 size{ 1.0f, 1.0f };
        glm::vec3 color{ 1.0f };
        float intensity = 8.0f;
        float range = 8.0f;
        bool two_sided = false;
        bool shadow_requested = false;
        bool cast_shadow = false;
        int32_t shadow_slot = -1;
        float shadow_strength = 0.85f;
    };

    struct RenderFrameReflectionProbe {
        AssetHandle environment_asset;
        AssetHandle baked_environment_asset;
        glm::vec3 position{ 0.0f };
        glm::vec3 box_extents{ 4.0f, 3.0f, 4.0f };
        float intensity = 1.0f;
        float diffuse_intensity = 0.75f;
        float blend_distance = 0.75f;
        uint32_t capture_resolution = 128;
        uint32_t capture_priority = 0;
        float capture_near_clip = 0.1f;
        float capture_far_clip = 40.0f;
        int entity_id = -1;
        bool diffuse_enabled = true;
        bool box_projection = true;
        bool capture_include_skybox = true;
        bool capture_dirty = true;
    };

    struct RenderFrameReflectionProbeReference {
        AssetHandle baked_environment_asset;
        int entity_id = -1;
    };

    struct RenderSceneFrameSourceCounts {
        size_t opaque_object_count = 0;
        size_t transparent_object_count = 0;
        size_t point_light_count = 0;
        size_t point_shadow_request_count = 0;
        size_t rect_light_count = 0;
        size_t rect_shadow_request_count = 0;
        size_t reflection_probe_count = 0;
        size_t debug_line_count = 0;
    };

    struct RenderSceneFrameObject {
        AssetHandle model_asset;
        std::vector<AssetHandle> material_overrides;
        glm::mat4 transform{ 1.0f };
        int entity_id = -1;
    };

    struct RenderSceneFrame {
        uint64_t frame_serial = 0;
        uint64_t scene_id = 0;
        RenderView view;
        RenderSettings render_settings;
        RenderDebugVisualizationOptions debug_visualization_options;
        RenderSceneFrameSourceCounts source_counts;

        std::vector<RenderSceneFrameObject> opaque_objects;
        std::vector<RenderSceneFrameObject> transparent_objects;

        RenderFrameDirectionalLight directional_light;
        std::vector<RenderFramePointLight> point_lights;
        std::vector<RenderFrameRectLight> rect_lights;
        std::vector<RenderFrameReflectionProbe> reflection_probes;
        std::vector<RenderFrameReflectionProbeReference> reflection_probe_references;

        AssetHandle environment_asset;
        glm::vec3 environment_color{ 0.08f, 0.10f, 0.14f };
        float environment_intensity = 0.65f;
        float skybox_intensity = 0.75f;
        float ibl_intensity = 0.65f;
        RenderDebugDrawData debug_draw;

        bool hasRenderableObjects() const {
            return !opaque_objects.empty() || !transparent_objects.empty();
        }
    };
} // namespace NexAur
