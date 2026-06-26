#include "pch.h"
#include "vulkan_debug_draw_pass.h"

#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"
#include "Function/Renderer/Vulkan/resources/vulkan_debug_draw_buffer.h"

namespace NexAur {
    VulkanDebugDrawPass::~VulkanDebugDrawPass() {
        shutdown();
    }

    bool VulkanDebugDrawPass::recreateResources(const VulkanDebugDrawPassContext& context) {
        cleanupResources();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanDebugDrawPass requires a valid context.");
            return false;
        }

        m_device = context.device;
        m_color_format = context.color_format;
        m_depth_format = context.depth_format;
        m_frame_descriptor_set_layout = context.frame_descriptor_set_layout;
        m_pipeline_cache = context.pipeline_cache;

        if (!createPipeline()) {
            cleanupResources();
            return false;
        }

        return true;
    }

    void VulkanDebugDrawPass::cleanupResources() {
        cleanupPipeline();
        m_color_format = VK_FORMAT_UNDEFINED;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_frame_descriptor_set_layout = VK_NULL_HANDLE;
        m_pipeline_cache = nullptr;
    }

    void VulkanDebugDrawPass::shutdown() {
        cleanupResources();
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanDebugDrawPass::record(
        VkCommandBuffer command_buffer,
        const VulkanRenderTarget& target,
        const VulkanDebugDrawBuffer& debug_draw_buffer,
        VkDescriptorSet frame_descriptor_set) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
            return false;
        }

        if (!debug_draw_buffer.hasVertices()) {
            return true;
        }

        if (target.color_format != m_color_format || target.depth_format != m_depth_format) {
            NX_CORE_ERROR("VulkanDebugDrawPass target format does not match the current pipeline.");
            return false;
        }

        if (m_pipeline == VK_NULL_HANDLE || m_pipeline_layout == VK_NULL_HANDLE || frame_descriptor_set == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanDebugDrawPass has no valid pipeline or frame descriptor set.");
            return false;
        }

        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = target.color_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingAttachmentInfo depth_attachment{};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = target.depth_view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = { 0, 0 };
        rendering_info.renderArea.extent = target.extent;
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;
        rendering_info.pDepthAttachment = &depth_attachment;

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

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline_layout,
            0,
            1,
            &frame_descriptor_set,
            0,
            nullptr);

        const VkBuffer vertex_buffer = debug_draw_buffer.getVertexBuffer();
        const VkDeviceSize vertex_offset = 0;
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &vertex_offset);
        vkCmdDraw(command_buffer, debug_draw_buffer.getVertexCount(), 1, 0, 0);

        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanDebugDrawPass::createPipeline() {
        if (!m_pipeline_cache) {
            NX_CORE_ERROR("VulkanDebugDrawPass requires a pipeline cache.");
            return false;
        }

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = "DebugDraw";
        desc.shader_program = VulkanShaderProgramId::DebugDraw;
        desc.color_format = m_color_format;
        desc.depth_format = m_depth_format;
        desc.descriptor_set_layouts = { m_frame_descriptor_set_layout };
        desc.vertex_layout = VulkanPipelineVertexLayout::DebugPositionColor;
        desc.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        desc.depth_test_enable = true;
        desc.depth_write_enable = false;
        desc.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        desc.cull_mode = VK_CULL_MODE_NONE;

        const VulkanGraphicsPipelineState pipeline_state = m_pipeline_cache->getOrCreateGraphicsPipeline(desc);
        if (!pipeline_state.valid()) {
            return false;
        }

        m_pipeline = pipeline_state.pipeline;
        m_pipeline_layout = pipeline_state.layout;
        return true;
    }

    void VulkanDebugDrawPass::cleanupPipeline() {
        m_pipeline = VK_NULL_HANDLE;
        m_pipeline_layout = VK_NULL_HANDLE;
    }
} // namespace NexAur
