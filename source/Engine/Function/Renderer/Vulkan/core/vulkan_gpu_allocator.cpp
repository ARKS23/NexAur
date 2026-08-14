#include "pch.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

namespace NexAur {
    VulkanGpuAllocator::~VulkanGpuAllocator() {
        shutdown();
    }

    bool VulkanGpuAllocator::init(const VulkanResourceContext& context) {
        if (isInitialized()) {
            return true;
        }

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanGpuAllocator requires a valid VulkanResourceContext.");
            return false;
        }

        VmaAllocatorCreateInfo allocator_info{};
        allocator_info.vulkanApiVersion = context.api_version;
        allocator_info.instance = context.instance;
        allocator_info.physicalDevice = context.physical_device;
        allocator_info.device = context.device;

        const VkResult result = vmaCreateAllocator(&allocator_info, &m_allocator);
        if (!VulkanDiagnosticsCollector::checkVk(result, "vmaCreateAllocator")) {
            m_allocator = VK_NULL_HANDLE;
            return false;
        }

        m_device = context.device;
        m_retirement_queue = context.retirement_queue;
        return true;
    }

    void VulkanGpuAllocator::shutdown() {
        if (m_allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }

        m_device = VK_NULL_HANDLE;
        m_retirement_queue = nullptr;
    }

    bool VulkanGpuAllocator::createImage(
        const VkImageCreateInfo& image_info,
        VmaMemoryUsage memory_usage,
        VkImage& image,
        VmaAllocation& allocation,
        const char* operation) const {
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        if (!isInitialized()) {
            NX_CORE_ERROR("{} failed: VulkanGpuAllocator is not initialized.", operation);
            return false;
        }

        VmaAllocationCreateInfo allocation_info{};
        allocation_info.usage = memory_usage;
        const VkResult result = vmaCreateImage(
            m_allocator,
            &image_info,
            &allocation_info,
            &image,
            &allocation,
            nullptr);
        if (!VulkanDiagnosticsCollector::checkVk(result, operation)) {
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            return false;
        }

        if (operation != nullptr && operation[0] != '\0') {
            vmaSetAllocationName(m_allocator, allocation, operation);
        }

        return true;
    }

    bool VulkanGpuAllocator::createBuffer(
        const VkBufferCreateInfo& buffer_info,
        VmaMemoryUsage memory_usage,
        VmaAllocationCreateFlags allocation_flags,
        VkBuffer& buffer,
        VmaAllocation& allocation,
        const char* operation) const {
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        if (!isInitialized()) {
            NX_CORE_ERROR("{} failed: VulkanGpuAllocator is not initialized.", operation);
            return false;
        }

        VmaAllocationCreateInfo allocation_info{};
        allocation_info.usage = memory_usage;
        allocation_info.flags = allocation_flags;
        const VkResult result = vmaCreateBuffer(
            m_allocator,
            &buffer_info,
            &allocation_info,
            &buffer,
            &allocation,
            nullptr);
        if (!VulkanDiagnosticsCollector::checkVk(result, operation)) {
            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            return false;
        }

        if (operation != nullptr && operation[0] != '\0') {
            vmaSetAllocationName(m_allocator, allocation, operation);
        }

        return true;
    }

    void VulkanGpuAllocator::destroyImage(VkImage& image, VmaAllocation& allocation) const {
        if (m_allocator != VK_NULL_HANDLE && image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(m_allocator, image, allocation);
        }

        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }

    void VulkanGpuAllocator::destroyBuffer(VkBuffer& buffer, VmaAllocation& allocation) const {
        if (m_allocator != VK_NULL_HANDLE && buffer != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_allocator, buffer, allocation);
        }

        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
} // namespace NexAur
