#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"
#include "Function/Renderer/Vulkan/targets/vulkan_smaa_target.h"

namespace NexAur {
    class VulkanPipelineCache;

    struct VulkanSmaaPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat source_color_format = VK_FORMAT_UNDEFINED;
        VkFormat mask_color_format = VK_FORMAT_UNDEFINED;
        VkFormat output_color_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout single_input_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout dual_input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* descriptor_allocator = nullptr;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   source_color_format != VK_FORMAT_UNDEFINED &&
                   mask_color_format != VK_FORMAT_UNDEFINED &&
                   output_color_format != VK_FORMAT_UNDEFINED &&
                   single_input_descriptor_set_layout != VK_NULL_HANDLE &&
                   dual_input_descriptor_set_layout != VK_NULL_HANDLE &&
                   descriptor_allocator != nullptr &&
                   pipeline_cache != nullptr;
        }
    };

    struct VulkanSmaaInput {
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

    class NEXAUR_API VulkanSmaaPass {
    public:
        VulkanSmaaPass() = default;
        ~VulkanSmaaPass();

        VulkanSmaaPass(const VulkanSmaaPass&) = delete;
        VulkanSmaaPass& operator=(const VulkanSmaaPass&) = delete;

        bool recreateResources(const VulkanSmaaPassContext& context);
        void cleanupResources();
        void shutdown();

        bool updateInputs(
            const VulkanSmaaInput& source_input,
            const VulkanSmaaInput& edge_input,
            const VulkanSmaaInput& blend_input);
        bool recordEdge(
            VkCommandBuffer command_buffer,
            const VulkanSmaaRenderTarget& target,
            const RenderAntiAliasingSettings& settings);
        bool recordBlend(
            VkCommandBuffer command_buffer,
            const VulkanSmaaRenderTarget& target,
            const RenderAntiAliasingSettings& settings);
        bool recordNeighborhood(
            VkCommandBuffer command_buffer,
            const VulkanSmaaRenderTarget& target,
            const RenderAntiAliasingSettings& settings);
        bool recordDebugResolve(
            VkCommandBuffer command_buffer,
            const VulkanSmaaRenderTarget& target,
            const VulkanSmaaInput& input);

        bool isReady() const {
            return m_edge_pipeline != VK_NULL_HANDLE &&
                   m_blend_pipeline != VK_NULL_HANDLE &&
                   m_neighborhood_pipeline != VK_NULL_HANDLE &&
                   m_edge_pipeline_layout != VK_NULL_HANDLE &&
                   m_blend_pipeline_layout != VK_NULL_HANDLE &&
                   m_neighborhood_pipeline_layout != VK_NULL_HANDLE &&
                   m_edge_descriptor_set != VK_NULL_HANDLE &&
                   m_blend_descriptor_set != VK_NULL_HANDLE &&
                   m_neighborhood_descriptor_set != VK_NULL_HANDLE &&
                   m_debug_descriptor_set != VK_NULL_HANDLE;
        }

    private:
        bool createPipelines();
        bool createPipeline(
            VulkanShaderProgramId shader_program,
            VkDescriptorSetLayout descriptor_set_layout,
            const VkPushConstantRange& push_constant_range,
            VkFormat color_format,
            const char* debug_name,
            VkPipeline& pipeline,
            VkPipelineLayout& pipeline_layout);
        bool allocateDescriptorSets();
        void cleanupPipelines();
        void freeDescriptorSets();
        bool updateDebugInput(const VulkanSmaaInput& input);
        bool beginRenderTarget(VkCommandBuffer command_buffer, const VulkanSmaaRenderTarget& target) const;
        void drawFullscreen(VkCommandBuffer command_buffer) const;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_source_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_mask_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_output_color_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout m_single_input_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_dual_input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanPipelineCache* m_pipeline_cache = nullptr;

        VulkanDescriptorSetAllocation m_edge_descriptor_allocation;
        VkDescriptorSet m_edge_descriptor_set = VK_NULL_HANDLE;
        VulkanDescriptorSetAllocation m_blend_descriptor_allocation;
        VkDescriptorSet m_blend_descriptor_set = VK_NULL_HANDLE;
        VulkanDescriptorSetAllocation m_neighborhood_descriptor_allocation;
        VkDescriptorSet m_neighborhood_descriptor_set = VK_NULL_HANDLE;
        VulkanDescriptorSetAllocation m_debug_descriptor_allocation;
        VkDescriptorSet m_debug_descriptor_set = VK_NULL_HANDLE;

        VkExtent2D m_source_extent{};
        VkExtent2D m_edge_extent{};

        VkPipelineLayout m_edge_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_edge_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_blend_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_blend_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_neighborhood_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_neighborhood_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
