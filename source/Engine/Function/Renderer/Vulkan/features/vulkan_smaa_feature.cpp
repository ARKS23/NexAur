#include "pch.h"
#include "vulkan_smaa_feature.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

#include <algorithm>

namespace NexAur {
    VulkanSmaaFeature::~VulkanSmaaFeature() {
        shutdown();
    }

    bool VulkanSmaaFeature::init(
        const VulkanRenderFeatureContext& context,
        VkFormat source_format,
        VkFormat mask_format,
        VkFormat output_format,
        uint32_t width,
        uint32_t height) {
        shutdown();
        if (!context.valid() ||
            source_format == VK_FORMAT_UNDEFINED ||
            mask_format == VK_FORMAT_UNDEFINED ||
            output_format == VK_FORMAT_UNDEFINED) {
            return false;
        }
        m_context = context;
        if (!m_target.init(context.resources, source_format, mask_format, width, height) ||
            !recreateSwapchainResources(output_format)) {
            shutdown();
            return false;
        }
        return true;
    }

    void VulkanSmaaFeature::shutdown() {
        for (VulkanSmaaPass& pass : m_passes) {
            pass.shutdown();
        }
        m_target.shutdown();
        m_context = {};
        m_output_format = VK_FORMAT_UNDEFINED;
    }

    void VulkanSmaaFeature::cleanupSwapchainResources() {
        for (VulkanSmaaPass& pass : m_passes) {
            pass.cleanupResources();
        }
    }

