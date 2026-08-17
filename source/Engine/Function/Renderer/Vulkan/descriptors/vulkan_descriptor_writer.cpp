#include "pch.h"
#include "vulkan_descriptor_writer.h"

namespace NexAur {
    VulkanDescriptorWriter& VulkanDescriptorWriter::writeBuffer(
        uint32_t binding,
        VkDescriptorType descriptor_type,
        const VkDescriptorBufferInfo& buffer_info) {
        const uint32_t info_index = static_cast<uint32_t>(m_buffer_infos.size());
        m_buffer_infos.push_back(buffer_info);
        m_writes.push_back({
            binding,
            descriptor_type,
            info_index,
            PendingWrite::InfoType::Buffer
        });
        return *this;
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::writeImage(
        uint32_t binding,
        VkDescriptorType descriptor_type,
        const VkDescriptorImageInfo& image_info) {
        const uint32_t info_index = static_cast<uint32_t>(m_image_infos.size());
        m_image_infos.push_back(image_info);
        m_writes.push_back({
            binding,
            descriptor_type,
            info_index,
            PendingWrite::InfoType::Image
        });
        return *this;
    }

    VulkanDescriptorWriter& VulkanDescriptorWriter::writeAccelerationStructure(
        uint32_t binding,
        VkAccelerationStructureKHR acceleration_structure) {
        const uint32_t info_index = static_cast<uint32_t>(m_acceleration_structures.size());
        m_acceleration_structures.push_back(acceleration_structure);
        m_writes.push_back({
            binding,
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            info_index,
            PendingWrite::InfoType::AccelerationStructure
        });
        return *this;
    }

    void VulkanDescriptorWriter::update(VkDevice device, VkDescriptorSet descriptor_set) const {
        if (device == VK_NULL_HANDLE || descriptor_set == VK_NULL_HANDLE || m_writes.empty()) {
            return;
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(m_writes.size());
        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> acceleration_structure_writes;
        acceleration_structure_writes.reserve(m_writes.size());

        for (const PendingWrite& pending_write : m_writes) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptor_set;
            write.dstBinding = pending_write.binding;
            write.descriptorCount = 1;
            write.descriptorType = pending_write.descriptor_type;

            switch (pending_write.info_type) {
                case PendingWrite::InfoType::Image:
                    write.pImageInfo = &m_image_infos[pending_write.info_index];
                    break;
                case PendingWrite::InfoType::AccelerationStructure: {
                    VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info{};
                    acceleration_structure_info.sType =
                        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                    acceleration_structure_info.accelerationStructureCount = 1;
                    acceleration_structure_info.pAccelerationStructures =
                        &m_acceleration_structures[pending_write.info_index];
                    acceleration_structure_writes.push_back(acceleration_structure_info);
                    write.pNext = &acceleration_structure_writes.back();
                    break;
                }
                case PendingWrite::InfoType::Buffer:
                default:
                    write.pBufferInfo = &m_buffer_infos[pending_write.info_index];
                    break;
            }

            writes.push_back(write);
        }

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
} // namespace NexAur
