#include "pch.h"
#include "vulkan_bloom_pass.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

#include <algorithm>
#include <cmath>

namespace NexAur {
    namespace {
        struct BloomDownsamplePushConstants {
            float source_texel_width = 1.0f;
            float source_texel_height = 1.0f;
            uint32_t prefilter = 0;
            uint32_t padding0 = 0;
        };

        struct BloomUpsamplePushConstants {
            float low_texel_width = 1.0f;
            float low_texel_height = 1.0f;
            float radius = 1.0f;
            float scatter = 0.7f;
        };

        struct BloomCompositePushConstants {
            float intensity = 0.08f;
            float padding0 = 0.0f;
            float padding1 = 0.0f;
            float padding2 = 0.0f;
        };

        static_assert(sizeof(BloomDownsamplePushConstants) <= 128, "Bloom downsample push constants exceed Vulkan minimum limit.");
        static_assert(sizeof(BloomUpsamplePushConstants) <= 128, "Bloom upsample push constants exceed Vulkan minimum limit.");
        static_assert(sizeof(BloomCompositePushConstants) <= 128, "Bloom composite push constants exceed Vulkan minimum limit.");

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
    } // namespace

    VulkanBloomPass::~VulkanBloomPass() {
        shutdown();
    }

    bool VulkanBloomPass::recreateResources(const VulkanBloomPassContext& context, uint32_t mip_count) {
        cleanupResources();

        if (!context.valid() || mip_count == 0) {
            NX_CORE_ERROR("VulkanBloomPass requires a valid context and at least one mip.");
            return false;
        }

        m_device = context.device;
        m_color_format = context.color_format;
        m_single_input_descriptor_set_layout = context.single_input_descriptor_set_layout;
        m_dual_input_descriptor_set_layout = context.dual_input_descriptor_set_layout;
        m_descriptor_allocator = context.descriptor_allocator;
        m_pipeline_cache = context.pipeline_cache;

        if (!createPipelines() || !allocateDescriptorSets(mip_count)) {
            cleanupResources();
            return false;
        }

        return true;
    }

    void VulkanBloomPass::cleanupResources() {
        cleanupPipelines();
        freeDescriptorSets();
        m_color_format = VK_FORMAT_UNDEFINED;
        m_single_input_descriptor_set_layout = VK_NULL_HANDLE;
        m_dual_input_descriptor_set_layout = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_pipeline_cache = nullptr;
    }

