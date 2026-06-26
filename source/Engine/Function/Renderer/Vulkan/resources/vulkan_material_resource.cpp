#include "pch.h"
#include "vulkan_material_resource.h"

#include "Function/Resource/material_asset.h"
#include "Function/Renderer/Vulkan/resources/vulkan_texture_resource.h"

#include <array>
#include <cstring>
#include <utility>

namespace NexAur {
    namespace {
        struct VulkanMaterialConstants {
            glm::vec4 base_color_factor{ 1.0f };
            glm::vec4 factors{ 0.0f, 1.0f, 0.5f, 0.0f };
        };

        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        float alphaModeToFloat(MaterialAlphaMode alpha_mode) {
            return static_cast<float>(static_cast<int>(alpha_mode));
        }
    } // namespace

    VulkanMaterialResource::~VulkanMaterialResource() {
        reset();
    }

    VulkanMaterialResource::VulkanMaterialResource(VulkanMaterialResource&& other) noexcept {
        moveFrom(std::move(other));
    }

    VulkanMaterialResource& VulkanMaterialResource::operator=(VulkanMaterialResource&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(std::move(other));
        }
        return *this;
    }

    bool VulkanMaterialResource::create(
        const VulkanMaterialResourceCreateContext& context,
        const MaterialAsset& material_asset,
        const VulkanTextureResource& base_color_texture) {
        reset();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanMaterialResource requires a valid create context.");
            return false;
        }
        if (!base_color_texture.isReady()) {
            NX_CORE_ERROR("VulkanMaterialResource requires a ready base color texture.");
            return false;
        }

        m_allocator = context.upload_context.allocator;
        m_device = context.upload_context.device;
        m_descriptor_pool = context.descriptor_pool;
        m_debug_name = material_asset.getDebugName();

        VulkanMaterialConstants constants;
        constants.base_color_factor = material_asset.getBaseColorFactor();
        constants.factors = glm::vec4{
            material_asset.getMetallicFactor(),
            material_asset.getRoughnessFactor(),
            material_asset.getAlphaCutoff(),
            alphaModeToFloat(material_asset.getAlphaMode())
        };

        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = sizeof(VulkanMaterialConstants);
        buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocation_info{};
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
        allocation_info.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo created_allocation_info{};
        if (!checkVk(
                vmaCreateBuffer(
                    m_allocator,
                    &buffer_info,
                    &allocation_info,
                    &m_material_buffer,
                    &m_material_allocation,
                    &created_allocation_info),
                "vmaCreateBuffer(material constants)")) {
            reset();
            return false;
        }

        if (!created_allocation_info.pMappedData) {
            NX_CORE_ERROR("Failed to map Vulkan material constant buffer.");
            reset();
            return false;
        }

        std::memcpy(created_allocation_info.pMappedData, &constants, sizeof(constants));
        vmaFlushAllocation(m_allocator, m_material_allocation, 0, VK_WHOLE_SIZE);

        VkDescriptorSetAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = context.descriptor_pool;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &context.descriptor_set_layout;
        if (!checkVk(vkAllocateDescriptorSets(m_device, &allocate_info, &m_descriptor_set), "vkAllocateDescriptorSets(material)")) {
            reset();
            return false;
        }

        VkDescriptorBufferInfo material_buffer_info{};
        material_buffer_info.buffer = m_material_buffer;
        material_buffer_info.offset = 0;
        material_buffer_info.range = sizeof(VulkanMaterialConstants);

        VkDescriptorImageInfo image_info{};
        image_info.imageView = base_color_texture.getImageView();
        image_info.imageLayout = base_color_texture.getImageLayout();

        VkDescriptorImageInfo sampler_info{};
        sampler_info.sampler = base_color_texture.getSampler();

        std::array<VkWriteDescriptorSet, 3> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_descriptor_set;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &material_buffer_info;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_descriptor_set;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[1].pImageInfo = &image_info;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = m_descriptor_set;
        writes[2].dstBinding = 2;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[2].pImageInfo = &sampler_info;

        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        return true;
    }

    void VulkanMaterialResource::reset() {
        if (m_device != VK_NULL_HANDLE &&
            m_descriptor_pool != VK_NULL_HANDLE &&
            m_descriptor_set != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(m_device, m_descriptor_pool, 1, &m_descriptor_set);
        }

        if (m_allocator != VK_NULL_HANDLE && m_material_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_allocator, m_material_buffer, m_material_allocation);
        }

        m_debug_name.clear();
        m_allocator = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_descriptor_pool = VK_NULL_HANDLE;
        m_descriptor_set = VK_NULL_HANDLE;
        m_material_buffer = VK_NULL_HANDLE;
        m_material_allocation = VK_NULL_HANDLE;
    }

    void VulkanMaterialResource::moveFrom(VulkanMaterialResource&& other) noexcept {
        m_debug_name = std::move(other.m_debug_name);
        m_allocator = other.m_allocator;
        m_device = other.m_device;
        m_descriptor_pool = other.m_descriptor_pool;
        m_descriptor_set = other.m_descriptor_set;
        m_material_buffer = other.m_material_buffer;
        m_material_allocation = other.m_material_allocation;

        other.m_allocator = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_descriptor_pool = VK_NULL_HANDLE;
        other.m_descriptor_set = VK_NULL_HANDLE;
        other.m_material_buffer = VK_NULL_HANDLE;
        other.m_material_allocation = VK_NULL_HANDLE;
    }
} // namespace NexAur
