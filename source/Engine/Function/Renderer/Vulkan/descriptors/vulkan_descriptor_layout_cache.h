#pragma once

#include <unordered_map>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"

namespace NexAur {
    class NEXAUR_API VulkanDescriptorLayoutCache {
    public:
        VulkanDescriptorLayoutCache() = default;
        ~VulkanDescriptorLayoutCache();

        VulkanDescriptorLayoutCache(const VulkanDescriptorLayoutCache&) = delete;
        VulkanDescriptorLayoutCache& operator=(const VulkanDescriptorLayoutCache&) = delete;

        bool init(VkDevice device);
        void shutdown();

        VkDescriptorSetLayout getOrCreateLayout(VulkanDescriptorSetLayoutDesc desc);
        VkDescriptorSetLayout getBuiltinLayout(VulkanDescriptorSetLayoutId layout_id);

        bool isInitialized() const { return m_device != VK_NULL_HANDLE; }

    private:
        VkDescriptorSetLayout createLayout(const VulkanDescriptorSetLayoutDesc& desc);
        bool hasDuplicateBindings(const VulkanDescriptorSetLayoutDesc& desc) const;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        std::unordered_map<VulkanDescriptorSetLayoutDesc, VkDescriptorSetLayout, VulkanDescriptorSetLayoutDescHash> m_layouts;
    };
} // namespace NexAur
