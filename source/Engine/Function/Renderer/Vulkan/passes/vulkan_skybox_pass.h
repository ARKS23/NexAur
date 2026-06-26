#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"

namespace NexAur {
    struct VulkanDrawList;
    class VulkanPipelineCache;

    struct VulkanSkyboxPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   pipeline_cache != nullptr;
        }
    };

    struct VulkanSkyboxRenderTarget {
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

    class NEXAUR_API VulkanSkyboxPass {
    public:
        VulkanSkyboxPass() = default;
        ~VulkanSkyboxPass();

        VulkanSkyboxPass(const VulkanSkyboxPass&) = delete;
        VulkanSkyboxPass& operator=(const VulkanSkyboxPass&) = delete;

        bool recreateResources(const VulkanSkyboxPassContext& context);
        void cleanupResources();
        void shutdown();

        bool record(
            VkCommandBuffer command_buffer,
            const VulkanSkyboxRenderTarget& target,
            const VulkanDrawList& draw_list);

    private:
        bool createPipeline();
        void cleanupPipeline();

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VulkanPipelineCache* m_pipeline_cache = nullptr;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
