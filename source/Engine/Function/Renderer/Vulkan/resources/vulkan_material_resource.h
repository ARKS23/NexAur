#pragma once

#include <string>

#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class MaterialAsset;
    class VulkanTextureResource;

    struct VulkanMaterialResourceCreateContext {
        VulkanResourceUploadContext upload_context;
        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

        bool valid() const {
            return upload_context.valid() &&
                   descriptor_set_layout != VK_NULL_HANDLE &&
                   descriptor_pool != VK_NULL_HANDLE;
        }
    };

    class NEXAUR_API VulkanMaterialResource {
    public:
        VulkanMaterialResource() = default;
        ~VulkanMaterialResource();

        VulkanMaterialResource(const VulkanMaterialResource&) = delete;
        VulkanMaterialResource& operator=(const VulkanMaterialResource&) = delete;

        VulkanMaterialResource(VulkanMaterialResource&& other) noexcept;
        VulkanMaterialResource& operator=(VulkanMaterialResource&& other) noexcept;

        bool create(
            const VulkanMaterialResourceCreateContext& context,
            const MaterialAsset& material_asset,
            const VulkanTextureResource& base_color_texture);
        void reset();

        bool isReady() const {
            return m_material_buffer != VK_NULL_HANDLE &&
                   m_descriptor_set != VK_NULL_HANDLE;
        }

        const std::string& getDebugName() const { return m_debug_name; }
        VkDescriptorSet getDescriptorSet() const { return m_descriptor_set; }

    private:
        void moveFrom(VulkanMaterialResource&& other) noexcept;

    private:
        std::string m_debug_name;
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
        VkBuffer m_material_buffer = VK_NULL_HANDLE;
        VmaAllocation m_material_allocation = VK_NULL_HANDLE;
    };
} // namespace NexAur
