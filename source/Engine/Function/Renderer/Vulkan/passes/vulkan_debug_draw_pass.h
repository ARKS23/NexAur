#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"

namespace NexAur {
    class VulkanDebugDrawBuffer;
    class VulkanPipelineCache;

    struct VulkanDebugDrawPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout frame_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   depth_format != VK_FORMAT_UNDEFINED &&
                   frame_descriptor_set_layout != VK_NULL_HANDLE &&
                   pipeline_cache != nullptr;
        }
    };

    class NEXAUR_API VulkanDebugDrawPass {
    public:
        VulkanDebugDrawPass() = default;
        ~VulkanDebugDrawPass();

        VulkanDebugDrawPass(const VulkanDebugDrawPass&) = delete;
        VulkanDebugDrawPass& operator=(const VulkanDebugDrawPass&) = delete;

        bool recreateResources(const VulkanDebugDrawPassContext& context);
        void cleanupResources();
        void shutdown();

        bool record(
            VkCommandBuffer command_buffer,
            const VulkanRenderTarget& target,
            const VulkanDebugDrawBuffer& debug_draw_buffer,
            VkDescriptorSet frame_descriptor_set);

    private:
        bool createPipeline();
        void cleanupPipeline();

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout m_frame_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanPipelineCache* m_pipeline_cache = nullptr;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
