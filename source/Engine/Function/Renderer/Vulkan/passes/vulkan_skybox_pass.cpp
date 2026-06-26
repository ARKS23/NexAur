#include "pch.h"
#include "vulkan_skybox_pass.h"

#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

#include <algorithm>

namespace NexAur {
    namespace {
        struct VulkanSkyboxPushConstants {
            glm::vec4 background_color_intensity{ 0.08f, 0.10f, 0.14f, 1.0f };
        };

        static_assert(sizeof(VulkanSkyboxPushConstants) <= 128, "Skybox pass push constants exceed Vulkan minimum limit.");
    } // namespace

    VulkanSkyboxPass::~VulkanSkyboxPass() {
        shutdown();
    }

    bool VulkanSkyboxPass::recreateResources(const VulkanSkyboxPassContext& context) {
        cleanupResources();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanSkyboxPass requires a valid context.");
            return false;
        }

        m_device = context.device;
        m_color_format = context.color_format;
        m_pipeline_cache = context.pipeline_cache;

        if (!createPipeline()) {
            cleanupResources();
            return false;
        }

        return true;
    }

    void VulkanSkyboxPass::cleanupResources() {
        cleanupPipeline();
        m_color_format = VK_FORMAT_UNDEFINED;
        m_pipeline_cache = nullptr;
    }

    void VulkanSkyboxPass::shutdown() {
        cleanupResources();
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanSkyboxPass::record(
        VkCommandBuffer command_buffer,
        const VulkanSkyboxRenderTarget& target,
        const VulkanDrawList& draw_list) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
            return false;
        }

        if (target.color_format != m_color_format) {
            NX_CORE_ERROR("VulkanSkyboxPass target format does not match the current pipeline.");
            return false;
        }

        if (m_pipeline == VK_NULL_HANDLE || m_pipeline_layout == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanSkyboxPass has no valid pipeline.");
            return false;
        }

        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = target.color_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = { 0, 0 };
        rendering_info.renderArea.extent = target.extent;
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;

        vkCmdBeginRendering(command_buffer, &rendering_info);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(target.extent.width);
        viewport.height = static_cast<float>(target.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = target.extent;
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        VulkanSkyboxPushConstants push_constants;
        push_constants.background_color_intensity = glm::vec4(
            glm::max(draw_list.environment_color, glm::vec3{ 0.0f }),
            std::max(0.0f, draw_list.environment_intensity));

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdPushConstants(
            command_buffer,
            m_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(VulkanSkyboxPushConstants),
            &push_constants);
        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanSkyboxPass::createPipeline() {
        if (!m_pipeline_cache) {
            NX_CORE_ERROR("VulkanSkyboxPass requires a pipeline cache.");
            return false;
        }

        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(VulkanSkyboxPushConstants);

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = "Skybox";
        desc.shader_program = VulkanShaderProgramId::Skybox;
        desc.color_format = m_color_format;
        desc.depth_format = VK_FORMAT_UNDEFINED;
        desc.vertex_layout = VulkanPipelineVertexLayout::None;
        desc.depth_test_enable = false;
        desc.depth_write_enable = false;
        desc.push_constant_ranges = { push_constant_range };

        const VulkanGraphicsPipelineState pipeline_state = m_pipeline_cache->getOrCreateGraphicsPipeline(desc);
        if (!pipeline_state.valid()) {
            return false;
        }

        m_pipeline = pipeline_state.pipeline;
        m_pipeline_layout = pipeline_state.layout;
        return true;
    }

    void VulkanSkyboxPass::cleanupPipeline() {
        m_pipeline = VK_NULL_HANDLE;
        m_pipeline_layout = VK_NULL_HANDLE;
    }
} // namespace NexAur
