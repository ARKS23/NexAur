#include "pch.h"
#include "vulkan_ssr_feature.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

#include <algorithm>

namespace NexAur {
    VulkanSsrFeature::~VulkanSsrFeature() {
        shutdown();
    }

    bool VulkanSsrFeature::init(
        const VulkanRenderFeatureContext& context,
        VkFormat reflection_format,
        VkFormat hit_mask_format,
        uint32_t width,
        uint32_t height) {
        shutdown();
        if (!context.valid() ||
            reflection_format == VK_FORMAT_UNDEFINED ||
            hit_mask_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        m_context = context;
        if (!m_target.init(context.resources, reflection_format, hit_mask_format, width, height) ||
            !recreateSwapchainResources()) {
            shutdown();
            return false;
        }
        return true;
    }

    void VulkanSsrFeature::shutdown() {
        for (VulkanSsrPass& pass : m_passes) {
            pass.shutdown();
        }
        m_target.shutdown();
        m_context = {};
    }

    void VulkanSsrFeature::cleanupSwapchainResources() {
        for (VulkanSsrPass& pass : m_passes) {
            pass.cleanupResources();
        }
    }

    bool VulkanSsrFeature::recreateSwapchainResources() {
        if (!m_target.isReady()) {
            return true;
        }
        VulkanSsrPassContext context;
        context.device = m_context.resources.device;
        context.reflection_color_format = m_target.getReflectionFormat();
        context.hit_mask_format = m_target.getHitMaskFormat();
        context.input_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::BloomDualInput);
        context.descriptor_allocator = m_context.descriptor_allocator;
        context.pipeline_cache = m_context.pipeline_cache;
        for (VulkanSsrPass& pass : m_passes) {
            if (!pass.recreateResources(context)) {
                cleanupSwapchainResources();
                return false;
            }
        }
        return true;
    }

    bool VulkanSsrFeature::resize(uint32_t width, uint32_t height) {
        return m_target.resize(width, height) && recreateSwapchainResources();
    }

    bool VulkanSsrFeature::isReady() const {
        return m_target.isReady() &&
               std::all_of(
                   m_passes.begin(),
                   m_passes.end(),
                   [](const VulkanSsrPass& pass) { return pass.isReady(); });
    }

    VulkanSsrFeatureGraphResources VulkanSsrFeature::addGraphResources(VulkanPassGraph& graph) {
        VulkanSsrFeatureGraphResources resources;
        if (!m_target.isReady()) {
            return resources;
        }

        const VulkanSsrImageView& raw = m_target.getRawReflectionImage();
        VulkanGraphImageDesc raw_desc;
        raw_desc.name = "SSRRawReflection";
        raw_desc.image = raw.image;
        raw_desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        raw_desc.initial_layout = raw.layout;
        raw_desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setRawReflectionLayout(layout);
        };
        resources.raw_reflection = graph.addImage(std::move(raw_desc));

        const VulkanSsrImageView& hit_mask = m_target.getHitMaskImage();
        VulkanGraphImageDesc hit_mask_desc;
        hit_mask_desc.name = "SSRHitMask";
        hit_mask_desc.image = hit_mask.image;
        hit_mask_desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        hit_mask_desc.initial_layout = hit_mask.layout;
        hit_mask_desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setHitMaskLayout(layout);
        };
        resources.hit_mask = graph.addImage(std::move(hit_mask_desc));
        return resources;
    }

    bool VulkanSsrFeature::addPasses(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle scene_color,
        VulkanGraphImageHandle scene_depth,
        const VulkanSsrFeatureGraphResources& resources,
        const VulkanFeatureImageInput& scene_color_input,
        VkImageView scene_depth_view,
        const VulkanRenderView& view,
        const RenderSsrSettings& settings,
        uint32_t frame_index) {
        if (!isReady() ||
            !scene_color.valid() ||
            !scene_depth.valid() ||
            !resources.valid() ||
            !scene_color_input.valid() ||
            scene_depth_view == VK_NULL_HANDLE) {
            return false;
        }

        VulkanSsrInput input;
        input.scene_color_view = scene_color_input.view;
        input.scene_depth_view = scene_depth_view;
        input.sampler = scene_color_input.sampler;
        input.extent = view.viewport_width > 0 && view.viewport_height > 0 ?
            VkExtent2D{ view.viewport_width, view.viewport_height } :
            scene_color_input.extent;
        input.scene_color_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input.scene_depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VulkanSsrPass& pass = getPass(frame_index);
        if (!pass.updateInput(input)) {
            return false;
        }
        VulkanSsrPass* frame_pass = &pass;

        const VulkanSsrRenderTarget raw_target = m_target.getRawReflectionRenderTarget();
        graph.addPass("SSRRawReflection")
            .readImage(scene_color, VulkanGraphImageUsage::ShaderRead)
            .readImage(scene_depth, VulkanGraphImageUsage::ShaderRead)
            .writeImage(resources.raw_reflection, VulkanGraphImageUsage::ColorAttachment)
            .execute([frame_pass, raw_target, view, settings](VkCommandBuffer command_buffer) {
                return frame_pass->recordTrace(
                    command_buffer,
                    raw_target,
                    view,
                    settings,
                    VulkanSsrOutputMode::RawReflection);
            });

        const VulkanSsrRenderTarget hit_mask_target = m_target.getHitMaskRenderTarget();
        graph.addPass("SSRHitMask")
            .readImage(scene_color, VulkanGraphImageUsage::ShaderRead)
            .readImage(scene_depth, VulkanGraphImageUsage::ShaderRead)
            .writeImage(resources.hit_mask, VulkanGraphImageUsage::ColorAttachment)
            .execute([frame_pass, hit_mask_target, view, settings](VkCommandBuffer command_buffer) {
                return frame_pass->recordTrace(
                    command_buffer,
                    hit_mask_target,
                    view,
                    settings,
                    VulkanSsrOutputMode::HitMask);
            });
        return true;
    }

    VulkanSsrFeatureInput VulkanSsrFeature::getPostProcessInput() const {
        VulkanSsrFeatureInput input;
        if (!m_target.isReady()) {
            return input;
        }
        input.raw_reflection_view = m_target.getRawReflectionImage().view;
        input.hit_mask_view = m_target.getHitMaskImage().view;
        input.sampler = m_target.getSampler();
        return input;
    }

    RendererDebugSsrStats VulkanSsrFeature::buildDebugStats(
        const RenderSsrSettings& settings,
        bool enabled) const {
        RendererDebugSsrStats stats;
        stats.enabled = enabled;
        stats.ready = isReady();
        stats.max_distance = settings.max_distance;
        stats.max_steps = settings.max_steps;
        stats.thickness = settings.thickness;
        stats.stride = settings.stride;
        stats.roughness_fade = settings.roughness_fade;
        stats.edge_fade = settings.edge_fade;
        stats.intensity = settings.intensity;
        if (!m_target.isReady()) {
            return stats;
        }
        const VkExtent2D extent = m_target.getExtent();
        stats.width = extent.width;
        stats.height = extent.height;
        stats.reflection_format =
            VulkanDiagnosticsCollector::vkFormatToString(m_target.getReflectionFormat());
        stats.hit_mask_format =
            VulkanDiagnosticsCollector::vkFormatToString(m_target.getHitMaskFormat());
        return stats;
    }

    VulkanSsrPass& VulkanSsrFeature::getPass(uint32_t frame_index) {
        return m_passes[frame_index % kVulkanFramesInFlight];
    }
} // namespace NexAur
