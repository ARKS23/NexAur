#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_acceleration_structure.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_static_mesh_blas_cache.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    VkTransformMatrixKHR toVulkanTransformMatrix(const glm::mat4& transform);

    struct VulkanTlasBuildStats {
        bool initialized = false;
        bool ready = false;
        uint32_t frame_index = 0;
        uint32_t source_instance_count = 0;
        uint32_t built_instance_count = 0;
        uint32_t skipped_blas_count = 0;
        uint32_t skipped_transform_count = 0;
        uint64_t build_count = 0;
        uint64_t instance_buffer_bytes = 0;
        uint64_t acceleration_structure_bytes = 0;
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
            uint32_t instance_count = 0;
            bool ready = false;
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
        std::array<FrameSlot, kVulkanFramesInFlight> m_frame_slots;
        VulkanTlasBuildStats m_stats;
        bool m_initialized = false;
    };
} // namespace NexAur
