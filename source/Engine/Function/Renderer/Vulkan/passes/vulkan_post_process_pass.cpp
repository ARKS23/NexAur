#include "pch.h"
#include "vulkan_post_process_pass.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

namespace NexAur {
    VulkanPostProcessPass::~VulkanPostProcessPass() {
        shutdown();
    }

    bool VulkanPostProcessPass::recreateResources(const VulkanPostProcessPassContext& context) {
        cleanupResources();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanPostProcessPass requires a valid context.");
            return false;
        }

        m_device = context.device;
        m_output_color_format = context.output_color_format;
        m_input_descriptor_set_layout = context.input_descriptor_set_layout;
        m_descriptor_allocator = context.descriptor_allocator;
        m_pipeline_cache = context.pipeline_cache;

        if (!allocateDescriptorSet() || !createPipeline()) {
            cleanupResources();
            return false;
        }

        return true;
    }

    void VulkanPostProcessPass::cleanupResources() {
        cleanupPipeline();
        freeDescriptorSet();
        m_output_color_format = VK_FORMAT_UNDEFINED;
        m_input_descriptor_set_layout = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_pipeline_cache = nullptr;
    }

    void VulkanPostProcessPass::shutdown() {
        cleanupResources();
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanPostProcessPass::updateInput(const VulkanPostProcessInput& input) {
        if (m_device == VK_NULL_HANDLE || m_input_descriptor_set == VK_NULL_HANDLE || !input.valid()) {
            return false;
        }

        VkDescriptorImageInfo color_info{};
        color_info.imageView = input.color_view;
        color_info.imageLayout = input.layout;

        VkDescriptorImageInfo sampler_info{};
        sampler_info.sampler = input.sampler;

        VulkanDescriptorWriter writer;
        writer
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, color_info)
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLER, sampler_info)
            .update(m_device, m_input_descriptor_set);
        return true;
    }

    bool VulkanPostProcessPass::record(
        VkCommandBuffer command_buffer,
        const VulkanPostProcessRenderTarget& target) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
            return false;
        }

        if (target.color_format != m_output_color_format) {
            NX_CORE_ERROR("VulkanPostProcessPass target format does not match the current pipeline.");
            return false;
        }

        if (!isReady()) {
            NX_CORE_ERROR("VulkanPostProcessPass has no valid pipeline or descriptor set.");
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

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline_layout,
            0,
            1,
            &m_input_descriptor_set,
            0,
            nullptr);
        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanPostProcessPass::allocateDescriptorSet() {
        if (!m_descriptor_allocator || m_input_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        m_input_descriptor_allocation = m_descriptor_allocator->allocate(m_input_descriptor_set_layout);
        if (!m_input_descriptor_allocation.valid()) {
            NX_CORE_ERROR("VulkanPostProcessPass failed to allocate input descriptor set.");
            return false;
        }

        m_input_descriptor_set = m_input_descriptor_allocation.set;
        return true;
    }

    bool VulkanPostProcessPass::createPipeline() {
        if (!m_pipeline_cache) {
            NX_CORE_ERROR("VulkanPostProcessPass requires a pipeline cache.");
            return false;
        }

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = "PostProcess";
        desc.shader_program = VulkanShaderProgramId::PostProcess;
        desc.color_format = m_output_color_format;
        desc.depth_format = VK_FORMAT_UNDEFINED;
        desc.descriptor_set_layouts = { m_input_descriptor_set_layout };
        desc.vertex_layout = VulkanPipelineVertexLayout::None;
        desc.depth_test_enable = false;
        desc.depth_write_enable = false;
        desc.cull_mode = VK_CULL_MODE_NONE;

        const VulkanGraphicsPipelineState pipeline_state = m_pipeline_cache->getOrCreateGraphicsPipeline(desc);
        if (!pipeline_state.valid()) {
            return false;
        }

        m_pipeline = pipeline_state.pipeline;
        m_pipeline_layout = pipeline_state.layout;
        return true;
    }

    void VulkanPostProcessPass::cleanupPipeline() {
        m_pipeline = VK_NULL_HANDLE;
        m_pipeline_layout = VK_NULL_HANDLE;
    }

    void VulkanPostProcessPass::freeDescriptorSet() {
        if (m_descriptor_allocator && m_input_descriptor_allocation.valid()) {
            m_descriptor_allocator->free(m_input_descriptor_allocation);
        }

        m_input_descriptor_allocation = {};
        m_input_descriptor_set = VK_NULL_HANDLE;
    }
} // namespace NexAur
