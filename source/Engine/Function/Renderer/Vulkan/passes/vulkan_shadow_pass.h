#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"

namespace NexAur {
    struct VulkanDrawList;
    class VulkanPipelineCache;

    struct VulkanShadowPassContext {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        VulkanPipelineCache* pipeline_cache = nullptr;

        bool valid() const {
            return device != VK_NULL_HANDLE &&
                   depth_format != VK_FORMAT_UNDEFINED &&
                   pipeline_cache != nullptr;
        }
    };

    class NEXAUR_API VulkanShadowPass {
    public:
        VulkanShadowPass() = default;
        ~VulkanShadowPass();

        VulkanShadowPass(const VulkanShadowPass&) = delete;
        VulkanShadowPass& operator=(const VulkanShadowPass&) = delete;

        bool init(const VulkanShadowPassContext& context);
        void shutdown();

        bool record(
            VkCommandBuffer command_buffer,
            const VulkanDepthRenderTarget& target,
            const VulkanDrawList& draw_list,
            const glm::mat4& light_view_projection);

    private:
        bool createPipeline();
        void cleanupPipeline();

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VulkanPipelineCache* m_pipeline_cache = nullptr;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
} // namespace NexAur