    bool VulkanSmaaFeature::recreateSwapchainResources(VkFormat output_format) {
        m_output_format = output_format;
        if (!m_target.isReady()) {
            return true;
        }
        VulkanSmaaPassContext context;
        context.device = m_context.resources.device;
        context.source_color_format = m_target.getSourceFormat();
        context.mask_color_format = m_target.getMaskFormat();
        context.output_color_format = output_format;
        context.single_input_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::AoInput);
        context.dual_input_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::BloomDualInput);
        context.descriptor_allocator = m_context.descriptor_allocator;
        context.pipeline_cache = m_context.pipeline_cache;
        for (VulkanSmaaPass& pass : m_passes) {
            if (!pass.recreateResources(context)) {
                cleanupSwapchainResources();
                return false;
            }
        }
        return true;
    }

    bool VulkanSmaaFeature::resize(uint32_t width, uint32_t height) {
        if (!m_target.resize(width, height)) {
            return false;
        }
        return recreateSwapchainResources(m_output_format);
    }

    bool VulkanSmaaFeature::isReady() const {
        return m_target.isReady() &&
               std::all_of(
                   m_passes.begin(),
                   m_passes.end(),
                   [](const VulkanSmaaPass& pass) { return pass.isReady(); });
    }

    VulkanGraphImageHandle VulkanSmaaFeature::addSourceImage(VulkanPassGraph& graph) {
        const VulkanSmaaImageView& image = m_target.getSourceImage();
        if (!image.valid()) {
            return {};
        }
        VulkanGraphImageDesc desc;
        desc.name = "SMAASource";
        desc.image = image.image;
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.initial_layout = image.layout;
        desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setSourceLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    bool VulkanSmaaFeature::addPasses(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle source_color,
        VulkanGraphImageHandle output_color,
        const VulkanSmaaRenderTarget& output_target,
        const RenderAntiAliasingSettings& settings,
        const RenderEffectDebugSettings& debug_settings,
        uint32_t frame_index) {
        if (!isReady() ||
            !source_color.valid() ||
            !output_color.valid() ||
            !output_target.valid()) {
            return false;
        }

        const VulkanGraphImageHandle edge_color = addEdgeImage(graph);
        if (!edge_color.valid()) {
            return false;
        }
        const VulkanSmaaInput source_input = makeInput(m_target.getSourceImage());
        const VulkanSmaaInput edge_input = makeInput(m_target.getEdgeImage());
        const VulkanSmaaInput blend_input = makeInput(m_target.getBlendImage());
        const VulkanSmaaRenderTarget edge_target = m_target.getEdgeRenderTarget();
        VulkanSmaaPass* frame_pass = &getPass(frame_index);
        graph.addPass("SMAAEdge")
            .readImage(source_color, VulkanGraphImageUsage::ShaderRead)
            .writeImage(edge_color, VulkanGraphImageUsage::ColorAttachment)
            .execute([frame_pass, source_input, edge_input, blend_input, edge_target, settings](VkCommandBuffer command_buffer) {
                if (!frame_pass->updateInputs(source_input, edge_input, blend_input)) {
                    return false;
                }
                return frame_pass->recordEdge(command_buffer, edge_target, settings);
            });

        if (debug_settings.view == RenderEffectDebugView::SmaaEdgeMask) {
            graph.addPass("SMAAEdgeDebug")
                .readImage(edge_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([frame_pass, output_target, edge_input](VkCommandBuffer command_buffer) {
                    return frame_pass->recordDebugResolve(command_buffer, output_target, edge_input);
                });
            return true;
        }

        const VulkanGraphImageHandle blend_color = addBlendImage(graph);
        if (!blend_color.valid()) {
            return false;
        }
        const VulkanSmaaRenderTarget blend_target = m_target.getBlendRenderTarget();
        graph.addPass("SMAABlend")
            .readImage(edge_color, VulkanGraphImageUsage::ShaderRead)
            .writeImage(blend_color, VulkanGraphImageUsage::ColorAttachment)
            .execute([frame_pass, blend_target, settings](VkCommandBuffer command_buffer) {
                return frame_pass->recordBlend(command_buffer, blend_target, settings);
            });

        if (debug_settings.view == RenderEffectDebugView::SmaaBlendWeight) {
            graph.addPass("SMAABlendDebug")
                .readImage(blend_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([frame_pass, output_target, blend_input](VkCommandBuffer command_buffer) {
                    return frame_pass->recordDebugResolve(command_buffer, output_target, blend_input);
                });
            return true;
        }

        graph.addPass("SMAANeighborhood")
            .readImage(source_color, VulkanGraphImageUsage::ShaderRead)
            .readImage(blend_color, VulkanGraphImageUsage::ShaderRead)
            .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
            .execute([frame_pass, output_target, settings](VkCommandBuffer command_buffer) {
                return frame_pass->recordNeighborhood(command_buffer, output_target, settings);
            });
        return true;
    }

    RendererDebugSmaaStats VulkanSmaaFeature::buildDebugStats(
        const RenderAntiAliasingSettings& settings,
        bool enabled) const {
        RendererDebugSmaaStats stats;
        stats.enabled = enabled;
        stats.ready = isReady();
        stats.mode = settings.mode == RenderAntiAliasingMode::None ? "None" : "SMAA";
        stats.edge_threshold = settings.smaa_edge_threshold;
        stats.contrast_factor = settings.smaa_contrast_factor;
        stats.max_search_steps = settings.smaa_max_search_steps;
        stats.blend_strength = settings.smaa_blend_strength;
        if (!m_target.isReady()) {
            return stats;
        }
        const VkExtent2D extent = m_target.getExtent();
        stats.width = extent.width;
        stats.height = extent.height;
        stats.source_format = VulkanDiagnosticsCollector::vkFormatToString(m_target.getSourceFormat());
        stats.edge_format = VulkanDiagnosticsCollector::vkFormatToString(m_target.getMaskFormat());
        stats.blend_format = VulkanDiagnosticsCollector::vkFormatToString(m_target.getMaskFormat());
        return stats;
    }

    VulkanGraphImageHandle VulkanSmaaFeature::addEdgeImage(VulkanPassGraph& graph) {
        const VulkanSmaaImageView& image = m_target.getEdgeImage();
        VulkanGraphImageDesc desc;
        desc.name = "SMAAEdge";
        desc.image = image.image;
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.initial_layout = image.layout;
        desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setEdgeLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    VulkanGraphImageHandle VulkanSmaaFeature::addBlendImage(VulkanPassGraph& graph) {
        const VulkanSmaaImageView& image = m_target.getBlendImage();
        VulkanGraphImageDesc desc;
        desc.name = "SMAABlend";
        desc.image = image.image;
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.initial_layout = image.layout;
        desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setBlendLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    VulkanSmaaInput VulkanSmaaFeature::makeInput(const VulkanSmaaImageView& image) const {
        VulkanSmaaInput input;
        input.view = image.view;
        input.sampler = m_target.getSampler();
        input.extent = image.extent;
        input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return input;
    }

    VulkanSmaaPass& VulkanSmaaFeature::getPass(uint32_t frame_index) {
        return m_passes[frame_index % kVulkanFramesInFlight];
    }
} // namespace NexAur
