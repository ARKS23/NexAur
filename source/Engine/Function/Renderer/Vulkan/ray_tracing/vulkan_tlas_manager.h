#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/diagnostics/vulkan_gpu_timestamp_query.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_acceleration_structure.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_scene_table.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_static_mesh_blas_cache.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    VkTransformMatrixKHR toVulkanTransformMatrix(const glm::mat4& transform);

    enum class VulkanTlasBuildMode : uint8_t {
        Build = 0,
        Update,
        Reuse
    };

    struct VulkanTlasBuildState {
        bool ready = false;
        bool update_capable = false;
        uint32_t instance_count = 0;
        uint32_t instance_capacity = 0;
        uint64_t topology_hash = 0;
        uint64_t content_hash = 0;
    };

    uint32_t growVulkanTlasInstanceCapacity(
        uint32_t required_count,
        uint32_t current_capacity);
    VulkanTlasBuildMode chooseVulkanTlasBuildMode(
        const VulkanTlasBuildState& previous,
        uint32_t instance_count,
        uint64_t topology_hash,
        uint64_t content_hash);
    const char* vulkanTlasBuildModeName(VulkanTlasBuildMode mode);

    struct VulkanTlasBuildStats {
        bool initialized = false;
        bool ready = false;
        uint32_t frame_index = 0;
        uint32_t source_instance_count = 0;
        uint32_t built_instance_count = 0;
        uint32_t skipped_blas_count = 0;
        uint32_t skipped_transform_count = 0;
        uint32_t skipped_material_count = 0;
        uint64_t build_count = 0;
        uint64_t rebuild_count = 0;
        uint64_t update_count = 0;
        uint64_t reuse_count = 0;
        uint64_t allocation_count = 0;
        uint64_t instance_buffer_bytes = 0;
        uint64_t instance_buffer_capacity_bytes = 0;
        uint64_t acceleration_structure_bytes = 0;
        uint64_t scratch_capacity_bytes = 0;
        uint32_t instance_capacity = 0;
        bool gpu_timing_supported = false;
        uint64_t gpu_timing_sample_count = 0;
        double last_build_gpu_ms = 0.0;
        std::string last_build_mode = "None";
        std::string last_failure_reason = "None";
    };

    class VulkanTlasManager final {
    public:
        VulkanTlasManager() = default;
        ~VulkanTlasManager();

        VulkanTlasManager(const VulkanTlasManager&) = delete;
        VulkanTlasManager& operator=(const VulkanTlasManager&) = delete;

        bool init(
            const VulkanResourceContext& context,
            const VulkanRayTracingDeviceFunctions& functions,
            VkDeviceSize scratch_alignment);
        void clear();
        void shutdown();

        bool buildFrame(
            uint32_t frame_index,
            std::span<const VulkanMeshDrawItem> opaque_items,
            const VulkanStaticMeshBlasCache& blas_cache);
        const VulkanAccelerationStructure* get(uint32_t frame_index) const;
        std::span<const VulkanRayTracingInstanceRecord> getAcceptedInstanceRecords(
            uint32_t frame_index) const;
        VulkanTlasBuildStats getStats() const;

        bool isInitialized() const { return m_initialized; }

    private:
        struct FrameSlot {
            FrameSlot() = default;
            FrameSlot(const FrameSlot&) = delete;
            FrameSlot& operator=(const FrameSlot&) = delete;
            FrameSlot(FrameSlot&&) noexcept = default;
            FrameSlot& operator=(FrameSlot&&) noexcept = default;

            VulkanOwnedBuffer instance_buffer;
            VulkanAccelerationStructure acceleration_structure;
            std::vector<VulkanRayTracingInstanceRecord> accepted_instances;
            uint32_t instance_count = 0;
            uint32_t instance_capacity = 0;
            uint64_t topology_hash = 0;
            uint64_t content_hash = 0;
            bool update_capable = false;
            bool ready = false;

            VulkanTlasBuildState buildState() const {
                VulkanTlasBuildState state;
                state.ready = ready &&
                              acceleration_structure.isReady() &&
                              instance_buffer.isReady();
                state.update_capable = update_capable;
                state.instance_count = instance_count;
                state.instance_capacity = instance_capacity;
                state.topology_hash = topology_hash;
                state.content_hash = content_hash;
                return state;
            }
        };

        bool createCommandPool();
        bool ensureInstanceBuffer(
            FrameSlot& slot,
            VkDeviceSize required_size,
            const char* debug_name);
        void resetSlot(FrameSlot& slot);
        void setFailure(std::string failure_reason);

        VulkanResourceContext m_context;
        VulkanRayTracingDeviceFunctions m_functions;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        VkDeviceSize m_scratch_alignment = 1;
        VulkanAccelerationStructureScratchBuffer m_scratch_buffer;
        VulkanGpuTimestampQuery m_gpu_timestamp_query;
        std::array<FrameSlot, kVulkanFramesInFlight> m_frame_slots;
        VulkanTlasBuildStats m_stats;
        bool m_initialized = false;
    };
} // namespace NexAur
