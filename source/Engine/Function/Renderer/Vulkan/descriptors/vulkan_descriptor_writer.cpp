#include "pch.h"
#include "vulkan_descriptor_writer.h"

namespace NexAur {
    VulkanDescriptorWriter& VulkanDescriptorWriter::writeBuffer(
        uint32_t binding,
        VkDescriptorType descriptor_type,
        const VkDescriptorBufferInfo& buffer_info) {
        const uint32_t info_index = static_cast<uint32_t>(m_buffer_infos.size());
        m_buffer_infos.push_back(buffer_info);
        m_writes.push_back({ binding, descriptor_type, info_index, false });
        return *this;
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::writeImage(
        uint32_t binding,
        VkDescriptorType descriptor_type,
        const VkDescriptorImageInfo& image_info) {
        const uint32_t info_index = static_cast<uint32_t>(m_image_infos.size());
        m_image_infos.push_back(image_info);
        m_writes.push_back({ binding, descriptor_type, info_index, true });
        return *this;
    }

    void VulkanDescriptorWriter::update(VkDevice device, VkDescriptorSet descriptor_set) const {
        if (device == VK_NULL_HANDLE || descriptor_set == VK_NULL_HANDLE || m_writes.empty()) {
            return;
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(m_writes.size());

        for (const PendingWrite& pending_write : m_writes) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptor_set;
            write.dstBinding = pending_write.binding;
            write.descriptorCount = 1;
            write.descriptorType = pending_write.descriptor_type;

            if (pending_write.is_image) {
                write.pImageInfo = &m_image_infos[pending_write.info_index];
            } else {
                write.pBufferInfo = &m_buffer_infos[pending_write.info_index];
            }

            writes.push_back(write);
        }

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
} // namespace NexAur
