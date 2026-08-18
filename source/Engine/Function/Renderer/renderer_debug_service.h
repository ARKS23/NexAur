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
        std::string requested_mode = "Screen Space";
        std::string active_technique = "Disabled";
        bool ray_query_available = false;
        bool ray_query_active = false;
        std::string fallback_reason = "None";
        uint32_t ray_count = 0;
        float filter_depth_threshold = 0.0f;
        float filter_normal_threshold = 0.0f;
        bool spatial_filter_enabled = false;
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

    struct RendererDebugReflectionStats {
        bool enabled = false;
        bool ready = false;
        bool valid = false;
        bool pending_reset = true;
        uint32_t read_index = 0;
        uint32_t write_index = 1;
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t surface_generation = 0;
        std::string surface_format = "None";
        std::string motion_vector_format = "None";
        std::string reset_reason = "First frame";
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
        bool ray_query_debug_available = false;
        bool ray_query_shadow_available = false;
        bool ray_query_shadow_enabled = false;
        bool ray_query_shadow_active = false;
        std::string ray_query_shadow_mode = "Auto";
        std::string ray_query_shadow_fallback_reason = "None";
        float ray_query_shadow_max_distance = 35.0f;
        float ray_query_shadow_normal_bias = 0.02f;
        float ray_query_shadow_direction_bias = 0.01f;
        bool ray_query_gpu_timing_supported = false;
        uint64_t ray_query_gpu_sample_count = 0;
        double ray_query_forward_gpu_ms = 0.0;
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
        bool static_mesh_blas_cache_ready = false;
        size_t static_mesh_blas_entry_count = 0;
        size_t static_mesh_blas_ready_count = 0;
        size_t static_mesh_blas_failed_count = 0;
        uint64_t static_mesh_blas_build_count = 0;
        uint64_t static_mesh_blas_cache_hit_count = 0;
        uint64_t static_mesh_blas_failed_build_count = 0;
        uint64_t static_mesh_blas_bytes = 0;
        uint64_t static_mesh_blas_memory_budget_bytes = 0;
        uint64_t static_mesh_blas_compaction_count = 0;
        uint64_t static_mesh_blas_failed_compaction_count = 0;
        uint64_t static_mesh_blas_compaction_saved_bytes = 0;
        uint64_t static_mesh_blas_eviction_count = 0;
        uint64_t static_mesh_blas_retired_entry_count = 0;
        uint64_t static_mesh_blas_retired_bytes = 0;
        uint64_t static_mesh_blas_scratch_capacity_bytes = 0;
        bool static_mesh_blas_compaction_enabled = false;
        bool static_mesh_blas_gpu_timing_supported = false;
        uint64_t static_mesh_blas_gpu_timing_sample_count = 0;
        double static_mesh_blas_build_gpu_ms = 0.0;
        double static_mesh_blas_compaction_gpu_ms = 0.0;
        std::string static_mesh_blas_last_failure = "None";
        bool tlas_manager_ready = false;
        bool tlas_ready = false;
        uint32_t tlas_source_instance_count = 0;
        uint32_t tlas_built_instance_count = 0;
        uint32_t tlas_skipped_blas_count = 0;
        uint32_t tlas_skipped_transform_count = 0;
        uint32_t tlas_skipped_material_count = 0;
        uint64_t tlas_build_count = 0;
        uint64_t tlas_rebuild_count = 0;
        uint64_t tlas_update_count = 0;
        uint64_t tlas_reuse_count = 0;
        uint64_t tlas_allocation_count = 0;
        uint64_t tlas_instance_buffer_bytes = 0;
        uint64_t tlas_instance_buffer_capacity_bytes = 0;
        uint64_t tlas_bytes = 0;
        uint64_t tlas_scratch_capacity_bytes = 0;
        uint32_t tlas_instance_capacity = 0;
        bool tlas_gpu_timing_supported = false;
        uint64_t tlas_gpu_timing_sample_count = 0;
        double tlas_build_gpu_ms = 0.0;
        std::string tlas_last_build_mode = "None";
        std::string tlas_last_failure = "None";
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

    struct RendererDebugRayTracingSceneTableStats {
        bool capability_supported = false;
        bool initialized = false;
        bool ready = false;
        bool descriptor_ready = false;
        bool bda_geometry_fetch_enabled = false;
        bool descriptor_indexed_geometry_fetch_enabled = false;
        uint32_t instance_count = 0;
        uint32_t geometry_count = 0;
        uint32_t material_count = 0;
        uint32_t texture_count = 0;
        uint32_t texture_capacity = 0;
        uint32_t texture_overflow_count = 0;
        uint32_t geometry_descriptor_capacity = 0;
        uint32_t geometry_overflow_count = 0;
        uint64_t instance_buffer_bytes = 0;
        uint64_t geometry_buffer_bytes = 0;
        uint64_t material_buffer_bytes = 0;
        std::string last_failure_reason = "None";
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
        RendererDebugReflectionStats reflection;
        RendererDebugSmaaStats smaa;
        RendererDebugEffectsStats effects;
        RendererDebugResourceStats resources;
        RendererDebugRayTracingSceneTableStats ray_tracing_scene_table;
    };

    class NEXAUR_API RendererDebugService {
    public:
        virtual ~RendererDebugService() = default;

        virtual RendererDebugSnapshot getDebugSnapshot() const = 0;
    };
} // namespace NexAur
