#pragma once
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Renderer/data/render_debug_draw.h"
#include "Function/Renderer/data/render_settings.h"

namespace NexAur {
    struct RendererCameraData {
        glm::mat4 view_matrix{ 1.0f };
        glm::mat4 projection_matrix{ 1.0f };
        glm::mat4 view_projection_matrix{ 1.0f }; // 缓存VP, shader中无需再次计算
        glm::vec3 position{ 0.0f };             // 摄像机世界坐标，用于高光/PBR计算
        float near_clip = 0.1f;
        float far_clip = 1000.0f;
    };

    struct RendererDirectionalLightData {
        glm::vec3 direction{ -0.2f, -1.0f, -0.3f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        bool cast_shadow = true;
        float shadow_strength = 0.65f;
        float shadow_bias = 0.002f;
        float shadow_distance = 30.0f;
    };

    struct RendererPointLightData{
        glm::vec3 position{ 0.0f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
        bool cast_shadow = false;
        float shadow_range = 8.0f;
        float shadow_strength = 0.85f;
    };

    struct RendererRectLightData {
        glm::vec3 position{ 0.0f };
        glm::vec3 right{ 1.0f, 0.0f, 0.0f };
        glm::vec3 up{ 0.0f, 0.0f, 1.0f };
        glm::vec3 normal{ 0.0f, -1.0f, 0.0f };
        glm::vec2 size{ 1.0f, 1.0f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 8.0f;
        float range = 8.0f;
        bool two_sided = false;
        bool cast_shadow = false;
        float shadow_strength = 0.85f;
    };

    struct RendererEnvironmentData {
        AssetHandle environment_asset;
        glm::vec3 background_color{ 0.08f, 0.10f, 0.14f };
        float intensity = 0.65f;
        float skybox_intensity = 0.75f;
        float ibl_intensity = 0.65f;
    };

    struct RendererReflectionProbeData {
        AssetHandle environment_asset;
        glm::vec3 position{ 0.0f };
        glm::vec3 box_extents{ 4.0f, 3.0f, 4.0f };
        float intensity = 1.0f;
        float blend_distance = 0.75f;
        bool enabled = true;
        bool box_projection = true;
    };

    // 场景提取阶段的物体引用：只保存可序列化资产身份，不直接保存 GPU 指针。
    struct RenderObjectData {
        AssetHandle model_asset;
        glm::mat4 transform{ 1.0f };
        int entity_id = -1; // 用于标记实体ID，编辑器选中时需要
    };
    
    // 渲染数据包: 每帧从场景收集的数据,供渲染器使用
    struct RenderDataPacket {
        RendererCameraData camera_data;

        RendererDirectionalLightData directional_light_data;
        std::vector<RendererPointLightData> point_lights_data;
        std::vector<RendererRectLightData> rect_lights_data;

        RendererEnvironmentData environment_data;
        std::vector<RendererReflectionProbeData> reflection_probes_data;

        std::vector<RenderObjectData> opaque_objects;   // 不透明物体
        std::vector<RenderObjectData> transparent_objects; // 透明物体
        RenderSettings render_settings;
        RenderDebugVisualizationOptions debug_visualization_options;
        RenderDebugDrawData debug_draw;

        void clear() {
            const RenderSettings current_render_settings = render_settings;
            const RenderDebugVisualizationOptions current_debug_options = debug_visualization_options;
            // 清空完整帧状态，避免缺少相机或灯光组件时沿用上一帧数据。
            camera_data = RendererCameraData();
            directional_light_data = RendererDirectionalLightData();
            environment_data = RendererEnvironmentData();
            point_lights_data.clear();
            rect_lights_data.clear();
            reflection_probes_data.clear();
            opaque_objects.clear();
            transparent_objects.clear();
            render_settings = current_render_settings;
            debug_visualization_options = current_debug_options;
            debug_draw.clear();
        }
    };

} // namespace NexAur
