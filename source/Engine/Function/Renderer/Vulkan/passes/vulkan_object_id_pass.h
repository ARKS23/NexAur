#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"

namespace NexAur {
    struct VulkanDrawList;

    struct VulkanObjectIdPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat object_id_format = VK_FORMAT_UNDEFINED;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   object_id_format != VK_FORMAT_UNDEFINED &&
                   depth_format != VK_FORMAT_UNDEFINED;
        }
    };

    class NEXAUR_API VulkanObjectIdPass {
    public:
        VulkanObjectIdPass() = default;
        ~VulkanObjectIdPass();

        VulkanObjectIdPass(const VulkanObjectIdPass&) = delete;
        VulkanObjectIdPass& operator=(const VulkanObjectIdPass&) = delete;

        bool init(const VulkanObjectIdPassContext& context);
        void shutdown();

        bool record(VkCommandBuffer command_buffer, const VulkanRenderTarget& target, const VulkanDrawList& draw_list);

    private:
        bool createPipeline();
        void cleanupPipeline();

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_object_id_format = VK_FORMAT_UNDEFINED;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
