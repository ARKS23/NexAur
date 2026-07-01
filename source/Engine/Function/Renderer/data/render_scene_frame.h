#pragma once

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

    struct RenderSceneFrameObject {
        AssetHandle model_asset;
        glm::mat4 transform{ 1.0f };
        int entity_id = -1;
    };

    struct RenderSceneFrame {
        RenderView view;

        std::vector<RenderSceneFrameObject> opaque_objects;
        std::vector<RenderSceneFrameObject> transparent_objects;

        RenderFrameDirectionalLight directional_light;
        std::vector<RenderFramePointLight> point_lights;

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
