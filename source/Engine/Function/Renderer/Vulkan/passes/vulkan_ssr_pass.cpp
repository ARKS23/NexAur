#include "pch.h"
#include "vulkan_ssr_pass.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

namespace NexAur {
    namespace {
        struct SsrTracePushConstants {
            glm::mat4 inverse_projection{ 1.0f };
            glm::vec4 trace_params{ 18.0f, 0.18f, 1.0f, 1.0f };
            glm::vec4 texture_params{ 1.0f, 1.0f, 32.0f, 0.12f };
            glm::vec4 output_params{ 0.0f, 0.65f, 0.0f, 0.0f };
        };

        static_assert(sizeof(SsrTracePushConstants) <= 128, "SSR trace push constants exceed Vulkan minimum limit.");

        VkDescriptorImageInfo sampledImageInfo(VkImageView view, VkImageLayout layout) {
            VkDescriptorImageInfo info{};
            info.imageView = view;
            info.imageLayout = layout;
            return info;
        }

        VkDescriptorImageInfo samplerInfo(VkSampler sampler) {
            VkDescriptorImageInfo info{};
            info.sampler = sampler;
            return info;
        }

        float safeTexelSize(uint32_t size) {
            return size > 0 ? 1.0f / static_cast<float>(size) : 1.0f;
        }

        float sanitizeRange(float value, float fallback, float minimum, float maximum) {
            if (!std::isfinite(value)) {
                return fallback;
            }
            return std::clamp(value, minimum, maximum);
        }
    } // namespace

    VulkanSsrPass::~VulkanSsrPass() {
        shutdown();
    }

    bool VulkanSsrPass::recreateResources(const VulkanSsrPassContext& context) {
        cleanupResources();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanSsrPass requires a valid context.");
            return false;
        }

        m_device = context.device;
        m_reflection_color_format = context.reflection_color_format;
        m_hit_mask_format = context.hit_mask_format;
        m_input_descriptor_set_layout = context.input_descriptor_set_layout;
        m_descriptor_allocator = context.descriptor_allocator;
        m_pipeline_cache = context.pipeline_cache;

        if (!createPipelines() || !allocateDescriptorSet()) {
            cleanupResources();
            return false;
        }

