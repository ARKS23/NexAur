#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/data/render_view.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"
#include "Function/Renderer/Vulkan/targets/vulkan_ssr_target.h"

namespace NexAur {
    class VulkanPipelineCache;

    struct VulkanSsrPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat reflection_color_format = VK_FORMAT_UNDEFINED;
        VkFormat hit_mask_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* descriptor_allocator = nullptr;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   reflection_color_format != VK_FORMAT_UNDEFINED &&
                   hit_mask_format != VK_FORMAT_UNDEFINED &&
                   input_descriptor_set_layout != VK_NULL_HANDLE &&
                   descriptor_allocator != nullptr &&
                   pipeline_cache != nullptr;
        }
    };

    struct VulkanSsrInput {
        VkImageView scene_color_view = VK_NULL_HANDLE;
        VkImageView scene_depth_view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkExtent2D extent{};
        VkImageLayout scene_color_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkImageLayout scene_depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        bool valid() const {
            return scene_color_view != VK_NULL_HANDLE &&
                   scene_depth_view != VK_NULL_HANDLE &&
                   sampler != VK_NULL_HANDLE &&
                   extent.width > 0 &&
                   extent.height > 0 &&
                   scene_color_layout != VK_IMAGE_LAYOUT_UNDEFINED &&
                   scene_depth_layout != VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };

    enum class VulkanSsrOutputMode : uint32_t {
        RawReflection = 0,
        HitMask = 1
    };

    class NEXAUR_API VulkanSsrPass {
    public:
        VulkanSsrPass() = default;
        ~VulkanSsrPass();

        VulkanSsrPass(const VulkanSsrPass&) = delete;
        VulkanSsrPass& operator=(const VulkanSsrPass&) = delete;

        bool recreateResources(const VulkanSsrPassContext& context);
        void cleanupResources();
        void shutdown();

        bool updateInput(const VulkanSsrInput& input);
        bool recordTrace(
            VkCommandBuffer command_buffer,
            const VulkanSsrRenderTarget& target,
            const RenderView& view,
            const RenderSsrSettings& settings,
            VulkanSsrOutputMode output_mode);

        bool isReady() const {
            return m_reflection_pipeline != VK_NULL_HANDLE &&
                   m_hit_mask_pipeline != VK_NULL_HANDLE &&
                   m_reflection_pipeline_layout != VK_NULL_HANDLE &&
                   m_hit_mask_pipeline_layout != VK_NULL_HANDLE &&
                   m_input_descriptor_set != VK_NULL_HANDLE;
        }

    private:
        bool createPipelines();
        bool createPipeline(
            VkFormat color_format,
            const char* debug_name,
            VkPipeline& pipeline,
            VkPipelineLayout& pipeline_layout);
        bool allocateDescriptorSet();
        void cleanupPipelines();
        void freeDescriptorSet();
        bool beginRenderTarget(VkCommandBuffer command_buffer, const VulkanSsrRenderTarget& target) const;
        void drawFullscreen(VkCommandBuffer command_buffer) const;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_reflection_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_hit_mask_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout m_input_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanPipelineCache* m_pipeline_cache = nullptr;

        VulkanDescriptorSetAllocation m_input_descriptor_allocation;
        VkDescriptorSet m_input_descriptor_set = VK_NULL_HANDLE;
        VkExtent2D m_input_extent{};

        VkPipelineLayout m_reflection_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_reflection_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_hit_mask_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_hit_mask_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
