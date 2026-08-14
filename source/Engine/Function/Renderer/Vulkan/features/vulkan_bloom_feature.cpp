#include "pch.h"
#include "vulkan_bloom_feature.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

namespace NexAur {
    VulkanBloomFeature::~VulkanBloomFeature() {
        shutdown();
    }

    bool VulkanBloomFeature::init(
        const VulkanRenderFeatureContext& context,
        VkFormat color_format,
        uint32_t width,
        uint32_t height,
        const VulkanFeatureImageInput& scene_color_input) {
        shutdown();
        if (!context.valid() ||
            color_format == VK_FORMAT_UNDEFINED ||
            !scene_color_input.valid()) {
            return false;
        }
        m_context = context;
        if (!m_target.init(context.resources, color_format, width, height) ||
            !recreateSwapchainResources(scene_color_input)) {
            shutdown();
            return false;
        }
        return true;
    }

    void VulkanBloomFeature::shutdown() {
        m_pass.shutdown();
        m_target.shutdown();
        m_context = {};
    }

    void VulkanBloomFeature::cleanupSwapchainResources() {
        m_pass.cleanupResources();
    }

    bool VulkanBloomFeature::recreateSwapchainResources(
        const VulkanFeatureImageInput& scene_color_input) {
        if (!m_target.isReady()) {
            return true;
        }
        VulkanBloomPassContext context;
        context.device = m_context.resources.device;
        context.color_format = m_target.getColorFormat();
        context.single_input_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::PostProcessInput);
        context.dual_input_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::BloomDualInput);
        context.descriptor_allocator = m_context.descriptor_allocator;
        context.pipeline_cache = m_context.pipeline_cache;
        return m_pass.recreateResources(context, m_target.getMipCount()) &&
               updateInputs(scene_color_input);
    }

    bool VulkanBloomFeature::resize(
        uint32_t width,
        uint32_t height,
        const VulkanFeatureImageInput& scene_color_input) {
        if (!m_target.resize(width, height)) {
            return false;
        }
        return recreateSwapchainResources(scene_color_input);
    }

    bool VulkanBloomFeature::addPasses(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle scene_color,
        const VulkanFeatureImageInput& scene_color_input,
        const RenderPostProcessSettings& settings,
        const RenderEffectDebugSettings& debug_settings,
        VulkanBloomFeatureOutput& output) {
        output.color = scene_color;
        output.input = scene_color_input;
        if (!isReady() || !scene_color.valid() || !scene_color_input.valid()) {
            return false;
        }

        const uint32_t mip_count = m_target.getMipCount();
        if (mip_count == 0) {
            return true;
        }

        std::vector<VulkanGraphImageHandle> downsample_images;
        downsample_images.reserve(mip_count);
        for (uint32_t mip_index = 0; mip_index < mip_count; ++mip_index) {
            VulkanGraphImageHandle image = addImage(graph, mip_index, false);
            if (!image.valid()) {
                return false;
            }
            downsample_images.push_back(image);
        }

        std::vector<VulkanGraphImageHandle> upsample_images;
        upsample_images.reserve(mip_count > 1 ? mip_count - 1 : 0);
        for (uint32_t mip_index = 0; mip_index + 1 < mip_count; ++mip_index) {
            VulkanGraphImageHandle image = addImage(graph, mip_index, true);
            if (!image.valid()) {
                return false;
            }
            upsample_images.push_back(image);
        }

        for (uint32_t mip_index = 0; mip_index < mip_count; ++mip_index) {
            const VulkanGraphImageHandle input_color =
                mip_index == 0 ? scene_color : downsample_images[mip_index - 1];
            const VulkanGraphImageHandle output_color = downsample_images[mip_index];
            const VulkanBloomRenderTarget target = m_target.getDownsampleRenderTarget(mip_index);
            graph.addPass("BloomDownsample" + std::to_string(mip_index))
                .readImage(input_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, mip_index, target](VkCommandBuffer command_buffer) {
                    return m_pass.recordDownsample(command_buffer, mip_index, target);
                });
        }

        for (uint32_t step = 0; step + 1 < mip_count; ++step) {
            const uint32_t mip_index = mip_count - 2u - step;
            const VulkanGraphImageHandle high_color = downsample_images[mip_index];
            const VulkanGraphImageHandle low_color = (mip_index + 1u == mip_count - 1u) ?
                downsample_images[mip_index + 1u] :
                upsample_images[mip_index + 1u];
            const VulkanGraphImageHandle output_color = upsample_images[mip_index];
            const VulkanBloomRenderTarget target = m_target.getUpsampleRenderTarget(mip_index);
            graph.addPass("BloomUpsample" + std::to_string(mip_index))
                .readImage(high_color, VulkanGraphImageUsage::ShaderRead)
                .readImage(low_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, mip_index, target, settings](VkCommandBuffer command_buffer) {
                    return m_pass.recordUpsample(command_buffer, mip_index, target, settings);
                });
        }

        if (debug_settings.view == RenderEffectDebugView::BloomDownsampleMip) {
            const uint32_t selected_mip = std::min(debug_settings.bloom_mip, mip_count - 1u);
            output.color = downsample_images[selected_mip];
            output.input = makeInput(m_target.getDownsampleImage(selected_mip));
            return true;
        }

        if (debug_settings.view == RenderEffectDebugView::BloomUpsampleMip) {
            if (upsample_images.empty()) {
                output.color = downsample_images[0];
                output.input = makeInput(m_target.getDownsampleImage(0));
                return true;
            }
            const uint32_t selected_mip = std::min(
                debug_settings.bloom_mip,
                static_cast<uint32_t>(upsample_images.size() - 1u));
            output.color = upsample_images[selected_mip];
            output.input = makeInput(m_target.getUpsampleImage(selected_mip));
            return true;
        }

        const VulkanGraphImageHandle final_bloom_color =
            mip_count > 1 ? upsample_images[0] : downsample_images[0];
        const VulkanGraphImageHandle composite = addCompositeImage(graph);
        if (!composite.valid()) {
            return false;
        }
        const VulkanBloomRenderTarget target = m_target.getCompositeRenderTarget();
        graph.addPass("BloomComposite")
            .readImage(scene_color, VulkanGraphImageUsage::ShaderRead)
            .readImage(final_bloom_color, VulkanGraphImageUsage::ShaderRead)
            .writeImage(composite, VulkanGraphImageUsage::ColorAttachment)
            .execute([this, target, settings](VkCommandBuffer command_buffer) {
                return m_pass.recordComposite(command_buffer, target, settings);
            });

        output.color = composite;
        output.input = makeInput(m_target.getCompositeImage());
        return true;
    }

    RendererDebugBloomStats VulkanBloomFeature::buildDebugStats(bool enabled) const {
        RendererDebugBloomStats stats;
        stats.enabled = enabled;
        stats.ready = isReady();
        if (!m_target.isReady()) {
            return stats;
        }
        const VkExtent2D extent = m_target.getExtent();
        stats.width = extent.width;
        stats.height = extent.height;
        stats.mip_count = m_target.getMipCount();
        stats.color_format = VulkanDiagnosticsCollector::vkFormatToString(m_target.getColorFormat());
        return stats;
    }

    VulkanGraphImageHandle VulkanBloomFeature::addImage(
        VulkanPassGraph& graph,
        uint32_t mip_index,
        bool upsample) {
        const VulkanBloomImageView& image = upsample ?
            m_target.getUpsampleImage(mip_index) :
            m_target.getDownsampleImage(mip_index);
        VulkanGraphImageDesc desc;
        desc.name = std::string(upsample ? "BloomUpsample" : "BloomDownsample") +
                    std::to_string(mip_index);
        desc.image = image.image;
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.initial_layout = image.layout;
        desc.commit_layout = [this, mip_index, upsample](VkImageLayout layout) {
            if (upsample) {
                m_target.setUpsampleLayout(mip_index, layout);
            } else {
                m_target.setDownsampleLayout(mip_index, layout);
            }
        };
        return graph.addImage(std::move(desc));
    }

    VulkanGraphImageHandle VulkanBloomFeature::addCompositeImage(VulkanPassGraph& graph) {
        const VulkanBloomImageView& image = m_target.getCompositeImage();
        VulkanGraphImageDesc desc;
        desc.name = "BloomComposite";
        desc.image = image.image;
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.initial_layout = image.layout;
        desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setCompositeLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    VulkanFeatureImageInput VulkanBloomFeature::makeInput(
        const VulkanBloomImageView& image) const {
        VulkanFeatureImageInput input;
        input.view = image.view;
        input.sampler = m_target.getSampler();
        input.extent = image.extent;
        return input;
    }

    bool VulkanBloomFeature::updateInputs(
        const VulkanFeatureImageInput& scene_color_input) {
        if (!m_pass.isReady() || !m_target.isReady() || !scene_color_input.valid()) {
            return false;
        }
        VulkanBloomInput input;
        input.color_view = scene_color_input.view;
        input.sampler = scene_color_input.sampler;
        input.extent = scene_color_input.extent;
        input.layout = scene_color_input.layout;
        return m_pass.updateInputs(input, m_target);
    }
} // namespace NexAur
