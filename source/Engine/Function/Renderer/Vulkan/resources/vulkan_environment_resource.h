#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class EnvironmentMapAsset;
    class VulkanDescriptorAllocator;

    struct VulkanEnvironmentResourceCreateContext {
        VulkanResourceUploadContext upload_context;
        VulkanDescriptorAllocator* descriptor_allocator = nullptr;
        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;

        bool valid() const {
            return upload_context.valid() &&
                   descriptor_allocator != nullptr &&
                   descriptor_set_layout != VK_NULL_HANDLE;
        }
    };

    class NEXAUR_API VulkanEnvironmentResource {
    public:
        VulkanEnvironmentResource() = default;
        ~VulkanEnvironmentResource();

        VulkanEnvironmentResource(const VulkanEnvironmentResource&) = delete;
        VulkanEnvironmentResource& operator=(const VulkanEnvironmentResource&) = delete;

        bool create(
            const VulkanEnvironmentResourceCreateContext& context,
            const EnvironmentMapAsset& environment_asset);
        bool createFallback(
            const VulkanEnvironmentResourceCreateContext& context,
            const glm::vec3& color);
        void reset();

        bool isReady() const {
            return m_descriptor_set != VK_NULL_HANDLE &&
                   m_environment.image != VK_NULL_HANDLE &&
                   m_irradiance.image != VK_NULL_HANDLE &&
                   m_prefiltered.image != VK_NULL_HANDLE &&
                   m_brdf_lut.image != VK_NULL_HANDLE;
        }

        const std::string& getDebugName() const { return m_debug_name; }
        VkDescriptorSet getDescriptorSet() const { return m_descriptor_set; }
        uint32_t getSourceWidth() const { return m_source_width; }
        uint32_t getSourceHeight() const { return m_source_height; }
        uint32_t getEnvironmentSize() const { return m_environment.width; }
        uint32_t getIrradianceSize() const { return m_irradiance.width; }
        uint32_t getPrefilterSize() const { return m_prefiltered.width; }
        uint32_t getPrefilterMipCount() const { return m_prefiltered.mip_count; }
        bool hasBrdfLut() const { return m_brdf_lut.image != VK_NULL_HANDLE; }

        struct ImageResource {
            VkImage image = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
            VkSampler sampler = VK_NULL_HANDLE;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t mip_count = 0;
            uint32_t layer_count = 0;
            bool cube = false;
        };

    private:
        void destroyImage(ImageResource& image);
        bool updateDescriptorSet();

    private:
        std::string m_debug_name;
        uint32_t m_source_width = 0;
        uint32_t m_source_height = 0;
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanDescriptorSetAllocation m_descriptor_allocation;
        VkDescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
        ImageResource m_environment;
        ImageResource m_irradiance;
        ImageResource m_prefiltered;
        ImageResource m_brdf_lut;
    };
} // namespace NexAur