    void VulkanBloomPass::shutdown() {
        cleanupResources();
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanBloomPass::updateInputs(const VulkanBloomInput& scene_color, const VulkanBloomTarget& target) {
        if (m_device == VK_NULL_HANDLE || !scene_color.valid() || !target.isReady()) {
            return false;
        }

        const uint32_t mip_count = target.getMipCount();
        if (mip_count == 0 || m_downsample_descriptor_sets.size() != mip_count) {
            return false;
        }

        for (uint32_t mip_index = 0; mip_index < mip_count; ++mip_index) {
            SingleInputDescriptor& descriptor = m_downsample_descriptor_sets[mip_index];
            const bool reads_scene_color = mip_index == 0;
            const VulkanBloomImageView& input_image = reads_scene_color ?
                VulkanBloomImageView{ VK_NULL_HANDLE, scene_color.color_view, target.getColorFormat(), scene_color.extent, scene_color.layout } :
                target.getDownsampleImage(mip_index - 1);
            const VkSampler input_sampler = reads_scene_color ? scene_color.sampler : target.getSampler();

            descriptor.input_extent = input_image.extent;
            VulkanDescriptorWriter writer;
            writer
                .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(input_image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
                .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(input_sampler))
                .update(m_device, descriptor.set);
        }

        for (uint32_t mip_index = 0; mip_index < m_upsample_descriptor_sets.size(); ++mip_index) {
            DualInputDescriptor& descriptor = m_upsample_descriptor_sets[mip_index];
            const VulkanBloomImageView& high_image = target.getDownsampleImage(mip_index);
            const VulkanBloomImageView& low_image = (mip_index + 1 == mip_count - 1) ?
                target.getDownsampleImage(mip_index + 1) :
                target.getUpsampleImage(mip_index + 1);

            descriptor.low_input_extent = low_image.extent;
            VulkanDescriptorWriter writer;
            writer
                .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(high_image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
                .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(low_image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
                .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(target.getSampler()))
                .update(m_device, descriptor.set);
        }

        const VulkanBloomImageView& final_bloom_image =
            mip_count > 1 ? target.getUpsampleImage(0) : target.getDownsampleImage(0);
        VulkanDescriptorWriter writer;
        writer
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(scene_color.color_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageInfo(final_bloom_image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
            .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLER, samplerInfo(target.getSampler()))
            .update(m_device, m_composite_descriptor_set);
        return true;
    }

    bool VulkanBloomPass::recordDownsample(
        VkCommandBuffer command_buffer,
        uint32_t mip_index,
        const VulkanBloomRenderTarget& target) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || mip_index >= m_downsample_descriptor_sets.size()) {
            return false;
        }
        if (target.color_format != m_color_format || !isReady()) {
            return false;
        }
        if (!beginRenderTarget(command_buffer, target)) {
            return false;
        }

        const VkExtent2D input_extent = m_downsample_descriptor_sets[mip_index].input_extent;
        BloomDownsamplePushConstants constants;
        constants.source_texel_width = safeTexelSize(input_extent.width);
        constants.source_texel_height = safeTexelSize(input_extent.height);
        constants.prefilter = mip_index == 0 ? 1u : 0u;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_downsample_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_downsample_pipeline_layout,
            0,
            1,
            &m_downsample_descriptor_sets[mip_index].set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_downsample_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(BloomDownsamplePushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanBloomPass::recordUpsample(
        VkCommandBuffer command_buffer,
        uint32_t mip_index,
        const VulkanBloomRenderTarget& target,
        const RenderPostProcessSettings& settings) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || mip_index >= m_upsample_descriptor_sets.size()) {
            return false;
        }
        if (target.color_format != m_color_format || !isReady()) {
            return false;
        }
        if (!beginRenderTarget(command_buffer, target)) {
            return false;
        }

        const VkExtent2D low_extent = m_upsample_descriptor_sets[mip_index].low_input_extent;
        BloomUpsamplePushConstants constants;
        constants.low_texel_width = safeTexelSize(low_extent.width);
        constants.low_texel_height = safeTexelSize(low_extent.height);
        constants.radius = std::clamp(settings.bloom_radius, 0.25f, 2.5f);
        constants.scatter = std::clamp(settings.bloom_scatter, 0.0f, 1.0f);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_upsample_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_upsample_pipeline_layout,
            0,
            1,
            &m_upsample_descriptor_sets[mip_index].set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_upsample_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(BloomUpsamplePushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanBloomPass::recordComposite(
        VkCommandBuffer command_buffer,
        const VulkanBloomRenderTarget& target,
        const RenderPostProcessSettings& settings) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid() || !isReady()) {
            return false;
        }
        if (target.color_format != m_color_format || m_composite_descriptor_set == VK_NULL_HANDLE) {
            return false;
        }
        if (!beginRenderTarget(command_buffer, target)) {
            return false;
        }

        BloomCompositePushConstants constants;
        constants.intensity = settings.bloom_enabled ? std::clamp(settings.bloom_intensity, 0.0f, 1.0f) : 0.0f;

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_composite_pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_composite_pipeline_layout,
            0,
            1,
            &m_composite_descriptor_set,
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_composite_pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(BloomCompositePushConstants),
            &constants);
        drawFullscreen(command_buffer);
        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanBloomPass::createPipelines() {
        VkPushConstantRange downsample_range{};
        downsample_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        downsample_range.offset = 0;
        downsample_range.size = sizeof(BloomDownsamplePushConstants);

        VkPushConstantRange upsample_range{};
        upsample_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        upsample_range.offset = 0;
        upsample_range.size = sizeof(BloomUpsamplePushConstants);

        VkPushConstantRange composite_range{};
        composite_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        composite_range.offset = 0;
        composite_range.size = sizeof(BloomCompositePushConstants);

        return createPipeline(
                   VulkanShaderProgramId::BloomDownsample,
                   m_single_input_descriptor_set_layout,
                   downsample_range,
                   "BloomDownsample",
                   m_downsample_pipeline,
                   m_downsample_pipeline_layout) &&
               createPipeline(
                   VulkanShaderProgramId::BloomUpsample,
                   m_dual_input_descriptor_set_layout,
                   upsample_range,
                   "BloomUpsample",
                   m_upsample_pipeline,
                   m_upsample_pipeline_layout) &&
               createPipeline(
                   VulkanShaderProgramId::BloomComposite,
                   m_dual_input_descriptor_set_layout,
                   composite_range,
                   "BloomComposite",
                   m_composite_pipeline,
                   m_composite_pipeline_layout);
    }

    bool VulkanBloomPass::createPipeline(
        VulkanShaderProgramId shader_program,
        VkDescriptorSetLayout descriptor_set_layout,
        const VkPushConstantRange& push_constant_range,
        const char* debug_name,
        VkPipeline& pipeline,
        VkPipelineLayout& pipeline_layout) {
        if (!m_pipeline_cache || descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = debug_name;
        desc.shader_program = shader_program;
        desc.color_format = m_color_format;
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

    bool VulkanBloomPass::allocateDescriptorSets(uint32_t mip_count) {
        if (!m_descriptor_allocator || mip_count == 0) {
            return false;
        }

        m_downsample_descriptor_sets.resize(mip_count);
        for (SingleInputDescriptor& descriptor : m_downsample_descriptor_sets) {
            descriptor.allocation = m_descriptor_allocator->allocate(m_single_input_descriptor_set_layout);
            if (!descriptor.allocation.valid()) {
                return false;
            }
            descriptor.set = descriptor.allocation.set;
        }

        m_upsample_descriptor_sets.resize(mip_count > 1 ? mip_count - 1 : 0);
        for (DualInputDescriptor& descriptor : m_upsample_descriptor_sets) {
            descriptor.allocation = m_descriptor_allocator->allocate(m_dual_input_descriptor_set_layout);
            if (!descriptor.allocation.valid()) {
                return false;
            }
            descriptor.set = descriptor.allocation.set;
        }

        m_composite_descriptor_allocation = m_descriptor_allocator->allocate(m_dual_input_descriptor_set_layout);
        if (!m_composite_descriptor_allocation.valid()) {
            return false;
        }
        m_composite_descriptor_set = m_composite_descriptor_allocation.set;
        return true;
    }

    void VulkanBloomPass::cleanupPipelines() {
        m_downsample_pipeline = VK_NULL_HANDLE;
        m_downsample_pipeline_layout = VK_NULL_HANDLE;
        m_upsample_pipeline = VK_NULL_HANDLE;
        m_upsample_pipeline_layout = VK_NULL_HANDLE;
        m_composite_pipeline = VK_NULL_HANDLE;
        m_composite_pipeline_layout = VK_NULL_HANDLE;
    }

    void VulkanBloomPass::freeDescriptorSets() {
        if (m_descriptor_allocator) {
            for (SingleInputDescriptor& descriptor : m_downsample_descriptor_sets) {
                if (descriptor.allocation.valid()) {
                    m_descriptor_allocator->free(descriptor.allocation);
                }
            }
            for (DualInputDescriptor& descriptor : m_upsample_descriptor_sets) {
                if (descriptor.allocation.valid()) {
                    m_descriptor_allocator->free(descriptor.allocation);
                }
            }
            if (m_composite_descriptor_allocation.valid()) {
                m_descriptor_allocator->free(m_composite_descriptor_allocation);
            }
        }

        m_downsample_descriptor_sets.clear();
        m_upsample_descriptor_sets.clear();
        m_composite_descriptor_allocation = {};
        m_composite_descriptor_set = VK_NULL_HANDLE;
    }

    bool VulkanBloomPass::beginRenderTarget(
        VkCommandBuffer command_buffer,
        const VulkanBloomRenderTarget& target) const {
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

    void VulkanBloomPass::drawFullscreen(VkCommandBuffer command_buffer) const {
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
    }
} // namespace NexAur
