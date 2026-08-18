#pragma once

#include <cstdint>
#include <span>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_gpu_timestamp_query.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_flight_tracker.h"
#include "Function/Renderer/Vulkan/resources/vulkan_debug_draw_buffer.h"
#include "Function/Renderer/Vulkan/resources/vulkan_frame_lighting_resource.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_scene_resource.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_scene_table.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanAccelerationStructure;
    class VulkanDescriptorAllocator;
    class VulkanDescriptorLayoutCache;
    class VulkanTextureResource;

    struct VulkanFrameGpuTimingStats {
        bool ray_query_supported = false;
        uint64_t ray_query_sample_count = 0;
        uint64_t ray_query_submission_serial = 0;
        double ray_query_forward_ms = 0.0;
    };

    class VulkanFrameContext final {
    public:
        VulkanFrameContext() = default;
        ~VulkanFrameContext();

        VulkanFrameContext(const VulkanFrameContext&) = delete;
        VulkanFrameContext& operator=(const VulkanFrameContext&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VulkanDescriptorLayoutCache& descriptor_layout_cache,
            VulkanDescriptorAllocator& descriptor_allocator,
            uint32_t frame_index,
            VkDescriptorSetLayout ray_tracing_scene_descriptor_set_layout = VK_NULL_HANDLE,
            VkDescriptorSetLayout ray_tracing_shading_scene_descriptor_set_layout = VK_NULL_HANDLE,
            uint32_t ray_tracing_shading_texture_capacity = 0,
            uint32_t ray_tracing_shading_geometry_descriptor_capacity = 0);
        void shutdown();

        void markSubmitted(uint64_t serial);
        void markCompleted();

        bool beginRayQueryGpuTiming(VkCommandBuffer command_buffer);
        bool endRayQueryGpuTiming(VkCommandBuffer command_buffer);
        void discardRayQueryGpuTiming();
        VulkanFrameGpuTimingStats getGpuTimingStats() const;

        bool isReady() const { return m_ready; }
        bool isInFlight() const { return m_flight_state.isInFlight(); }
        uint32_t getFrameIndex() const { return m_frame_index; }
        uint64_t getSubmissionSerial() const {
            return m_flight_state.getSubmissionSerial();
        }

        VkCommandPool getCommandPool() const { return m_command_pool; }
        VkCommandBuffer getCommandBuffer() const { return m_command_buffer; }
        VkSemaphore getImageAvailableSemaphore() const { return m_image_available; }
        VkFence getFence() const { return m_fence; }

        VulkanFrameLightingResource& getLightingResource() { return m_lighting_resource; }
        const VulkanFrameLightingResource& getLightingResource() const { return m_lighting_resource; }
        bool updateRayTracingScene(const VulkanAccelerationStructure* acceleration_structure) {
            return m_ray_tracing_scene_resource.update(acceleration_structure);
        }
        bool hasRayTracingScene() const { return m_ray_tracing_scene_resource.isReady(); }
        VkDescriptorSet getRayTracingSceneDescriptorSet() const {
            return m_ray_tracing_scene_resource.getDescriptorSet();
        }
        bool updateRayTracingShadingTable(
            const VulkanAccelerationStructure* acceleration_structure,
            std::span<const VulkanRayTracingInstanceRecord> accepted_instances,
            const VulkanRayTracingFallbackTextures& fallback_textures) {
            return m_ray_tracing_scene_table.update(
                acceleration_structure,
                accepted_instances,
                fallback_textures);
        }
        bool hasRayTracingShadingTable() const {
            return m_ray_tracing_scene_table.isReady();
        }
        void clearRayTracingShadingTable() { m_ray_tracing_scene_table.clear(); }
        VkDescriptorSet getRayTracingShadingTableDescriptorSet() const {
            return m_ray_tracing_scene_table.getDescriptorSet();
        }
        const VulkanRayTracingSceneTableStats& getRayTracingShadingTableStats() const {
            return m_ray_tracing_scene_table.getStats();
        }
        VulkanRayTracingSceneShadingTable& getRayTracingShadingTable() {
            return m_ray_tracing_scene_table;
        }
        const VulkanRayTracingSceneShadingTable& getRayTracingShadingTable() const {
            return m_ray_tracing_scene_table;
        }
        VulkanDebugDrawBuffer& getDebugDrawBuffer() { return m_debug_draw_buffer; }
        const VulkanDebugDrawBuffer& getDebugDrawBuffer() const { return m_debug_draw_buffer; }

    private:
        bool createCommandResources(uint32_t queue_family_index);
        bool createSyncObjects();

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
        VkSemaphore m_image_available = VK_NULL_HANDLE;
        VkFence m_fence = VK_NULL_HANDLE;
        VulkanFrameLightingResource m_lighting_resource;
        VulkanRayTracingSceneResource m_ray_tracing_scene_resource;
        VulkanRayTracingSceneShadingTable m_ray_tracing_scene_table;
        VulkanDebugDrawBuffer m_debug_draw_buffer;
        VulkanGpuTimestampQuery m_ray_query_gpu_timestamp;
        VulkanFrameSlotState m_flight_state;
        uint64_t m_ray_query_timing_submission_serial = 0;
        uint32_t m_frame_index = 0;
        bool m_ready = false;
    };
} // namespace NexAur
