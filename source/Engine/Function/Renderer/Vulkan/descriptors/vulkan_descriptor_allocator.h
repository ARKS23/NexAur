#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"

namespace NexAur {
    struct VulkanDescriptorSetAllocation {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;

        bool valid() const {
            return pool != VK_NULL_HANDLE &&
                   set != VK_NULL_HANDLE;
        }
    };

    class NEXAUR_API VulkanDescriptorAllocator {
    public:
        VulkanDescriptorAllocator() = default;
        ~VulkanDescriptorAllocator();

        VulkanDescriptorAllocator(const VulkanDescriptorAllocator&) = delete;
        VulkanDescriptorAllocator& operator=(const VulkanDescriptorAllocator&) = delete;

        bool init(VkDevice device);
        void shutdown();

        VulkanDescriptorSetAllocation allocate(VkDescriptorSetLayout layout);
        void free(const VulkanDescriptorSetAllocation& allocation);

        bool isInitialized() const { return m_device != VK_NULL_HANDLE; }

    private:
        VkDescriptorPool createPool(uint32_t max_sets) const;
        VulkanDescriptorSetAllocation allocateFromPool(VkDescriptorPool pool, VkDescriptorSetLayout layout) const;
        bool isPoolCapacityError(VkResult result) const;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        std::vector<VkDescriptorPool> m_pools;
        uint32_t m_next_pool_set_count = 128;
    };
} // namespace NexAur
