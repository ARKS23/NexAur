#include "pch.h"
#include "vulkan_material_resource.h"

#include "Function/Resource/material_asset.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/resources/vulkan_texture_resource.h"

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
        m_descriptor_allocator = context.descriptor_allocator;
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

        m_descriptor_allocation = m_descriptor_allocator->allocate(context.descriptor_set_layout);
        if (!m_descriptor_allocation.valid()) {
            NX_CORE_ERROR("Failed to allocate Vulkan material descriptor set.");
            reset();
            return false;
        }
        m_descriptor_set = m_descriptor_allocation.set;

        VkDescriptorBufferInfo material_buffer_info{};
        material_buffer_info.buffer = m_material_buffer;
        material_buffer_info.offset = 0;
        material_buffer_info.range = sizeof(VulkanMaterialConstants);

        VkDescriptorImageInfo image_info{};
        image_info.imageView = base_color_texture.getImageView();
        image_info.imageLayout = base_color_texture.getImageLayout();

        VkDescriptorImageInfo sampler_info{};
        sampler_info.sampler = base_color_texture.getSampler();

        VulkanDescriptorWriter()
            .writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, material_buffer_info)
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, image_info)
            .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLER, sampler_info)
            .update(m_device, m_descriptor_set);
        return true;
    }

    void VulkanMaterialResource::reset() {
        if (m_descriptor_allocator != nullptr && m_descriptor_allocation.valid()) {
            m_descriptor_allocator->free(m_descriptor_allocation);
        }

        if (m_allocator != VK_NULL_HANDLE && m_material_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_allocator, m_material_buffer, m_material_allocation);
        }

        m_debug_name.clear();
        m_allocator = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_descriptor_allocation = {};
        m_descriptor_set = VK_NULL_HANDLE;
        m_material_buffer = VK_NULL_HANDLE;
        m_material_allocation = VK_NULL_HANDLE;
    }

    void VulkanMaterialResource::moveFrom(VulkanMaterialResource&& other) noexcept {
        m_debug_name = std::move(other.m_debug_name);
        m_allocator = other.m_allocator;
        m_device = other.m_device;
        m_descriptor_allocator = other.m_descriptor_allocator;
        m_descriptor_allocation = other.m_descriptor_allocation;
        m_descriptor_set = other.m_descriptor_set;
        m_material_buffer = other.m_material_buffer;
        m_material_allocation = other.m_material_allocation;

        other.m_allocator = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_descriptor_allocator = nullptr;
        other.m_descriptor_allocation = {};
        other.m_descriptor_set = VK_NULL_HANDLE;
        other.m_material_buffer = VK_NULL_HANDLE;
        other.m_material_allocation = VK_NULL_HANDLE;
    }
} // namespace NexAur
