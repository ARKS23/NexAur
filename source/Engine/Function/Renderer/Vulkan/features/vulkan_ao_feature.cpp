#include "pch.h"
#include "vulkan_ao_feature.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

#include <algorithm>

namespace NexAur {
    VulkanAoFeature::~VulkanAoFeature() {
        shutdown();
    }

    bool VulkanAoFeature::init(
        const VulkanRenderFeatureContext& context,
        VkFormat color_format,
        uint32_t width,
        uint32_t height,
        bool half_resolution) {
        shutdown();
        if (!context.valid() || color_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        m_context = context;
        if (!m_target.init(context.resources, color_format, width, height, half_resolution) ||
            !recreatePassResources()) {
            shutdown();
            return false;
        }
        return true;
    }

    void VulkanAoFeature::shutdown() {
        for (VulkanAoPass& pass : m_passes) {
            pass.shutdown();
        }
        m_target.shutdown();
        m_context = {};
    }

    bool VulkanAoFeature::prepare(
        const RenderAoSettings& settings,
        uint32_t width,
        uint32_t height) {
        const VkExtent2D expected_extent{
            settings.half_resolution ? std::max(1u, width / 2u) : std::max(1u, width),
            settings.half_resolution ? std::max(1u, height / 2u) : std::max(1u, height)
        };
        if (m_target.isReady()) {
            const VkExtent2D current_extent = m_target.getExtent();
            if (current_extent.width == expected_extent.width &&
                current_extent.height == expected_extent.height &&
                m_target.isHalfResolution() == settings.half_resolution) {
                return isReady() || recreatePassResources();
            }
        }

        if (m_context.resources.device == VK_NULL_HANDLE) {
            return false;
        }
        vkDeviceWaitIdle(m_context.resources.device);
        return resize(width, height, settings.half_resolution);
    }

    bool VulkanAoFeature::resize(uint32_t width, uint32_t height, bool half_resolution) {
        return m_target.resize(width, height, half_resolution) && recreatePassResources();
    }

    bool VulkanAoFeature::isReady() const {
        return m_target.isReady() &&
               std::all_of(
                   m_passes.begin(),
                   m_passes.end(),
                   [](const VulkanAoPass& pass) { return pass.isReady(); });
    }

    VulkanAoFeatureGraphResources VulkanAoFeature::addGraphResources(VulkanPassGraph& graph) {
        VulkanAoFeatureGraphResources resources;
        if (!m_target.isReady()) {
            return resources;
        }

        const VulkanAoImageView& raw = m_target.getRawImage();
        VulkanGraphImageDesc raw_desc;
        raw_desc.name = "AORaw";
        raw_desc.image = raw.image;
        raw_desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        raw_desc.initial_layout = raw.layout;
        raw_desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setRawLayout(layout);
        };
        resources.raw = graph.addImage(std::move(raw_desc));

        const VulkanAoImageView& blurred = m_target.getBlurredImage();
        VulkanGraphImageDesc blurred_desc;
        blurred_desc.name = "AOBlurred";
        blurred_desc.image = blurred.image;
        blurred_desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        blurred_desc.initial_layout = blurred.layout;
        blurred_desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setBlurredLayout(layout);
        };
        resources.blurred = graph.addImage(std::move(blurred_desc));
        return resources;
    }

    bool VulkanAoFeature::addPasses(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle scene_depth,
        const VulkanAoFeatureGraphResources& resources,
        VkImageView scene_depth_view,
        const VulkanRenderView& view,
        const RenderAoSettings& settings,
        uint32_t frame_index) {
        if (!isReady() || !scene_depth.valid() || !resources.valid()) {
            return false;
        }

        VulkanAoInput depth_input;
        depth_input.view = scene_depth_view;
        depth_input.sampler = m_target.getSampler();
        depth_input.extent = { view.viewport_width, view.viewport_height };
        depth_input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        const VulkanAoImageView& raw = m_target.getRawImage();
        VulkanAoInput raw_input;
        raw_input.view = raw.view;
        raw_input.sampler = m_target.getSampler();
        raw_input.extent = raw.extent;
        raw_input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VulkanAoPass& pass = getPass(frame_index);
        if (!pass.updateInputs(depth_input, raw_input)) {
            return false;
        }
        VulkanAoPass* frame_pass = &pass;

        const VulkanAoRenderTarget raw_target = m_target.getRawRenderTarget();
        graph.addPass("SSAO")
            .readImage(scene_depth, VulkanGraphImageUsage::ShaderRead)
            .writeImage(resources.raw, VulkanGraphImageUsage::ColorAttachment)
            .execute([frame_pass, raw_target, view, settings](VkCommandBuffer command_buffer) {
                return frame_pass->recordSsao(command_buffer, raw_target, view, settings);
            });

        const VulkanAoRenderTarget blurred_target = m_target.getBlurredRenderTarget();
        graph.addPass("AOBlur")
            .readImage(resources.raw, VulkanGraphImageUsage::ShaderRead)
            .writeImage(resources.blurred, VulkanGraphImageUsage::ColorAttachment)
            .execute([frame_pass, blurred_target, settings](VkCommandBuffer command_buffer) {
                return frame_pass->recordBlur(command_buffer, blurred_target, settings);
            });
        return true;
    }

    VulkanAoFeatureInput VulkanAoFeature::getPostProcessInput() const {
        VulkanAoFeatureInput input;
        if (!m_target.isReady()) {
            return input;
        }
        input.raw_view = m_target.getRawImage().view;
        input.blurred_view = m_target.getBlurredImage().view;
        input.sampler = m_target.getSampler();
        return input;
    }

    RendererDebugAoStats VulkanAoFeature::buildDebugStats(bool enabled) const {
        RendererDebugAoStats stats;
        stats.enabled = enabled;
        stats.ready = isReady();
        if (!m_target.isReady()) {
            return stats;
        }
        const VkExtent2D extent = m_target.getExtent();
        stats.width = extent.width;
        stats.height = extent.height;
        stats.color_format = VulkanDiagnosticsCollector::vkFormatToString(m_target.getColorFormat());
        stats.half_resolution = m_target.isHalfResolution();
        return stats;
    }

    bool VulkanAoFeature::recreatePassResources() {
        if (!m_target.isReady()) {
            return true;
        }
        VulkanAoPassContext context;
        context.device = m_context.resources.device;
        context.color_format = m_target.getColorFormat();
        context.input_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::AoInput);
        context.descriptor_allocator = m_context.descriptor_allocator;
        context.pipeline_cache = m_context.pipeline_cache;
        for (VulkanAoPass& pass : m_passes) {
            if (!pass.recreateResources(context)) {
                for (VulkanAoPass& cleanup_pass : m_passes) {
                    cleanup_pass.cleanupResources();
                }
                return false;
            }
        }
        return true;
    }

    VulkanAoPass& VulkanAoFeature::getPass(uint32_t frame_index) {
        return m_passes[frame_index % kVulkanFramesInFlight];
    }
} // namespace NexAur
