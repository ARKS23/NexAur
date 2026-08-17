#pragma once

#include <cstdint>
#include <string>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanRetirementQueue;

    class VulkanGpuAllocator final {
    public:
        VulkanGpuAllocator() = default;
        ~VulkanGpuAllocator();

        VulkanGpuAllocator(const VulkanGpuAllocator&) = delete;
        VulkanGpuAllocator& operator=(const VulkanGpuAllocator&) = delete;

        bool init(const VulkanResourceContext& context);
        void shutdown();

        bool isInitialized() const { return m_allocator != VK_NULL_HANDLE; }
        VmaAllocator getHandle() const { return m_allocator; }
        VkDevice getDevice() const { return m_device; }
        VulkanRetirementQueue* getRetirementQueue() const { return m_retirement_queue; }
        bool isBufferDeviceAddressEnabled() const { return m_buffer_device_address_enabled; }

        bool createImage(
            const VkImageCreateInfo& image_info,
            VmaMemoryUsage memory_usage,
            VkImage& image,
            VmaAllocation& allocation,
            const char* operation) const;
        bool createBuffer(
            const VkBufferCreateInfo& buffer_info,
            VmaMemoryUsage memory_usage,
            VmaAllocationCreateFlags allocation_flags,
            VkDeviceSize minimum_alignment,
            VkBuffer& buffer,
            VmaAllocation& allocation,
            const char* operation) const;

        VkDeviceAddress getBufferDeviceAddress(
            VkBuffer buffer,
            const char* operation) const;

        void destroyImage(VkImage& image, VmaAllocation& allocation) const;
        void destroyBuffer(VkBuffer& buffer, VmaAllocation& allocation) const;

    private:
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanRetirementQueue* m_retirement_queue = nullptr;
        bool m_buffer_device_address_enabled = false;
    };
} // namespace NexAur
