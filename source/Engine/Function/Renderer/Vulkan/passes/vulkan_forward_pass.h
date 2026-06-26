#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"

namespace NexAur {
    struct VulkanDrawList;
    class VulkanPipelineCache;

    struct VulkanForwardPassSwapchainContext {
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
        std::vector<VkImage> color_images;
        VkDescriptorSetLayout frame_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout material_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return physical_device != VK_NULL_HANDLE &&
                   device != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0 &&
                   !color_images.empty() &&
                   frame_descriptor_set_layout != VK_NULL_HANDLE &&
                   material_descriptor_set_layout != VK_NULL_HANDLE &&
                   pipeline_cache != nullptr;
        }
    };

    struct VulkanForwardPassRenderOptions {
        VkAttachmentLoadOp color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentLoadOp depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkClearValue color_clear_value{};
        VkClearValue depth_clear_value{};
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

        bool record(
            VkCommandBuffer command_buffer,
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            VkDescriptorSet frame_descriptor_set);
        bool record(
            VkCommandBuffer command_buffer,
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            VkDescriptorSet frame_descriptor_set,
            const VulkanForwardPassRenderOptions& options);
        bool record(
            VkCommandBuffer command_buffer,
            const VulkanRenderTarget& target,
            const VulkanDrawList& draw_list,
            VkDescriptorSet frame_descriptor_set);
        bool record(
            VkCommandBuffer command_buffer,
            const VulkanRenderTarget& target,
            const VulkanDrawList& draw_list,
            VkDescriptorSet frame_descriptor_set,
            const VulkanForwardPassRenderOptions& options);
        VkImageView getSwapchainColorImageView(uint32_t image_index) const;
        VulkanRenderTarget getSwapchainRenderTarget(uint32_t image_index) const;
        VkImage getDepthImage() const { return m_depth_image; }
        VkImageLayout getDepthImageLayout() const { return m_depth_image_layout; }
        void setDepthImageLayout(VkImageLayout layout) { m_depth_image_layout = layout; }
        VkFormat getDepthFormat() const { return m_depth_format; }

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
        VkDescriptorSetLayout m_frame_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_material_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanPipelineCache* m_pipeline_cache = nullptr;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
