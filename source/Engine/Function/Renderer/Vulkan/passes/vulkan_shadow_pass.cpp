#include "pch.h"
#include "vulkan_shadow_pass.h"

#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"

namespace NexAur {
    namespace {
        struct VulkanShadowPushConstants {
            glm::mat4 light_view_projection{ 1.0f };
            glm::mat4 model{ 1.0f };
        };

        static_assert(sizeof(VulkanShadowPushConstants) <= 128, "Shadow pass push constants exceed Vulkan minimum limit.");
    } // namespace

    VulkanShadowPass::~VulkanShadowPass() {
        shutdown();
    }

    bool VulkanShadowPass::init(const VulkanShadowPassContext& context) {
        shutdown();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanShadowPass requires a valid context.");
            return false;
        }

        m_device = context.device;
        m_depth_format = context.depth_format;
        m_pipeline_cache = context.pipeline_cache;

        if (!createPipeline()) {
            shutdown();
            return false;
        }

        return true;
    }

    void VulkanShadowPass::shutdown() {
        cleanupPipeline();
        m_device = VK_NULL_HANDLE;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_pipeline_cache = nullptr;
    }

    bool VulkanShadowPass::record(
        VkCommandBuffer command_buffer,
        const VulkanDepthRenderTarget& target,
        const VulkanDrawList& draw_list,
        const glm::mat4& light_view_projection) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
            return false;
        }

        if (target.depth_format != m_depth_format) {
            NX_CORE_ERROR("VulkanShadowPass target format does not match the current pipeline.");
            return false;
        }

        if (m_pipeline == VK_NULL_HANDLE || m_pipeline_layout == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanShadowPass has no valid pipeline.");
            return false;
        }

        VkClearValue depth_clear{};
        depth_clear.depthStencil.depth = 1.0f;
        depth_clear.depthStencil.stencil = 0;

        VkRenderingAttachmentInfo depth_attachment{};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = target.depth_view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.clearValue = depth_clear;

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = { 0, 0 };
        rendering_info.renderArea.extent = target.extent;
        rendering_info.layerCount = 1;
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

        for (const VulkanMeshDrawItem& item : draw_list.opaque_items) {
            if (!item.mesh || !item.mesh->isReady()) {
                continue;
            }

            const VkBuffer vertex_buffer = item.mesh->getVertexBuffer();
            const VkDeviceSize vertex_offset = 0;
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &vertex_offset);
            vkCmdBindIndexBuffer(command_buffer, item.mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            VulkanShadowPushConstants push_constants;
            push_constants.light_view_projection = light_view_projection;
            push_constants.model = item.transform;
            vkCmdPushConstants(
                command_buffer,
                m_pipeline_layout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(VulkanShadowPushConstants),
                &push_constants);

            vkCmdDrawIndexed(command_buffer, item.mesh->getIndexCount(), 1, 0, 0, 0);
        }

        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanShadowPass::createPipeline() {
        if (!m_pipeline_cache) {
            NX_CORE_ERROR("VulkanShadowPass requires a pipeline cache.");
            return false;
        }

        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(VulkanShadowPushConstants);

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = "ShadowDepth";
        desc.shader_program = VulkanShaderProgramId::ShadowDepth;
        desc.color_format = VK_FORMAT_UNDEFINED;
        desc.depth_format = m_depth_format;
        desc.push_constant_ranges = { push_constant_range };
        desc.depth_test_enable = true;
        desc.depth_write_enable = true;
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

    void VulkanShadowPass::cleanupPipeline() {
        m_pipeline = VK_NULL_HANDLE;
        m_pipeline_layout = VK_NULL_HANDLE;
    }
} // namespace NexAur
