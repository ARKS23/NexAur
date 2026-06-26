#include "pch.h"
#include "vulkan_descriptor_allocator.h"

#include <algorithm>
#include <array>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        constexpr uint32_t kMaxDescriptorSetsPerPool = 4096;

        std::array<VkDescriptorPoolSize, 5> descriptorPoolSizes(uint32_t max_sets) {
            std::array<VkDescriptorPoolSize, 5> pool_sizes{};
            pool_sizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_sets * 2 };
            pool_sizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_sets };
            pool_sizes[2] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_sets * 4 };
            pool_sizes[3] = { VK_DESCRIPTOR_TYPE_SAMPLER, max_sets * 4 };
            pool_sizes[4] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_sets * 4 };
            return pool_sizes;
        }
    } // namespace

    VulkanDescriptorAllocator::~VulkanDescriptorAllocator() {
        shutdown();
    }

    bool VulkanDescriptorAllocator::init(VkDevice device) {
        shutdown();

        if (device == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanDescriptorAllocator requires a valid device.");
            return false;
        }

        m_device = device;
        m_next_pool_set_count = 128;
        return true;
    }

    void VulkanDescriptorAllocator::shutdown() {
        if (m_device != VK_NULL_HANDLE) {
            for (VkDescriptorPool pool : m_pools) {
                if (pool != VK_NULL_HANDLE) {
                    vkDestroyDescriptorPool(m_device, pool, nullptr);
                }
            }
        }

        m_pools.clear();
        m_next_pool_set_count = 128;
        m_device = VK_NULL_HANDLE;
    }

    VulkanDescriptorSetAllocation VulkanDescriptorAllocator::allocate(VkDescriptorSetLayout layout) {
        if (m_device == VK_NULL_HANDLE || layout == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanDescriptorAllocator requires a valid device and layout.");
            return {};
        }

        for (VkDescriptorPool pool : m_pools) {
            VulkanDescriptorSetAllocation allocation = allocateFromPool(pool, layout);
            if (allocation.valid()) {
                return allocation;
            }
        }

        VkDescriptorPool new_pool = createPool(m_next_pool_set_count);
        if (new_pool == VK_NULL_HANDLE) {
            return {};
        }

        m_pools.push_back(new_pool);
        m_next_pool_set_count = std::min(m_next_pool_set_count * 2, kMaxDescriptorSetsPerPool);
        return allocateFromPool(new_pool, layout);
    }

    void VulkanDescriptorAllocator::free(const VulkanDescriptorSetAllocation& allocation) {
        if (m_device == VK_NULL_HANDLE || !allocation.valid()) {
            return;
        }

        checkVk(
            vkFreeDescriptorSets(m_device, allocation.pool, 1, &allocation.set),
            "vkFreeDescriptorSets");
    }

    VkDescriptorPool VulkanDescriptorAllocator::createPool(uint32_t max_sets) const {
        const std::array<VkDescriptorPoolSize, 5> pool_sizes = descriptorPoolSizes(max_sets);

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = max_sets;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (!checkVk(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &pool), "vkCreateDescriptorPool(descriptor allocator)")) {
            return VK_NULL_HANDLE;
        }

        return pool;
    }

    VulkanDescriptorSetAllocation VulkanDescriptorAllocator::allocateFromPool(VkDescriptorPool pool, VkDescriptorSetLayout layout) const {
        VkDescriptorSetAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = pool;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &layout;

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        const VkResult result = vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set);
        if (result == VK_SUCCESS) {
            return { pool, descriptor_set };
        }

        if (!isPoolCapacityError(result)) {
            NX_CORE_ERROR("vkAllocateDescriptorSets failed: {}", static_cast<int>(result));
        }

        return {};
    }

    bool VulkanDescriptorAllocator::isPoolCapacityError(VkResult result) const {
        return result == VK_ERROR_OUT_OF_POOL_MEMORY ||
               result == VK_ERROR_FRAGMENTED_POOL;
    }
} // namespace NexAur
