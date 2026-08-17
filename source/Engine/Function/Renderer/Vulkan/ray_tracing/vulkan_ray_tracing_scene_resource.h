#pragma once

#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"

namespace NexAur {
    class VulkanAccelerationStructure;

    class VulkanRayTracingSceneResource final {
    public:
        VulkanRayTracingSceneResource() = default;
        ~VulkanRayTracingSceneResource();

        VulkanRayTracingSceneResource(const VulkanRayTracingSceneResource&) = delete;
        VulkanRayTracingSceneResource& operator=(const VulkanRayTracingSceneResource&) = delete;

        bool init(
            VkDevice device,
            VulkanDescriptorAllocator& descriptor_allocator,
            VkDescriptorSetLayout descriptor_set_layout);
        void shutdown();

        bool update(const VulkanAccelerationStructure* acceleration_structure);

        bool isInitialized() const {
            return m_device != VK_NULL_HANDLE &&
                   m_descriptor_allocator != nullptr &&
                   m_descriptor_set_layout != VK_NULL_HANDLE &&
                   m_descriptor_allocation.valid();
        }

        bool isReady() const {
            return isInitialized() && m_acceleration_structure != VK_NULL_HANDLE;
        }

        VkDescriptorSet getDescriptorSet() const {
            return isReady() ? m_descriptor_allocation.set : VK_NULL_HANDLE;
        }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VkDescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorSetAllocation m_descriptor_allocation;
        VkAccelerationStructureKHR m_acceleration_structure = VK_NULL_HANDLE;
    };
} // namespace NexAur
