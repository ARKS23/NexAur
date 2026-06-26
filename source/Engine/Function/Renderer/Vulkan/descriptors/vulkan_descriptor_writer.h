#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"

namespace NexAur {
    class NEXAUR_API VulkanDescriptorWriter {
    public:
        VulkanDescriptorWriter& writeBuffer(
            uint32_t binding,
            VkDescriptorType descriptor_type,
            const VkDescriptorBufferInfo& buffer_info);

        VulkanDescriptorWriter& writeImage(
            uint32_t binding,
            VkDescriptorType descriptor_type,
            const VkDescriptorImageInfo& image_info);

        void update(VkDevice device, VkDescriptorSet descriptor_set) const;

    private:
        struct PendingWrite {
            uint32_t binding = 0;
            VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            uint32_t info_index = 0;
            bool is_image = false;
        };

    private:
        std::vector<VkDescriptorBufferInfo> m_buffer_infos;
        std::vector<VkDescriptorImageInfo> m_image_infos;
        std::vector<PendingWrite> m_writes;
    };
} // namespace NexAur
