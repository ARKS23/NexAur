#include "pch.h"
#include "vulkan_smaa_pass.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace NexAur {
    namespace {
        struct SmaaEdgePushConstants {
            float source_texel_width = 1.0f;
            float source_texel_height = 1.0f;
            float edge_threshold = 0.08f;
            float contrast_factor = 2.0f;
        };

        struct SmaaBlendPushConstants {
            float edge_texel_width = 1.0f;
            float edge_texel_height = 1.0f;
            uint32_t max_search_steps = 8;
            float padding0 = 0.0f;
        };

        struct SmaaNeighborhoodPushConstants {
            float source_texel_width = 1.0f;
            float source_texel_height = 1.0f;
            float blend_strength = 0.85f;
            uint32_t debug_mode = 0;
        };

        static_assert(sizeof(SmaaEdgePushConstants) <= 128, "SMAA edge push constants exceed Vulkan minimum limit.");
        static_assert(sizeof(SmaaBlendPushConstants) <= 128, "SMAA blend push constants exceed Vulkan minimum limit.");
        static_assert(sizeof(SmaaNeighborhoodPushConstants) <= 128, "SMAA neighborhood push constants exceed Vulkan minimum limit.");

        VkDescriptorImageInfo sampledImageInfo(const VulkanSmaaInput& input) {
            VkDescriptorImageInfo info{};
            info.imageView = input.view;
            info.imageLayout = input.layout;
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

    VulkanSmaaPass::~VulkanSmaaPass() {
        shutdown();
    }

    bool VulkanSmaaPass::recreateResources(const VulkanSmaaPassContext& context) {
        cleanupResources();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanSmaaPass requires a valid context.");
            return false;
        }

        m_device = context.device;
        m_source_color_format = context.source_color_format;
        m_mask_color_format = context.mask_color_format;
        m_output_color_format = context.output_color_format;
        m_single_input_descriptor_set_layout = context.single_input_descriptor_set_layout;
        m_dual_input_descriptor_set_layout = context.dual_input_descriptor_set_layout;
        m_descriptor_allocator = context.descriptor_allocator;
        m_pipeline_cache = context.pipeline_cache;

        if (!createPipelines() || !allocateDescriptorSets()) {
            cleanupResources();
            return false;
        }

        return true;
    }

    void VulkanSmaaPass::cleanupResources() {
        cleanupPipelines();
        freeDescriptorSets();
        m_source_color_format = VK_FORMAT_UNDEFINED;
        m_mask_color_format = VK_FORMAT_UNDEFINED;
        m_output_color_format = VK_FORMAT_UNDEFINED;
        m_single_input_descriptor_set_layout = VK_NULL_HANDLE;
        m_dual_input_descriptor_set_layout = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_pipeline_cache = nullptr;
        m_source_extent = {};
        m_edge_extent = {};
    }

    void VulkanSmaaPass::shutdown() {
        cleanupResources();
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanSmaaPass::updateInputs(
        const VulkanSmaaInput& source_input,
        const VulkanSmaaInput& edge_input,
        const VulkanSmaaInput& blend_input) {
        if (m_device == VK_NULL_HANDLE ||
            m_edge_descriptor_set == VK_NULL_HANDLE ||
            m_blend_descriptor_set == VK_NULL_HANDLE ||
            m_neighborhood_descriptor_set == VK_NULL_HANDLE ||
            !source_input.valid() ||
            !edge_input.valid() ||
            !blend_input.valid()) {
            return false;
        }

        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(source_input))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(source_input.sampler))
            .update(m_device, m_edge_descriptor_set);

        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(edge_input))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(edge_input.sampler))
            .update(m_device, m_blend_descriptor_set);

        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(source_input))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(blend_input))
            .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(source_input.sampler))
            .update(m_device, m_neighborhood_descriptor_set);

        m_source_extent = source_input.extent;
        m_edge_extent = edge_input.extent;
        return true;
    }

    bool VulkanSmaaPass::recordEdge(
        VkCommandBuffer command_buffer,
        const VulkanSmaaRenderTarget& target,
        const RenderAntiAliasingSettings& settings) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || !isReady()) {
            return false;
        }
        if (target.color_format != m_mask_color_format || !beginRenderTarget(command_buffer, target)) {
            return false;
        }

        SmaaEdgePushConstants constants;
        constants.source_texel_width = safeTexelSize(m_source_extent.width);
        constants.source_texel_height = safeTexelSize(m_source_extent.height);
        constants.edge_threshold = sanitizeRange(settings.smaa_edge_threshold, 0.08f, 0.01f, 0.5f);
        constants.contrast_factor = sanitizeRange(settings.smaa_contrast_factor, 2.0f, 0.25f, 8.0f);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_edge_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_edge_pipeline_layout,
            0,
            1,
            &m_edge_descriptor_set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_edge_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SmaaEdgePushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanSmaaPass::recordBlend(
        VkCommandBuffer command_buffer,
        const VulkanSmaaRenderTarget& target,
        const RenderAntiAliasingSettings& settings) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || !isReady()) {
            return false;
        }
        if (target.color_format != m_mask_color_format || !beginRenderTarget(command_buffer, target)) {
            return false;
        }

        SmaaBlendPushConstants constants;
        constants.edge_texel_width = safeTexelSize(m_edge_extent.width);
        constants.edge_texel_height = safeTexelSize(m_edge_extent.height);
        constants.max_search_steps = std::clamp(settings.smaa_max_search_steps, 1u, 16u);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blend_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_blend_pipeline_layout,
            0,
            1,
            &m_blend_descriptor_set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_blend_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SmaaBlendPushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanSmaaPass::recordNeighborhood(
        VkCommandBuffer command_buffer,
        const VulkanSmaaRenderTarget& target,
        const RenderAntiAliasingSettings& settings) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || !isReady()) {
            return false;
        }
        if (target.color_format != m_output_color_format || !beginRenderTarget(command_buffer, target)) {
            return false;
        }

        SmaaNeighborhoodPushConstants constants;
        constants.source_texel_width = safeTexelSize(m_source_extent.width);
        constants.source_texel_height = safeTexelSize(m_source_extent.height);
        constants.blend_strength = sanitizeRange(settings.smaa_blend_strength, 0.85f, 0.0f, 1.0f);
        constants.debug_mode = 0u;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_neighborhood_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_neighborhood_pipeline_layout,
            0,
            1,
            &m_neighborhood_descriptor_set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_neighborhood_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SmaaNeighborhoodPushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanSmaaPass::recordDebugResolve(
        VkCommandBuffer command_buffer,
        const VulkanSmaaRenderTarget& target,
        const VulkanSmaaInput& input) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || !input.valid() || !isReady()) {
            return false;
        }
        if (target.color_format != m_output_color_format || !updateDebugInput(input) || !beginRenderTarget(command_buffer, target)) {
            return false;
        }

        SmaaNeighborhoodPushConstants constants;
        constants.source_texel_width = safeTexelSize(input.extent.width);
        constants.source_texel_height = safeTexelSize(input.extent.height);
        constants.blend_strength = 1.0f;
        constants.debug_mode = 1u;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_neighborhood_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_neighborhood_pipeline_layout,
            0,
            1,
            &m_debug_descriptor_set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_neighborhood_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SmaaNeighborhoodPushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanSmaaPass::createPipelines() {
        VkPushConstantRange edge_range{};
        edge_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        edge_range.offset = 0;
        edge_range.size = sizeof(SmaaEdgePushConstants);

        VkPushConstantRange blend_range{};
        blend_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        blend_range.offset = 0;
        blend_range.size = sizeof(SmaaBlendPushConstants);

        VkPushConstantRange neighborhood_range{};
        neighborhood_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        neighborhood_range.offset = 0;
        neighborhood_range.size = sizeof(SmaaNeighborhoodPushConstants);

        return createPipeline(
                   VulkanShaderProgramId::SmaaEdge,
                   m_single_input_descriptor_set_layout,
                   edge_range,
                   m_mask_color_format,
                   "SMAAEdge",
                   m_edge_pipeline,
                   m_edge_pipeline_layout) &&
               createPipeline(
                   VulkanShaderProgramId::SmaaBlend,
                   m_single_input_descriptor_set_layout,
                   blend_range,
                   m_mask_color_format,
                   "SMAABlend",
                   m_blend_pipeline,
                   m_blend_pipeline_layout) &&
               createPipeline(
                   VulkanShaderProgramId::SmaaNeighborhood,
                   m_dual_input_descriptor_set_layout,
                   neighborhood_range,
                   m_output_color_format,
                   "SMAANeighborhood",
                   m_neighborhood_pipeline,
                   m_neighborhood_pipeline_layout);
    }

    bool VulkanSmaaPass::createPipeline(
        VulkanShaderProgramId shader_program,
        VkDescriptorSetLayout descriptor_set_layout,
        const VkPushConstantRange& push_constant_range,
        VkFormat color_format,
        const char* debug_name,
        VkPipeline& pipeline,
        VkPipelineLayout& pipeline_layout) {
        if (!m_pipeline_cache || descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = debug_name;
        desc.shader_program = shader_program;
        desc.color_format = color_format;
        desc.depth_format = VK_FORMAT_UNDEFINED;
        desc.descriptor_set_layouts = { descriptor_set_layout };
        desc.push_constant_ranges = { push_constant_range };
        desc.vertex_layout = VulkanPipelineVertexLayout::None;
        desc.depth_test_enable = false;
        desc.depth_write_enable = false;
        desc.cull_mode = VK_CULL_MODE_NONE;

        const VulkanGraphicsPipelineState pipeline_state = m_pipeline_cache->getOrCreateGraphicsPipeline(desc);
        if (!pipeline_state.valid()) {
            return false;
        }

        pipeline = pipeline_state.pipeline;
        pipeline_layout = pipeline_state.layout;
        return true;
    }

    bool VulkanSmaaPass::allocateDescriptorSets() {
        if (!m_descriptor_allocator ||
            m_single_input_descriptor_set_layout == VK_NULL_HANDLE ||
            m_dual_input_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        m_edge_descriptor_allocation = m_descriptor_allocator->allocate(m_single_input_descriptor_set_layout);
        m_blend_descriptor_allocation = m_descriptor_allocator->allocate(m_single_input_descriptor_set_layout);
        m_neighborhood_descriptor_allocation = m_descriptor_allocator->allocate(m_dual_input_descriptor_set_layout);
        m_debug_descriptor_allocation = m_descriptor_allocator->allocate(m_dual_input_descriptor_set_layout);
        if (!m_edge_descriptor_allocation.valid() ||
            !m_blend_descriptor_allocation.valid() ||
            !m_neighborhood_descriptor_allocation.valid() ||
            !m_debug_descriptor_allocation.valid()) {
            return false;
        }

        m_edge_descriptor_set = m_edge_descriptor_allocation.set;
        m_blend_descriptor_set = m_blend_descriptor_allocation.set;
        m_neighborhood_descriptor_set = m_neighborhood_descriptor_allocation.set;
        m_debug_descriptor_set = m_debug_descriptor_allocation.set;
        return true;
    }

    void VulkanSmaaPass::cleanupPipelines() {
        m_edge_pipeline = VK_NULL_HANDLE;
        m_edge_pipeline_layout = VK_NULL_HANDLE;
        m_blend_pipeline = VK_NULL_HANDLE;
        m_blend_pipeline_layout = VK_NULL_HANDLE;
        m_neighborhood_pipeline = VK_NULL_HANDLE;
        m_neighborhood_pipeline_layout = VK_NULL_HANDLE;
    }

    void VulkanSmaaPass::freeDescriptorSets() {
        if (m_descriptor_allocator) {
            if (m_edge_descriptor_allocation.valid()) {
                m_descriptor_allocator->free(m_edge_descriptor_allocation);
            }
            if (m_blend_descriptor_allocation.valid()) {
                m_descriptor_allocator->free(m_blend_descriptor_allocation);
            }
            if (m_neighborhood_descriptor_allocation.valid()) {
                m_descriptor_allocator->free(m_neighborhood_descriptor_allocation);
            }
            if (m_debug_descriptor_allocation.valid()) {
                m_descriptor_allocator->free(m_debug_descriptor_allocation);
            }
        }

        m_edge_descriptor_allocation = {};
        m_edge_descriptor_set = VK_NULL_HANDLE;
        m_blend_descriptor_allocation = {};
        m_blend_descriptor_set = VK_NULL_HANDLE;
        m_neighborhood_descriptor_allocation = {};
        m_neighborhood_descriptor_set = VK_NULL_HANDLE;
        m_debug_descriptor_allocation = {};
        m_debug_descriptor_set = VK_NULL_HANDLE;
    }

    bool VulkanSmaaPass::updateDebugInput(const VulkanSmaaInput& input) {
        if (m_device == VK_NULL_HANDLE || m_debug_descriptor_set == VK_NULL_HANDLE || !input.valid()) {
            return false;
        }

        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(input))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(input))
            .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(input.sampler))
            .update(m_device, m_debug_descriptor_set);
        return true;
    }

    bool VulkanSmaaPass::beginRenderTarget(VkCommandBuffer command_buffer, const VulkanSmaaRenderTarget& target) const {
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

    void VulkanSmaaPass::drawFullscreen(VkCommandBuffer command_buffer) const {
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
    }
} // namespace NexAur
