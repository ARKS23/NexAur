#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/RendererV2/vulkan_render_target.h"

namespace NexAur {
    struct VulkanDrawList;

    struct VulkanForwardPassSwapchainContext {
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
        std::vector<VkImage> color_images;

        bool valid() const {
            return physical_device != VK_NULL_HANDLE &&
                   device != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0 &&
                   !color_images.empty();
        }
    };

    class NEXAUR_API VulkanForwardPass {
    public:
        VulkanForwardPass() = default;
        ~VulkanForwardPass();

        VulkanForwardPass(const VulkanForwardPass&) = delete;
        VulkanForwardPass& operator=(const VulkanForwardPass&) = delete;

        bool recreateSwapchainResources(const VulkanForwardPassSwapchainContext& context);
        void cleanupSwapchainResources();
        void shutdown();

        bool record(VkCommandBuffer command_buffer, uint32_t image_index, const VulkanDrawList& draw_list);
        bool record(VkCommandBuffer command_buffer, const VulkanRenderTarget& target, const VulkanDrawList& draw_list);
        VkImageView getSwapchainColorImageView(uint32_t image_index) const;

    private:
        bool createImageViews(const VulkanForwardPassSwapchainContext& context);
        bool createDepthResources(const VulkanForwardPassSwapchainContext& context);
        bool createPipeline();
        void cleanupDepthResources();
        void cleanupPipeline();

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};
        std::vector<VkImageView> m_color_image_views;
        VkImage m_depth_image = VK_NULL_HANDLE;
        VkDeviceMemory m_depth_memory = VK_NULL_HANDLE;
        VkImageView m_depth_image_view = VK_NULL_HANDLE;
        VkImageLayout m_depth_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
