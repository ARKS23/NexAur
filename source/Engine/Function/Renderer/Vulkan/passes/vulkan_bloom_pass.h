#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"
#include "Function/Renderer/Vulkan/targets/vulkan_bloom_target.h"

namespace NexAur {
    class VulkanPipelineCache;

    struct VulkanBloomPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout single_input_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout dual_input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* descriptor_allocator = nullptr;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   single_input_descriptor_set_layout != VK_NULL_HANDLE &&
                   dual_input_descriptor_set_layout != VK_NULL_HANDLE &&
                   descriptor_allocator != nullptr &&
                   pipeline_cache != nullptr;
        }
    };

    struct VulkanBloomInput {
        VkImageView color_view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkExtent2D extent{};
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        bool valid() const {
            return color_view != VK_NULL_HANDLE &&
                   sampler != VK_NULL_HANDLE &&
                   extent.width > 0 &&
                   extent.height > 0 &&
                   layout != VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };

    class NEXAUR_API VulkanBloomPass {
    public:
        VulkanBloomPass() = default;
        ~VulkanBloomPass();

        VulkanBloomPass(const VulkanBloomPass&) = delete;
        VulkanBloomPass& operator=(const VulkanBloomPass&) = delete;

        bool recreateResources(const VulkanBloomPassContext& context, uint32_t mip_count);
        void cleanupResources();
        void shutdown();

        bool updateInputs(const VulkanBloomInput& scene_color, const VulkanBloomTarget& target);

        bool recordDownsample(
            VkCommandBuffer command_buffer,
            uint32_t mip_index,
            const VulkanBloomRenderTarget& target);
        bool recordUpsample(
            VkCommandBuffer command_buffer,
            uint32_t mip_index,
            const VulkanBloomRenderTarget& target,
            const RenderPostProcessSettings& settings);
        bool recordComposite(
            VkCommandBuffer command_buffer,
            const VulkanBloomRenderTarget& target,
            const RenderPostProcessSettings& settings);

        bool isReady() const {
            return m_downsample_pipeline != VK_NULL_HANDLE &&
                   m_upsample_pipeline != VK_NULL_HANDLE &&
                   m_composite_pipeline != VK_NULL_HANDLE &&
                   m_downsample_pipeline_layout != VK_NULL_HANDLE &&
                   m_upsample_pipeline_layout != VK_NULL_HANDLE &&
                   m_composite_pipeline_layout != VK_NULL_HANDLE &&
                   !m_downsample_descriptor_sets.empty() &&
                   m_composite_descriptor_set != VK_NULL_HANDLE;
        }

    private:
        struct SingleInputDescriptor {
            VulkanDescriptorSetAllocation allocation;
            VkDescriptorSet set = VK_NULL_HANDLE;
            VkExtent2D input_extent{};
        };

        struct DualInputDescriptor {
            VulkanDescriptorSetAllocation allocation;
            VkDescriptorSet set = VK_NULL_HANDLE;
            VkExtent2D low_input_extent{};
        };

        bool createPipelines();
        bool createPipeline(
            VulkanShaderProgramId shader_program,
            VkDescriptorSetLayout descriptor_set_layout,
            const VkPushConstantRange& push_constant_range,
            const char* debug_name,
            VkPipeline& pipeline,
            VkPipelineLayout& pipeline_layout);
        bool allocateDescriptorSets(uint32_t mip_count);
        void cleanupPipelines();
        void freeDescriptorSets();
        bool beginRenderTarget(VkCommandBuffer command_buffer, const VulkanBloomRenderTarget& target) const;
        void drawFullscreen(VkCommandBuffer command_buffer) const;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout m_single_input_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_dual_input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanPipelineCache* m_pipeline_cache = nullptr;

        std::vector<SingleInputDescriptor> m_downsample_descriptor_sets;
        std::vector<DualInputDescriptor> m_upsample_descriptor_sets;
        VulkanDescriptorSetAllocation m_composite_descriptor_allocation;
        VkDescriptorSet m_composite_descriptor_set = VK_NULL_HANDLE;

        VkPipelineLayout m_downsample_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_downsample_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_upsample_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_upsample_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_composite_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_composite_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
