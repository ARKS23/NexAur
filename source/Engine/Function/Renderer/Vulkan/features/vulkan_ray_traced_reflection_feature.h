#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/features/vulkan_reflection_surface_feature.h"
#include "Function/Renderer/Vulkan/features/vulkan_render_feature_context.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_render_view.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_types.h"

namespace NexAur {
    class VulkanPassGraph;

    enum class VulkanRayTracedReflectionDebugMode : uint32_t {
        None = 0,
        RawRadiance,
        HitConfidence,
        HitDistance,
        InstanceId,
        PrimitiveId
    };

    VkExtent2D getVulkanRayTracedReflectionExtent(
        uint32_t width,
        uint32_t height,
        bool half_resolution);
    glm::vec2 encodeVulkanReflectionSurfaceNormal(const glm::vec3& normal);
    glm::vec3 decodeVulkanReflectionSurfaceNormal(const glm::vec2& encoded);
    glm::uvec2 mapVulkanRayTracedReflectionSourcePixel(
        glm::uvec2 output_pixel,
        VkExtent2D source_extent,
        VkExtent2D output_extent);
    glm::vec3 interpolateVulkanRayTracingTriangleAttribute(
        const glm::vec3& vertex0,
        const glm::vec3& vertex1,
        const glm::vec3& vertex2,
        const glm::vec2& barycentrics);
    glm::vec3 transformVulkanRayTracingHitNormal(
        const glm::mat4& object_to_world,
        const glm::vec3& object_normal);
    bool isVulkanRayTracedReflectionTraceEligible(
        float depth,
        const glm::vec4& reflection_surface,
        float ssr_confidence,
        const RenderRayTracedReflectionSettings& settings,
        bool ssr_enabled);

    struct VulkanRayTracedReflectionInput {
        VkImageView reflection_surface_view = VK_NULL_HANDLE;
        VkImageView scene_depth_view = VK_NULL_HANDLE;
        VkImageView ssr_hit_mask_view = VK_NULL_HANDLE;
        VkImageView raw_reflection_view = VK_NULL_HANDLE;
        VkImageView hit_distance_view = VK_NULL_HANDLE;
        VkDescriptorSet scene_table_descriptor_set = VK_NULL_HANDLE;
        VkDescriptorSet environment_descriptor_set = VK_NULL_HANDLE;
        VkExtent2D source_extent{};
        VkExtent2D output_extent{};
        uint32_t instance_count = 0;
        uint32_t geometry_count = 0;
        uint32_t material_count = 0;
        float environment_intensity = 1.0f;

        bool valid() const {
            return reflection_surface_view != VK_NULL_HANDLE &&
                   scene_depth_view != VK_NULL_HANDLE &&
                   ssr_hit_mask_view != VK_NULL_HANDLE &&
                   raw_reflection_view != VK_NULL_HANDLE &&
                   hit_distance_view != VK_NULL_HANDLE &&
                   scene_table_descriptor_set != VK_NULL_HANDLE &&
                   environment_descriptor_set != VK_NULL_HANDLE &&
                   source_extent.width > 0 &&
                   source_extent.height > 0 &&
                   output_extent.width > 0 &&
                   output_extent.height > 0 &&
                   instance_count > 0 &&
                   geometry_count > 0 &&
                   material_count > 0;
        }
    };

    struct VulkanRayTracedReflectionTimingCallbacks {
        std::function<bool(VkCommandBuffer)> begin;
        std::function<bool(VkCommandBuffer)> end;
        std::function<void()> discard;
    };

    class VulkanRayTracedReflectionFeature final {
    public:
        VulkanRayTracedReflectionFeature() = default;
        ~VulkanRayTracedReflectionFeature();

        VulkanRayTracedReflectionFeature(const VulkanRayTracedReflectionFeature&) = delete;
        VulkanRayTracedReflectionFeature& operator=(const VulkanRayTracedReflectionFeature&) = delete;

        bool init(
            const VulkanRenderFeatureContext& context,
            uint32_t texture_capacity,
            uint32_t geometry_descriptor_capacity);
        void shutdown();

        bool isReady() const;
        bool addPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle reflection_surface,
            VulkanGraphImageHandle scene_depth,
            VulkanGraphImageHandle ssr_hit_mask,
            VulkanGraphImageHandle raw_reflection,
            VulkanGraphImageHandle hit_distance,
            VulkanGraphAccelerationStructureHandle ray_query_scene,
            const VulkanRayTracedReflectionInput& input,
            const VulkanRenderView& view,
            const RenderRayTracedReflectionSettings& settings,
            bool ssr_enabled,
            VulkanRayTracedReflectionDebugMode debug_mode,
            uint32_t frame_index,
            VulkanRayTracedReflectionTimingCallbacks timing_callbacks = {});

        uint64_t getDispatchCount() const { return m_dispatch_count; }
        VkExtent2D getLastDispatchExtent() const { return m_last_dispatch_extent; }

    private:
        struct TracePushConstants {
            glm::mat4 inverse_view_projection{ 1.0f };
            glm::vec4 camera_position_max_distance{ 0.0f, 0.0f, 0.0f, 30.0f };
            glm::uvec4 extents{ 1u };
            glm::vec4 trace_params{ 0.85f, 0.02f, 0.001f, 1.0f };
            glm::uvec4 table_counts_flags{ 0u };
        };
        static_assert(sizeof(TracePushConstants) == 128);

        bool createPipeline(
            uint32_t texture_capacity,
            uint32_t geometry_descriptor_capacity);
        bool allocateDescriptors();
        void freeDescriptors();
        bool updateDescriptor(
            uint32_t frame_index,
            const VulkanRayTracedReflectionInput& input) const;
        bool recordTrace(
            VkCommandBuffer command_buffer,
            const VulkanRayTracedReflectionInput& input,
            const VulkanRenderView& view,
            const RenderRayTracedReflectionSettings& settings,
            bool ssr_enabled,
            VulkanRayTracedReflectionDebugMode debug_mode,
            uint32_t frame_index);

    private:
        VulkanRenderFeatureContext m_context;
        VkDescriptorSetLayout m_trace_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_scene_table_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_environment_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanComputePipelineState m_pipeline;
        std::array<VulkanDescriptorSetAllocation, kVulkanFramesInFlight>
            m_trace_descriptor_allocations;
        uint64_t m_dispatch_count = 0;
        VkExtent2D m_last_dispatch_extent{};
    };
} // namespace NexAur
