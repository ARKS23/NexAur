#pragma once

#include <span>
#include <string>

#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_functions.h"

namespace NexAur {
    class VulkanGpuAllocator;
    class VulkanRetirementQueue;

    struct VulkanAccelerationStructureBuildSizes {
        VkDeviceSize acceleration_structure_size = 0;
        VkDeviceSize build_scratch_size = 0;
        VkDeviceSize update_scratch_size = 0;

        bool valid() const {
            return acceleration_structure_size > 0 && build_scratch_size > 0;
        }
    };

    struct VulkanAccelerationStructureCreateInfo {
        VkAccelerationStructureTypeKHR type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
        VkDeviceSize size = 0;
        std::string debug_name;

        bool valid() const {
            return (type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR ||
                    type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR) &&
                   size > 0;
        }
    };

    VkDeviceSize alignVulkanAccelerationStructureScratchSize(
        VkDeviceSize size,
        VkDeviceSize alignment);

    bool queryVulkanAccelerationStructureBuildSizes(
        VkDevice device,
        const VulkanRayTracingDeviceFunctions& functions,
        VkAccelerationStructureBuildTypeKHR build_type,
        const VkAccelerationStructureBuildGeometryInfoKHR& build_info,
        std::span<const uint32_t> max_primitive_counts,
        VulkanAccelerationStructureBuildSizes& sizes);

    class VulkanAccelerationStructureScratchBuffer final {
    public:
        VulkanAccelerationStructureScratchBuffer() = default;
        ~VulkanAccelerationStructureScratchBuffer() = default;

        VulkanAccelerationStructureScratchBuffer(
            const VulkanAccelerationStructureScratchBuffer&) = delete;
        VulkanAccelerationStructureScratchBuffer& operator=(
            const VulkanAccelerationStructureScratchBuffer&) = delete;

        VulkanAccelerationStructureScratchBuffer(
            VulkanAccelerationStructureScratchBuffer&& other) noexcept;
        VulkanAccelerationStructureScratchBuffer& operator=(
            VulkanAccelerationStructureScratchBuffer&& other) noexcept;

        bool ensureCapacity(
            const VulkanGpuAllocator& allocator,
            VkDeviceSize required_size,
            VkDeviceSize alignment,
            const char* debug_name);
        void reset();

        bool isReady() const { return m_buffer.isReady() && m_buffer.isDeviceAddressable(); }
        VkBuffer getBuffer() const { return m_buffer.get(); }
        VkDeviceAddress getDeviceAddress() const { return m_buffer.getDeviceAddress(); }
        VkDeviceSize getCapacity() const { return m_capacity; }
        VkDeviceSize getAlignment() const { return m_alignment; }

    private:
        void moveFrom(VulkanAccelerationStructureScratchBuffer&& other) noexcept;

        VulkanOwnedBuffer m_buffer;
        VkDeviceSize m_capacity = 0;
        VkDeviceSize m_alignment = 1;
    };

    class VulkanAccelerationStructure final {
    public:
        VulkanAccelerationStructure() = default;
        ~VulkanAccelerationStructure();

        VulkanAccelerationStructure(const VulkanAccelerationStructure&) = delete;
        VulkanAccelerationStructure& operator=(const VulkanAccelerationStructure&) = delete;

        VulkanAccelerationStructure(VulkanAccelerationStructure&& other) noexcept;
        VulkanAccelerationStructure& operator=(VulkanAccelerationStructure&& other) noexcept;

        bool create(
            const VulkanGpuAllocator& allocator,
            const VulkanRayTracingDeviceFunctions& functions,
            const VulkanAccelerationStructureCreateInfo& create_info);
        bool recordBuild(
            VkCommandBuffer command_buffer,
            VkAccelerationStructureBuildGeometryInfoKHR build_info,
            std::span<const VkAccelerationStructureBuildRangeInfoKHR> build_ranges,
            VkDeviceAddress scratch_address) const;
        void reset();

        bool isReady() const {
            return m_handle != VK_NULL_HANDLE &&
                   m_backing_buffer.isReady() &&
                   m_device_address != 0;
        }
        VkAccelerationStructureKHR get() const { return m_handle; }
        VkAccelerationStructureTypeKHR getType() const { return m_type; }
        VkDeviceAddress getDeviceAddress() const { return m_device_address; }
        VkDeviceSize getSize() const { return m_size; }
        VkBuffer getBackingBuffer() const { return m_backing_buffer.get(); }
        const std::string& getDebugName() const { return m_debug_name; }

    private:
        void moveFrom(VulkanAccelerationStructure&& other) noexcept;

        VulkanRayTracingDeviceFunctions m_functions;
        VulkanRetirementQueue* m_retirement_queue = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanOwnedBuffer m_backing_buffer;
        VkAccelerationStructureKHR m_handle = VK_NULL_HANDLE;
        VkAccelerationStructureTypeKHR m_type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
        VkDeviceAddress m_device_address = 0;
        VkDeviceSize m_size = 0;
        std::string m_debug_name;
    };
} // namespace NexAur
