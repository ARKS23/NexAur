#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/data/render_view.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"
#include "Function/Renderer/Vulkan/targets/vulkan_ao_target.h"

namespace NexAur {
    class VulkanPipelineCache;

    struct VulkanAoPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* descriptor_allocator = nullptr;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   input_descriptor_set_layout != VK_NULL_HANDLE &&
                   descriptor_allocator != nullptr &&
                   pipeline_cache != nullptr;
        }
    };

    struct VulkanAoInput {
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

    class NEXAUR_API VulkanAoPass {
    public:
        VulkanAoPass() = default;
        ~VulkanAoPass();

        VulkanAoPass(const VulkanAoPass&) = delete;
        VulkanAoPass& operator=(const VulkanAoPass&) = delete;

        bool recreateResources(const VulkanAoPassContext& context);
        void cleanupResources();
        void shutdown();

        bool updateInputs(const VulkanAoInput& depth_input, const VulkanAoInput& raw_ao_input);
        bool recordSsao(
            VkCommandBuffer command_buffer,
            const VulkanAoRenderTarget& target,
            const RenderView& view,
            const RenderAoSettings& settings);
        bool recordBlur(
            VkCommandBuffer command_buffer,
            const VulkanAoRenderTarget& target,
            const RenderAoSettings& settings);

        bool isReady() const {
            return m_ssao_pipeline != VK_NULL_HANDLE &&
                   m_blur_pipeline != VK_NULL_HANDLE &&
                   m_ssao_pipeline_layout != VK_NULL_HANDLE &&
                   m_blur_pipeline_layout != VK_NULL_HANDLE &&
                   m_depth_descriptor_set != VK_NULL_HANDLE &&
                   m_raw_descriptor_set != VK_NULL_HANDLE;
        }

    private:
        bool createPipelines();
        bool createPipeline(
            VulkanShaderProgramId shader_program,
            const VkPushConstantRange& push_constant_range,
            const char* debug_name,
            VkPipeline& pipeline,
            VkPipelineLayout& pipeline_layout);
        bool allocateDescriptorSets();
        void cleanupPipelines();
        void freeDescriptorSets();
        bool beginRenderTarget(VkCommandBuffer command_buffer, const VulkanAoRenderTarget& target) const;
        void drawFullscreen(VkCommandBuffer command_buffer) const;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout m_input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanPipelineCache* m_pipeline_cache = nullptr;

        VulkanDescriptorSetAllocation m_depth_descriptor_allocation;
        VkDescriptorSet m_depth_descriptor_set = VK_NULL_HANDLE;
        VulkanDescriptorSetAllocation m_raw_descriptor_allocation;
        VkDescriptorSet m_raw_descriptor_set = VK_NULL_HANDLE;
        VkExtent2D m_depth_extent{};
        VkExtent2D m_raw_extent{};

        VkPipelineLayout m_ssao_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_ssao_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_blur_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_blur_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
