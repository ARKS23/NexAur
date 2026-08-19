#include "pch.h"
#include "vulkan_post_process_feature.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

#include <algorithm>

namespace NexAur {
    namespace {
        const char* toneMappingModeToText(RenderToneMappingMode mode) {
            return mode == RenderToneMappingMode::None ? "None" : "ACES";
        }
    } // namespace

    VulkanPostProcessFeature::~VulkanPostProcessFeature() {
        shutdown();
    }

    bool VulkanPostProcessFeature::init(
        const VulkanRenderFeatureContext& context,
        VkFormat output_format) {
        shutdown();
        if (!context.valid()) {
            return false;
        }
        m_context = context;
        if (!recreateSwapchainResources(output_format)) {
            shutdown();
            return false;
        }
        return true;
    }

    void VulkanPostProcessFeature::shutdown() {
        for (VulkanPostProcessPass& pass : m_passes) {
            pass.shutdown();
        }
        m_context = {};
    }

    void VulkanPostProcessFeature::cleanupSwapchainResources() {
        for (VulkanPostProcessPass& pass : m_passes) {
            pass.cleanupResources();
        }
    }

    bool VulkanPostProcessFeature::recreateSwapchainResources(VkFormat output_format) {
        if (!m_context.valid()) {
            return true;
        }
        if (output_format == VK_FORMAT_UNDEFINED) {
            return false;
        }
        VulkanPostProcessPassContext context;
        context.device = m_context.resources.device;
        context.output_color_format = output_format;
        context.input_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::PostProcessInput);
        context.descriptor_allocator = m_context.descriptor_allocator;
        context.pipeline_cache = m_context.pipeline_cache;
        for (VulkanPostProcessPass& pass : m_passes) {
            if (!pass.recreateResources(context)) {
                cleanupSwapchainResources();
                return false;
            }
        }
        return true;
    }

    bool VulkanPostProcessFeature::isReady() const {
        return std::all_of(
            m_passes.begin(),
            m_passes.end(),
            [](const VulkanPostProcessPass& pass) { return pass.isReady(); });
    }

    bool VulkanPostProcessFeature::addPass(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle input_color,
        VulkanGraphImageHandle output_color,
        VulkanGraphImageHandle scene_depth,
        VulkanGraphImageHandle ao_raw,
        VulkanGraphImageHandle ao_blurred,
        VulkanGraphImageHandle ssr_raw_reflection,
        VulkanGraphImageHandle ssr_hit_mask,
        const VulkanPostProcessRenderTarget& target,
        const VulkanPostProcessInput& input,
        const RenderPostProcessSettings& post_process_settings,
        const RenderAoSettings& ao_settings,
        const RenderSsrSettings& ssr_settings,
        const RenderRayTracedReflectionSettings& ray_traced_reflection_settings,
        const RenderEffectDebugSettings& debug_settings,
        bool isolate_forward_debug,
        uint32_t frame_index) {
        VulkanPostProcessPass& pass = getPass(frame_index);
        if (!input_color.valid() ||
            !output_color.valid() ||
            !scene_depth.valid() ||
            !ao_raw.valid() ||
            !ao_blurred.valid() ||
            !ssr_raw_reflection.valid() ||
            !ssr_hit_mask.valid() ||
            !target.valid() ||
            !input.valid() ||
            !pass.isReady() ||
            !pass.updateInput(input)) {
            return false;
        }
        VulkanPostProcessPass* frame_pass = &pass;

        graph.addPass("PostProcess")
            .readImage(input_color, VulkanGraphImageUsage::ShaderRead)
            .readImage(scene_depth, VulkanGraphImageUsage::ShaderRead)
            .readImage(ao_raw, VulkanGraphImageUsage::ShaderRead)
            .readImage(ao_blurred, VulkanGraphImageUsage::ShaderRead)
            .readImage(ssr_raw_reflection, VulkanGraphImageUsage::ShaderRead)
            .readImage(ssr_hit_mask, VulkanGraphImageUsage::ShaderRead)
            .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
            .execute([frame_pass, target, post_process_settings, ao_settings, ssr_settings, ray_traced_reflection_settings, debug_settings, isolate_forward_debug](VkCommandBuffer command_buffer) {
                return frame_pass->record(
                    command_buffer,
                    target,
                    post_process_settings,
                    ao_settings,
                    ssr_settings,
                    ray_traced_reflection_settings,
                    debug_settings,
                    isolate_forward_debug);
            });
        return true;
    }

    RendererDebugPostProcessStats VulkanPostProcessFeature::buildDebugStats(
        const RenderPostProcessSettings& settings,
        bool enabled,
        bool bloom_enabled) const {
        RendererDebugPostProcessStats stats;
        stats.enabled = enabled;
        stats.ready = isReady();
        const VkFormat output_format = m_passes.front().getOutputColorFormat();
        if (output_format != VK_FORMAT_UNDEFINED) {
            stats.output_format =
                VulkanDiagnosticsCollector::vkFormatToString(output_format);
        }
        stats.tone_mapping = toneMappingModeToText(settings.tone_mapping_mode);
        stats.exposure = settings.exposure;
        stats.bloom_enabled = bloom_enabled;
        stats.bloom_intensity = settings.bloom_intensity;
        stats.color_grading_enabled = settings.color_grading_enabled;
        stats.color_grading_exposure_offset = settings.color_grading_exposure_offset;
        stats.color_grading_contrast = settings.color_grading_contrast;
        stats.color_grading_saturation = settings.color_grading_saturation;
        stats.color_grading_temperature = settings.color_grading_temperature;
        stats.color_grading_tint = settings.color_grading_tint;
        stats.color_grading_black_point = settings.color_grading_black_point;
        stats.color_grading_white_point = settings.color_grading_white_point;
        stats.vignette_intensity = settings.vignette_intensity;
        stats.sharpen_intensity = settings.sharpen_intensity;
        return stats;
    }

    VulkanPostProcessPass& VulkanPostProcessFeature::getPass(uint32_t frame_index) {
        return m_passes[frame_index % kVulkanFramesInFlight];
    }
} // namespace NexAur
