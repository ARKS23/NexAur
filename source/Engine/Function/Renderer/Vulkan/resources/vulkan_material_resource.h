#pragma once

#include <string>

#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class MaterialAsset;
    class VulkanDescriptorAllocator;
    class VulkanTextureResource;

    struct VulkanMaterialTextureSet {
        const VulkanTextureResource* base_color = nullptr;
        const VulkanTextureResource* normal = nullptr;
        const VulkanTextureResource* metallic = nullptr;
        const VulkanTextureResource* roughness = nullptr;
        const VulkanTextureResource* metallic_roughness = nullptr;
        const VulkanTextureResource* ao = nullptr;
        const VulkanTextureResource* emissive = nullptr;

        bool valid() const {
            return base_color != nullptr &&
                   normal != nullptr &&
                   metallic != nullptr &&
                   roughness != nullptr &&
                   metallic_roughness != nullptr &&
                   ao != nullptr &&
                   emissive != nullptr;
        }
    };

    struct VulkanMaterialResourceCreateContext {
        VulkanResourceUploadContext upload_context;
        VulkanDescriptorAllocator* descriptor_allocator = nullptr;
        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;

        bool valid() const {
            return upload_context.valid() &&
                   descriptor_allocator != nullptr &&
                   descriptor_set_layout != VK_NULL_HANDLE;
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
            const VulkanMaterialTextureSet& textures);
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
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanDescriptorSetAllocation m_descriptor_allocation;
        VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
        VkBuffer m_material_buffer = VK_NULL_HANDLE;
        VmaAllocation m_material_allocation = VK_NULL_HANDLE;
    };
} // namespace NexAur
