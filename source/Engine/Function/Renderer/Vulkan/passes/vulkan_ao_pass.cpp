#include "pch.h"
#include "vulkan_ao_pass.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace NexAur {
    namespace {
        struct SsaoPushConstants {
            glm::mat4 inverse_projection{ 1.0f };
            glm::vec4 ao_params{ 1.2f, 0.6f, 0.025f, 1.2f };
            glm::vec4 texture_params{ 1.0f, 1.0f, 1.0f, 0.0f };
        };

        struct AoBlurPushConstants {
            float source_texel_width = 1.0f;
            float source_texel_height = 1.0f;
            uint32_t blur_enabled = 1;
            uint32_t padding0 = 0;
        };

        static_assert(sizeof(SsaoPushConstants) <= 128, "SSAO push constants exceed Vulkan minimum limit.");
        static_assert(sizeof(AoBlurPushConstants) <= 128, "AO blur push constants exceed Vulkan minimum limit.");

        VkDescriptorImageInfo sampledImageInfo(const VulkanAoInput& input) {
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

        float sanitizeMin(float value, float fallback, float minimum) {
            return std::isfinite(value) && value >= minimum ? value : fallback;
        }
    } // namespace

    VulkanAoPass::~VulkanAoPass() {
        shutdown();
    }

    bool VulkanAoPass::recreateResources(const VulkanAoPassContext& context) {
        cleanupResources();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanAoPass requires a valid context.");
            return false;
        }

        m_device = context.device;
        m_color_format = context.color_format;
        m_input_descriptor_set_layout = context.input_descriptor_set_layout;
        m_descriptor_allocator = context.descriptor_allocator;
        m_pipeline_cache = context.pipeline_cache;

        if (!createPipelines() || !allocateDescriptorSets()) {
            cleanupResources();
            return false;
        }

        return true;
    }

    void VulkanAoPass::cleanupResources() {
        cleanupPipelines();
        freeDescriptorSets();
        m_color_format = VK_FORMAT_UNDEFINED;
        m_input_descriptor_set_layout = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_pipeline_cache = nullptr;
        m_depth_extent = {};
        m_raw_extent = {};
    }

    void VulkanAoPass::shutdown() {
        cleanupResources();
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanAoPass::updateInputs(const VulkanAoInput& depth_input, const VulkanAoInput& raw_ao_input) {
        if (m_device == VK_NULL_HANDLE ||
            m_depth_descriptor_set == VK_NULL_HANDLE ||
            m_raw_descriptor_set == VK_NULL_HANDLE ||
            !depth_input.valid() ||
            !raw_ao_input.valid()) {
            return false;
        }

        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(depth_input))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(depth_input.sampler))
            .update(m_device, m_depth_descriptor_set);

        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(raw_ao_input))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(raw_ao_input.sampler))
            .update(m_device, m_raw_descriptor_set);

        m_depth_extent = depth_input.extent;
        m_raw_extent = raw_ao_input.extent;
        return true;
    }

    bool VulkanAoPass::recordSsao(
        VkCommandBuffer command_buffer,
        const VulkanAoRenderTarget& target,
        const RenderView& view,
        const RenderAoSettings& settings) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || !isReady()) {
            return false;
        }
        if (target.color_format != m_color_format || !beginRenderTarget(command_buffer, target)) {
            return false;
        }

        SsaoPushConstants constants;
        constants.inverse_projection = view.inverse_projection_matrix;
        constants.ao_params = glm::vec4(
            sanitizeMin(settings.radius, 1.2f, 0.01f),
            std::clamp(settings.intensity, 0.0f, 2.0f),
            sanitizeMin(settings.bias, 0.025f, 0.0f),
            sanitizeMin(settings.power, 1.2f, 0.01f));
        constants.texture_params = glm::vec4(
            safeTexelSize(m_depth_extent.width),
            safeTexelSize(m_depth_extent.height),
            m_depth_extent.height > 0 ?
                static_cast<float>(m_depth_extent.width) / static_cast<float>(m_depth_extent.height) :
                1.0f,
            0.0f);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssao_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_ssao_pipeline_layout,
            0,
            1,
            &m_depth_descriptor_set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_ssao_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SsaoPushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanAoPass::recordBlur(
        VkCommandBuffer command_buffer,
        const VulkanAoRenderTarget& target,
        const RenderAoSettings& settings) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || !isReady()) {
            return false;
        }
        if (target.color_format != m_color_format || !beginRenderTarget(command_buffer, target)) {
            return false;
        }

        AoBlurPushConstants constants;
        constants.source_texel_width = safeTexelSize(m_raw_extent.width);
        constants.source_texel_height = safeTexelSize(m_raw_extent.height);
        constants.blur_enabled = settings.blur_enabled ? 1u : 0u;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blur_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_blur_pipeline_layout,
            0,
            1,
            &m_raw_descriptor_set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_blur_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(AoBlurPushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanAoPass::createPipelines() {
        VkPushConstantRange ssao_range{};
        ssao_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        ssao_range.offset = 0;
        ssao_range.size = sizeof(SsaoPushConstants);

        VkPushConstantRange blur_range{};
        blur_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        blur_range.offset = 0;
        blur_range.size = sizeof(AoBlurPushConstants);

        return createPipeline(
                   VulkanShaderProgramId::Ssao,
                   ssao_range,
                   "SSAO",
                   m_ssao_pipeline,
                   m_ssao_pipeline_layout) &&
               createPipeline(
                   VulkanShaderProgramId::AoBlur,
                   blur_range,
                   "AOBlur",
                   m_blur_pipeline,
                   m_blur_pipeline_layout);
    }

    bool VulkanAoPass::createPipeline(
        VulkanShaderProgramId shader_program,
        const VkPushConstantRange& push_constant_range,
        const char* debug_name,
        VkPipeline& pipeline,
        VkPipelineLayout& pipeline_layout) {
        if (!m_pipeline_cache || m_input_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = debug_name;
        desc.shader_program = shader_program;
        desc.color_format = m_color_format;
        desc.depth_format = VK_FORMAT_UNDEFINED;
        desc.descriptor_set_layouts = { m_input_descriptor_set_layout };
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

    bool VulkanAoPass::allocateDescriptorSets() {
        if (!m_descriptor_allocator || m_input_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        m_depth_descriptor_allocation = m_descriptor_allocator->allocate(m_input_descriptor_set_layout);
        m_raw_descriptor_allocation = m_descriptor_allocator->allocate(m_input_descriptor_set_layout);
        if (!m_depth_descriptor_allocation.valid() || !m_raw_descriptor_allocation.valid()) {
            return false;
        }

        m_depth_descriptor_set = m_depth_descriptor_allocation.set;
        m_raw_descriptor_set = m_raw_descriptor_allocation.set;
        return true;
    }

    void VulkanAoPass::cleanupPipelines() {
        m_ssao_pipeline = VK_NULL_HANDLE;
        m_ssao_pipeline_layout = VK_NULL_HANDLE;
        m_blur_pipeline = VK_NULL_HANDLE;
        m_blur_pipeline_layout = VK_NULL_HANDLE;
    }

    void VulkanAoPass::freeDescriptorSets() {
        if (m_descriptor_allocator) {
            if (m_depth_descriptor_allocation.valid()) {
                m_descriptor_allocator->free(m_depth_descriptor_allocation);
            }
            if (m_raw_descriptor_allocation.valid()) {
                m_descriptor_allocator->free(m_raw_descriptor_allocation);
            }
        }

        m_depth_descriptor_allocation = {};
        m_depth_descriptor_set = VK_NULL_HANDLE;
        m_raw_descriptor_allocation = {};
        m_raw_descriptor_set = VK_NULL_HANDLE;
    }

    bool VulkanAoPass::beginRenderTarget(VkCommandBuffer command_buffer, const VulkanAoRenderTarget& target) const {
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

    void VulkanAoPass::drawFullscreen(VkCommandBuffer command_buffer) const {
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
    }
} // namespace NexAur
