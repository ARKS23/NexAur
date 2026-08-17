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
        bool ray_query_supported = false;
        bool ray_query_enabled = false;
        bool acceleration_structure_functions_loaded = false;
        bool buffer_device_address_enabled = false;
        bool ray_tracing_pipeline_supported = false;
        std::string ray_query_fallback_reason = "Not queried";
        uint64_t acceleration_structure_min_scratch_alignment = 0;
        uint64_t acceleration_structure_max_geometry_count = 0;
        uint64_t acceleration_structure_max_instance_count = 0;
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
        double frame_wait_ms = 0.0;
        uint32_t frame_slot_count = 0;
        uint32_t current_frame_slot = 0;
        uint32_t frames_in_flight = 0;
        uint32_t swapchain_images_in_flight = 0;
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
        size_t reflection_probe_count = 0;
        bool active_reflection_probe = false;
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
        uint32_t pending_request_count = 0;
        uint32_t readback_in_flight_count = 0;
        std::string last_request_status = "None";
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
        std::string tone_mapping = "ACES";
        float exposure = 1.0f;
        bool bloom_enabled = false;
        float bloom_intensity = 0.0f;
        bool color_grading_enabled = false;
        float color_grading_exposure_offset = 0.0f;
        float color_grading_contrast = 1.0f;
        float color_grading_saturation = 1.0f;
        float color_grading_temperature = 0.0f;
        float color_grading_tint = 0.0f;
        float color_grading_black_point = 0.0f;
        float color_grading_white_point = 1.0f;
        float vignette_intensity = 0.0f;
        float sharpen_intensity = 0.0f;
    };

    struct RendererDebugBloomStats {
        bool enabled = false;
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mip_count = 0;
        std::string color_format = "None";
    };

    struct RendererDebugAoStats {
        bool enabled = false;
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string color_format = "None";
        bool half_resolution = false;
    };

    struct RendererDebugSsrStats {
        bool enabled = false;
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string reflection_format = "None";
        std::string hit_mask_format = "None";
        float max_distance = 18.0f;
        uint32_t max_steps = 32;
        float thickness = 0.18f;
        float stride = 1.0f;
        float roughness_fade = 0.65f;
        float edge_fade = 0.12f;
        float intensity = 1.0f;
    };

    struct RendererDebugSmaaStats {
        bool enabled = false;
        bool ready = false;
        uint32_t width = 0;
        uint32_t height = 0;
        std::string source_format = "None";
        std::string edge_format = "None";
        std::string blend_format = "None";
        std::string mode = "SMAA";
        float edge_threshold = 0.08f;
        float contrast_factor = 2.0f;
        uint32_t max_search_steps = 8;
        float blend_strength = 0.85f;
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
        bool ssr_debug_available = false;
        bool smaa_debug_available = false;
        bool shadow_debug_available = false;
        bool point_shadow_debug_available = false;
        bool rect_shadow_debug_available = false;
        bool point_shadow_enabled = false;
        bool rect_shadow_enabled = false;
        bool contact_shadow_enabled = false;
        uint32_t point_shadow_budget = 0;
        uint32_t rect_shadow_budget = 0;
        uint32_t point_shadow_map_resolution = 0;
        uint32_t rect_shadow_map_resolution = 0;
        float point_shadow_strength = 1.0f;
        float point_shadow_bias = 0.0f;
        float point_shadow_normal_bias = 0.0f;
        float point_shadow_filter_radius = 0.0f;
        float rect_shadow_strength = 1.0f;
        float rect_shadow_bias = 0.0f;
        float rect_shadow_normal_bias = 0.0f;
        float rect_shadow_filter_radius = 0.0f;
        float rect_shadow_projection_margin = 0.0f;
        bool rect_shadow_soft_enabled = false;
        float rect_shadow_pcss_light_radius = 0.0f;
        float rect_shadow_pcss_search_radius = 0.0f;
        float rect_shadow_pcss_min_filter_radius = 0.0f;
        float rect_shadow_pcss_max_filter_radius = 0.0f;
        uint32_t rect_shadow_pcss_blocker_taps = 0;
        uint32_t rect_shadow_pcss_filter_taps = 0;
        bool rect_ltc_specular_enabled = false;
        bool rect_ltc_debug_only = false;
        float rect_ltc_specular_scale = 1.0f;
    };

    struct RendererDebugResourceStats {
        size_t model_count = 0;
        size_t texture_count = 0;
        size_t environment_count = 0;
        size_t mesh_count = 0;
        size_t device_address_mesh_count = 0;
        size_t material_count = 0;
        uint64_t gpu_submitted_serial = 0;
        uint64_t gpu_completed_serial = 0;
        size_t gpu_retirement_pending_count = 0;
        uint64_t gpu_retired_count = 0;
        uint64_t gpu_collected_count = 0;
        size_t upload_pending_bytes = 0;
        size_t upload_submitted_bytes_this_frame = 0;
        size_t upload_byte_budget_per_frame = 0;
        uint32_t upload_pending_requests = 0;
        uint32_t upload_in_flight_requests = 0;
        uint32_t upload_submitted_requests_this_frame = 0;
        uint32_t upload_request_budget_per_frame = 0;
        uint64_t upload_ready_requests = 0;
        uint64_t upload_failed_requests = 0;
        uint64_t upload_cancelled_requests = 0;
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
        bool active_reflection_probe_ready = false;
        std::string active_reflection_probe_name = "None";
        float active_reflection_probe_intensity = 0.0f;
        bool active_reflection_probe_diffuse_enabled = false;
        float active_reflection_probe_diffuse_intensity = 0.0f;
        float active_reflection_probe_blend_distance = 0.0f;
        bool active_reflection_probe_runtime = false;
        uint32_t active_reflection_probe_capture_resolution = 0;
        std::string active_reflection_probe_capture_status = "Idle";
        uint32_t reflection_probe_capture_pending_count = 0;
        uint32_t reflection_probe_capture_budget_per_frame = 0;
        uint32_t reflection_probe_runtime_capture_count = 0;
        uint32_t reflection_probe_pinned_capture_count = 0;
        uint32_t reflection_probe_runtime_capture_limit = 0;
        int reflection_probe_last_captured_entity_id = -1;
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
        RendererDebugSsrStats ssr;
        RendererDebugSmaaStats smaa;
        RendererDebugEffectsStats effects;
        RendererDebugResourceStats resources;
    };

    class NEXAUR_API RendererDebugService {
    public:
        virtual ~RendererDebugService() = default;

        virtual RendererDebugSnapshot getDebugSnapshot() const = 0;
    };
} // namespace NexAur
