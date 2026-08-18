#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"

namespace NexAur {
    class VulkanDescriptorWriter {
    public:
        VulkanDescriptorWriter& writeBuffer(
            uint32_t binding,
            VkDescriptorType descriptor_type,
            const VkDescriptorBufferInfo& buffer_info);

        VulkanDescriptorWriter& writeBufferArray(
            uint32_t binding,
            VkDescriptorType descriptor_type,
            std::span<const VkDescriptorBufferInfo> buffer_infos);

        VulkanDescriptorWriter& writeImage(
            uint32_t binding,
            VkDescriptorType descriptor_type,
            const VkDescriptorImageInfo& image_info);

        VulkanDescriptorWriter& writeImageArray(
            uint32_t binding,
            VkDescriptorType descriptor_type,
            std::span<const VkDescriptorImageInfo> image_infos);

        VulkanDescriptorWriter& writeAccelerationStructure(
            uint32_t binding,
            VkAccelerationStructureKHR acceleration_structure);

        void update(VkDevice device, VkDescriptorSet descriptor_set) const;

    private:
        struct PendingWrite {
            enum class InfoType : uint8_t {
                Buffer = 0,
                Image,
                AccelerationStructure
            };

            uint32_t binding = 0;
            VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            uint32_t info_index = 0;
            uint32_t descriptor_count = 1;
            InfoType info_type = InfoType::Buffer;
        };

    private:
        std::vector<VkDescriptorBufferInfo> m_buffer_infos;
        std::vector<VkDescriptorImageInfo> m_image_infos;
        std::vector<VkAccelerationStructureKHR> m_acceleration_structures;
        std::vector<PendingWrite> m_writes;
    };
} // namespace NexAur
