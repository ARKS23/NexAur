#pragma once

#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanDescriptorAllocator;
    class VulkanDescriptorLayoutCache;
    class VulkanPipelineCache;

    struct VulkanRenderFeatureContext {
        VulkanResourceContext resources;
        VulkanDescriptorLayoutCache* descriptor_layout_cache = nullptr;
        VulkanDescriptorAllocator* descriptor_allocator = nullptr;
        VulkanPipelineCache* pipeline_cache = nullptr;
        bool ray_query_enabled = false;

        bool valid() const {
            return resources.valid() &&
                   resources.gpu_allocator != nullptr &&
                   descriptor_layout_cache != nullptr &&
                   descriptor_allocator != nullptr &&
                   pipeline_cache != nullptr;
        }
    };

    struct VulkanFeatureImageInput {
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkExtent2D extent{};
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        bool valid() const {
            return view != VK_NULL_HANDLE &&
                   sampler != VK_NULL_HANDLE &&
                   extent.width > 0 &&
                   extent.height > 0 &&
                   layout != VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };
} // namespace NexAur
