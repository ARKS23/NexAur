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
        if (context.buffer_device_address_enabled) {
            allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }

        const VkResult result = vmaCreateAllocator(&allocator_info, &m_allocator);
        if (!VulkanDiagnosticsCollector::checkVk(result, "vmaCreateAllocator")) {
            m_allocator = VK_NULL_HANDLE;
            return false;
        }

        m_device = context.device;
        m_retirement_queue = context.retirement_queue;
        m_buffer_device_address_enabled = context.buffer_device_address_enabled;
        return true;
    }

    void VulkanGpuAllocator::shutdown() {
        if (m_allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }

        m_device = VK_NULL_HANDLE;
        m_retirement_queue = nullptr;
        m_buffer_device_address_enabled = false;
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
        VkDeviceSize minimum_alignment,
        VkBuffer& buffer,
        VmaAllocation& allocation,
        const char* operation) const {
        const char* operation_name =
            operation != nullptr && operation[0] != '\0' ? operation : "vmaCreateBuffer";
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        if (!isInitialized()) {
            NX_CORE_ERROR("{} failed: VulkanGpuAllocator is not initialized.", operation_name);
            return false;
        }
        if (minimum_alignment == 0 ||
            (minimum_alignment & (minimum_alignment - 1)) != 0) {
            NX_CORE_ERROR("{} failed: minimum alignment must be a power of two.", operation_name);
            return false;
        }
        if ((buffer_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0 &&
            !m_buffer_device_address_enabled) {
            NX_CORE_ERROR(
                "{} failed: buffer device address usage was requested without an enabled allocator capability.",
                operation_name);
            return false;
        }

        VmaAllocationCreateInfo allocation_info{};
        allocation_info.usage = memory_usage;
        allocation_info.flags = allocation_flags;
        const VkResult result = minimum_alignment > 1 ?
            vmaCreateBufferWithAlignment(
                m_allocator,
                &buffer_info,
                &allocation_info,
                minimum_alignment,
                &buffer,
                &allocation,
                nullptr) :
            vmaCreateBuffer(
                m_allocator,
                &buffer_info,
                &allocation_info,
                &buffer,
                &allocation,
                nullptr);
        if (!VulkanDiagnosticsCollector::checkVk(result, operation_name)) {
            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            return false;
        }

        if (operation_name[0] != '\0') {
            vmaSetAllocationName(m_allocator, allocation, operation_name);
        }

        return true;
    }

    VkDeviceAddress VulkanGpuAllocator::getBufferDeviceAddress(
        VkBuffer buffer,
        const char* operation) const {
        const char* operation_name =
            operation != nullptr && operation[0] != '\0' ? operation : "vkGetBufferDeviceAddress";
        if (!isInitialized() || !m_buffer_device_address_enabled || buffer == VK_NULL_HANDLE) {
            NX_CORE_ERROR(
                "{} failed: allocator, buffer, or buffer device address capability is invalid.",
                operation_name);
            return 0;
        }

        VkBufferDeviceAddressInfo address_info{};
        address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        address_info.buffer = buffer;
        const VkDeviceAddress address = vkGetBufferDeviceAddress(m_device, &address_info);
        if (address == 0) {
            NX_CORE_ERROR("{} failed: Vulkan returned a zero device address.", operation_name);
        }
        return address;
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