        return true;
    }

    void VulkanSsrPass::cleanupResources() {
        cleanupPipelines();
        freeDescriptorSet();
        m_reflection_color_format = VK_FORMAT_UNDEFINED;
        m_hit_mask_format = VK_FORMAT_UNDEFINED;
        m_input_descriptor_set_layout = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_pipeline_cache = nullptr;
        m_input_extent = {};
    }

    void VulkanSsrPass::shutdown() {
        cleanupResources();
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanSsrPass::updateInput(const VulkanSsrInput& input) {
        if (m_device == VK_NULL_HANDLE || m_input_descriptor_set == VK_NULL_HANDLE || !input.valid()) {
            return false;
        }

        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(input.scene_color_view, input.scene_color_layout))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(input.scene_depth_view, input.scene_depth_layout))
            .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(input.sampler))
            .update(m_device, m_input_descriptor_set);

        m_input_extent = input.extent;
        return true;
    }

    bool VulkanSsrPass::recordTrace(
        VkCommandBuffer command_buffer,
        const VulkanSsrRenderTarget& target,
        const RenderView& view,
        const RenderSsrSettings& settings,
        VulkanSsrOutputMode output_mode) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || !isReady()) {
            return false;
        }

        const bool write_hit_mask = output_mode == VulkanSsrOutputMode::HitMask;
        const VkFormat expected_format = write_hit_mask ? m_hit_mask_format : m_reflection_color_format;
        VkPipeline pipeline = write_hit_mask ? m_hit_mask_pipeline : m_reflection_pipeline;
        VkPipelineLayout pipeline_layout = write_hit_mask ? m_hit_mask_pipeline_layout : m_reflection_pipeline_layout;
        if (target.color_format != expected_format ||
            pipeline == VK_NULL_HANDLE ||
            pipeline_layout == VK_NULL_HANDLE ||
            !beginRenderTarget(command_buffer, target)) {
            return false;
        }

        SsrTracePushConstants constants;
        constants.inverse_projection = view.inverse_projection_matrix;
        constants.trace_params = glm::vec4(
            sanitizeRange(settings.max_distance, 18.0f, 0.25f, 120.0f),
            sanitizeRange(settings.thickness, 0.18f, 0.001f, 2.0f),
            sanitizeRange(settings.stride, 1.0f, 0.1f, 8.0f),
            sanitizeRange(settings.intensity, 1.0f, 0.0f, 4.0f));
        constants.texture_params = glm::vec4(
            safeTexelSize(m_input_extent.width),
            safeTexelSize(m_input_extent.height),
            static_cast<float>(std::clamp(settings.max_steps, 1u, 96u)),
            sanitizeRange(settings.edge_fade, 0.12f, 0.0f, 0.5f));
        constants.output_params = glm::vec4(
            write_hit_mask ? 1.0f : 0.0f,
            sanitizeRange(settings.roughness_fade, 0.65f, 0.0f, 1.0f),
            0.0f,
            0.0f);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout,
            0,
            1,
            &m_input_descriptor_set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SsrTracePushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanSsrPass::createPipelines() {
        return createPipeline(
                   m_reflection_color_format,
                   "SSRTraceRawReflection",
                   m_reflection_pipeline,
                   m_reflection_pipeline_layout) &&
               createPipeline(
                   m_hit_mask_format,
                   "SSRTraceHitMask",
                   m_hit_mask_pipeline,
                   m_hit_mask_pipeline_layout);
    }

    bool VulkanSsrPass::createPipeline(
        VkFormat color_format,
        const char* debug_name,
        VkPipeline& pipeline,
        VkPipelineLayout& pipeline_layout) {
        if (!m_pipeline_cache || m_input_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = debug_name;
        desc.shader_program = VulkanShaderProgramId::SsrTrace;
        desc.color_format = color_format;
        desc.depth_format = VK_FORMAT_UNDEFINED;
        desc.descriptor_set_layouts = { m_input_descriptor_set_layout };
        desc.vertex_layout = VulkanPipelineVertexLayout::None;
        desc.depth_test_enable = false;
        desc.depth_write_enable = false;
        desc.cull_mode = VK_CULL_MODE_NONE;

        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(SsrTracePushConstants);
        desc.push_constant_ranges = { push_constant_range };

        const VulkanGraphicsPipelineState pipeline_state = m_pipeline_cache->getOrCreateGraphicsPipeline(desc);
        if (!pipeline_state.valid()) {
            return false;
        }

        pipeline = pipeline_state.pipeline;
        pipeline_layout = pipeline_state.layout;
        return true;
    }

    bool VulkanSsrPass::allocateDescriptorSet() {
        if (!m_descriptor_allocator || m_input_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        m_input_descriptor_allocation = m_descriptor_allocator->allocate(m_input_descriptor_set_layout);
        if (!m_input_descriptor_allocation.valid()) {
            NX_CORE_ERROR("VulkanSsrPass failed to allocate input descriptor set.");
            return false;
        }

        m_input_descriptor_set = m_input_descriptor_allocation.set;
        return true;
    }

    void VulkanSsrPass::cleanupPipelines() {
        m_reflection_pipeline = VK_NULL_HANDLE;
        m_reflection_pipeline_layout = VK_NULL_HANDLE;
        m_hit_mask_pipeline = VK_NULL_HANDLE;
        m_hit_mask_pipeline_layout = VK_NULL_HANDLE;
    }

    void VulkanSsrPass::freeDescriptorSet() {
        if (m_descriptor_allocator && m_input_descriptor_allocation.valid()) {
            m_descriptor_allocator->free(m_input_descriptor_allocation);
        }

        m_input_descriptor_allocation = {};
        m_input_descriptor_set = VK_NULL_HANDLE;
    }

    bool VulkanSsrPass::beginRenderTarget(VkCommandBuffer command_buffer, const VulkanSsrRenderTarget& target) const {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
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
        return true;
    }

    void VulkanSsrPass::drawFullscreen(VkCommandBuffer command_buffer) const {
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
    }
} // namespace NexAur
