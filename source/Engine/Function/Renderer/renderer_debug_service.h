#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Renderer/renderer_service_types.h"

namespace NexAur {
    struct RendererDebugBackendStats {
        RendererBackendType backend = RendererBackendType::Unknown;
        bool initialized = false;
        std::string device_api_version = "Unknown";
        bool swapchain_ready = false;
        uint32_t swapchain_width = 0;
        uint32_t swapchain_height = 0;
        uint32_t swapchain_image_count = 0;
        std::string swapchain_format = "Unknown";
        ViewportOutputKind viewport_output_kind = ViewportOutputKind::None;
    };

    struct RendererDebugFrameStats {
        double engine_delta_ms = 0.0;
        double renderer_cpu_ms = 0.0;
        size_t opaque_object_count = 0;
        size_t transparent_object_count = 0;
        size_t opaque_draw_item_count = 0;
        size_t transparent_draw_item_count = 0;
        size_t point_light_count = 0;
        size_t point_shadow_request_count = 0;
        size_t shadowed_point_light_count = 0;
        size_t rect_light_count = 0;
        size_t rect_light_clipped_count = 0;
        size_t rect_shadow_request_count = 0;
        size_t shadowed_rect_light_count = 0;
        size_t debug_line_count = 0;
    };

    struct RendererDebugViewStats {
        uint32_t viewport_width = 0;
        uint32_t viewport_height = 0;
        glm::vec3 camera_position{ 0.0f };
        float near_clip = 0.0f;
        float far_clip = 0.0f;
    };

    struct RendererDebugRenderTargetStats {
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string color_format = "None";
        std::string depth_format = "None";
    };

    struct RendererDebugPickingTargetStats {
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string object_id_format = "None";
        std::string depth_format = "None";
        bool frame_ready = false;
    };

    struct RendererDebugShadowTargetStats {
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t layer_count = 0;
        std::string depth_format = "None";
    };

    struct RendererDebugPostProcessStats {
        bool enabled = false;
        bool ready = false;
        std::string output_format = "None";
    };

    struct RendererDebugBloomStats {
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mip_count = 0;
        std::string color_format = "None";
    };

    struct RendererDebugAoStats {
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string color_format = "None";
        bool half_resolution = false;
    };

    struct RendererDebugEffectsStats {
        std::string lighting_preset = "Outdoor";
        std::string debug_view = "Final Lit";
        uint32_t bloom_mip = 0;
        uint32_t shadow_cascade = 0;
        uint32_t point_shadow_layer = 0;
        uint32_t rect_shadow_layer = 0;
        bool bloom_debug_available = false;
        bool ao_debug_available = false;
        bool shadow_debug_available = false;
        bool point_shadow_debug_available = false;
        bool rect_shadow_debug_available = false;
        bool rect_ltc_specular_enabled = false;
        bool rect_ltc_debug_only = false;
        float rect_ltc_specular_scale = 1.0f;
    };

    struct RendererDebugResourceStats {
        size_t model_count = 0;
        size_t texture_count = 0;
        size_t environment_count = 0;
        size_t mesh_count = 0;
        size_t material_count = 0;
        bool fallback_white_texture_ready = false;
        bool fallback_material_ready = false;
        bool fallback_environment_ready = false;
        bool active_environment_ready = false;
        std::string active_environment_name = "None";
        uint32_t environment_source_width = 0;
        uint32_t environment_source_height = 0;
        uint32_t environment_size = 0;
        uint32_t irradiance_size = 0;
        uint32_t prefilter_size = 0;
        uint32_t prefilter_mip_count = 0;
        bool brdf_lut_ready = false;
    };

    struct RendererDebugSnapshot {
        RendererDebugBackendStats backend;
        RendererDebugFrameStats frame;
        RendererDebugViewStats view;
        RendererDebugRenderTargetStats viewport_target;
        RendererDebugRenderTargetStats hdr_scene_target;
        RendererDebugPickingTargetStats picking_target;
        RendererDebugShadowTargetStats shadow_target;
        RendererDebugShadowTargetStats point_shadow_target;
        RendererDebugShadowTargetStats rect_shadow_target;
        RendererDebugPostProcessStats post_process;
        RendererDebugBloomStats bloom;
        RendererDebugAoStats ao;
        RendererDebugEffectsStats effects;
        RendererDebugResourceStats resources;
    };

    class NEXAUR_API RendererDebugService {
    public:
        virtual ~RendererDebugService() = default;

        virtual RendererDebugSnapshot getDebugSnapshot() const = 0;
    };
} // namespace NexAur
