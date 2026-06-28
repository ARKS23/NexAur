#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"

namespace NexAur {
    class VulkanPipelineCache;

    struct VulkanPostProcessPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat output_color_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* descriptor_allocator = nullptr;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   output_color_format != VK_FORMAT_UNDEFINED &&
                   input_descriptor_set_layout != VK_NULL_HANDLE &&
                   descriptor_allocator != nullptr &&
                   pipeline_cache != nullptr;
        }
    };

    struct VulkanPostProcessInput {
        VkImageView color_view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        bool valid() const {
            return color_view != VK_NULL_HANDLE &&
                   sampler != VK_NULL_HANDLE &&
                   layout != VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };

    struct VulkanPostProcessRenderTarget {
        VkImageView color_view = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};

        bool valid() const {
            return color_view != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0;
        }
    };

    class NEXAUR_API VulkanPostProcessPass {
    public:
        VulkanPostProcessPass() = default;
        ~VulkanPostProcessPass();

        VulkanPostProcessPass(const VulkanPostProcessPass&) = delete;
        VulkanPostProcessPass& operator=(const VulkanPostProcessPass&) = delete;

        bool recreateResources(const VulkanPostProcessPassContext& context);
        void cleanupResources();
        void shutdown();

        bool updateInput(const VulkanPostProcessInput& input);
        bool record(
            VkCommandBuffer command_buffer,
            const VulkanPostProcessRenderTarget& target,
            const RenderPostProcessSettings& settings);

        bool isReady() const {
            return m_pipeline != VK_NULL_HANDLE &&
                   m_pipeline_layout != VK_NULL_HANDLE &&
                   m_input_descriptor_set != VK_NULL_HANDLE;
        }
        VkFormat getOutputColorFormat() const { return m_output_color_format; }

    private:
        bool allocateDescriptorSet();
        bool createPipeline();
        void cleanupPipeline();
        void freeDescriptorSet();

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_output_color_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout m_input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanPipelineCache* m_pipeline_cache = nullptr;
        VulkanDescriptorSetAllocation m_input_descriptor_allocation;
        VkDescriptorSet m_input_descriptor_set = VK_NULL_HANDLE;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
