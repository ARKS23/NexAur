#include "pch.h"
#include "vulkan_post_process_pass.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

#include <algorithm>
#include <cstdint>

namespace NexAur {
    namespace {
        constexpr uint32_t kPostProcessFlagManualGamma = 1u << 0u;

        struct VulkanPostProcessPushConstants {
            float exposure = 1.0f;
            float output_gamma = 2.2f;
            uint32_t tone_mapping_mode = static_cast<uint32_t>(RenderToneMappingMode::ACES);
            uint32_t flags = 0;
            uint32_t effect_debug_view = static_cast<uint32_t>(RenderEffectDebugView::FinalLit);
            uint32_t effect_debug_index = 0;
            uint32_t shadow_layer_count = 1;
            uint32_t _padding0 = 0;
            float ao_intensity = 0.0f;
            float ao_power = 1.0f;
            uint32_t ao_enabled = 0;
            uint32_t _padding1 = 0;
        };

        static_assert(
            sizeof(VulkanPostProcessPushConstants) <= 128,
            "Post process push constants exceed Vulkan minimum limit.");

        bool isSrgbColorFormat(VkFormat format) {
            switch (format) {
            case VK_FORMAT_R8_SRGB:
            case VK_FORMAT_R8G8_SRGB:
            case VK_FORMAT_R8G8B8_SRGB:
            case VK_FORMAT_B8G8R8_SRGB:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                return true;
            default:
                return false;
            }
        }

        VulkanPostProcessPushConstants makePushConstants(
            const VulkanPostProcessRenderTarget& target,
            const RenderPostProcessSettings& post_process_settings,
            const RenderAoSettings& ao_settings,
            const RenderEffectDebugSettings& debug_settings,
            uint32_t shadow_layer_count,
            uint32_t point_shadow_layer_count) {
            VulkanPostProcessPushConstants constants;
            constants.exposure = std::max(0.0f, post_process_settings.exposure);
            constants.tone_mapping_mode = static_cast<uint32_t>(post_process_settings.tone_mapping_mode);
            constants.output_gamma = 2.2f;
            constants.flags = isSrgbColorFormat(target.color_format) ? 0u : kPostProcessFlagManualGamma;
            constants.effect_debug_view = static_cast<uint32_t>(debug_settings.view);
            if (debug_settings.view == RenderEffectDebugView::PointShadowMap) {
                constants.effect_debug_index = debug_settings.point_shadow_layer;
                constants.shadow_layer_count = std::max(1u, point_shadow_layer_count);
            } else {
                constants.effect_debug_index = debug_settings.shadow_cascade;
                constants.shadow_layer_count = std::max(1u, shadow_layer_count);
            }
            constants.ao_intensity = std::clamp(ao_settings.intensity, 0.0f, 2.0f);
            constants.ao_power = std::max(0.01f, ao_settings.power);
            constants.ao_enabled = ao_settings.enabled ? 1u : 0u;
            return constants;
        }
    } // namespace

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
        m_current_input = {};
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

        VkDescriptorImageInfo shadow_info{};
        shadow_info.imageView = input.shadow_view;
        shadow_info.imageLayout = input.shadow_layout;

        VkDescriptorImageInfo shadow_sampler_info{};
        shadow_sampler_info.sampler = input.shadow_sampler;

        VkDescriptorImageInfo point_shadow_info{};
        point_shadow_info.imageView = input.point_shadow_view;
        point_shadow_info.imageLayout = input.point_shadow_layout;

        VkDescriptorImageInfo point_shadow_sampler_info{};
        point_shadow_sampler_info.sampler = input.point_shadow_sampler;

        VkDescriptorImageInfo scene_depth_info{};
        scene_depth_info.imageView = input.scene_depth_view;
        scene_depth_info.imageLayout = input.scene_depth_layout;

        VkDescriptorImageInfo ao_raw_info{};
        ao_raw_info.imageView = input.ao_raw_view;
        ao_raw_info.imageLayout = input.ao_layout;

        VkDescriptorImageInfo ao_blurred_info{};
        ao_blurred_info.imageView = input.ao_blurred_view;
        ao_blurred_info.imageLayout = input.ao_layout;

        VkDescriptorImageInfo ao_sampler_info{};
        ao_sampler_info.sampler = input.ao_sampler;

        VulkanDescriptorWriter writer;
        writer
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, color_info)
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLER, sampler_info)
            .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_info)
            .writeImage(3, VK_DESCRIPTOR_TYPE_SAMPLER, shadow_sampler_info)
            .writeImage(4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, scene_depth_info)
            .writeImage(5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ao_raw_info)
            .writeImage(6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ao_blurred_info)
            .writeImage(7, VK_DESCRIPTOR_TYPE_SAMPLER, ao_sampler_info)
            .writeImage(8, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, point_shadow_info)
            .writeImage(9, VK_DESCRIPTOR_TYPE_SAMPLER, point_shadow_sampler_info)
            .update(m_device, m_input_descriptor_set);
        m_current_input = input;
        return true;
    }

    bool VulkanPostProcessPass::record(
        VkCommandBuffer command_buffer,
        const VulkanPostProcessRenderTarget& target,
        const RenderPostProcessSettings& post_process_settings,
        const RenderAoSettings& ao_settings,
        const RenderEffectDebugSettings& debug_settings) {
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
        const VulkanPostProcessPushConstants push_constants = makePushConstants(
            target,
            post_process_settings,
            ao_settings,
            debug_settings,
            m_current_input.shadow_layer_count,
            m_current_input.point_shadow_layer_count);
        vkCmdPushConstants(
            command_buffer,
            m_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(VulkanPostProcessPushConstants),
            &push_constants);
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

        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(VulkanPostProcessPushConstants);
        desc.push_constant_ranges = { push_constant_range };

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
