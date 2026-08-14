#include "pch.h"
#include "vulkan_shadow_feature.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"
#include "Function/Renderer/Vulkan/resources/vulkan_frame_lighting_resource.h"

namespace NexAur {
    namespace {
        constexpr uint32_t kDefaultShadowMapResolution = 2048u;

        uint32_t sanitizeDirectionalResolution(uint32_t resolution) {
            if (resolution >= 4096u) {
                return 4096u;
            }
            if (resolution >= 2048u) {
                return 2048u;
            }
            return 1024u;
        }

        uint32_t sanitizePointResolution(uint32_t resolution) {
            if (resolution >= 1024u) {
                return 1024u;
            }
            if (resolution >= 512u) {
                return 512u;
            }
            return 256u;
        }

        uint32_t sanitizeRectResolution(uint32_t resolution) {
            if (resolution >= 2048u) {
                return 2048u;
            }
            if (resolution >= 1024u) {
                return 1024u;
            }
            return 512u;
        }

        uint32_t sanitizeCascadeCount(const RenderShadowSettings& settings) {
            return settings.cascades_enabled ?
                std::clamp(settings.cascade_count, 1u, kMaxRenderShadowCascadeCount) :
                1u;
        }

        RendererDebugShadowTargetStats buildDebugStats(
            bool ready,
            VkExtent2D extent,
            uint32_t layer_count,
            VkFormat format) {
            RendererDebugShadowTargetStats stats;
            stats.ready = ready;
            if (!ready) {
                return stats;
            }
            stats.width = extent.width;
            stats.height = extent.height;
            stats.layer_count = layer_count;
            stats.depth_format = VulkanDiagnosticsCollector::vkFormatToString(format);
            return stats;
        }
    } // namespace

    VulkanShadowFeature::~VulkanShadowFeature() {
        shutdown();
    }

    bool VulkanShadowFeature::init(
        const VulkanRenderFeatureContext& context,
        const RenderSettings& settings) {
        shutdown();
        if (!context.valid()) {
            return false;
        }
        m_context = context;

        RenderShadowSettings directional_settings = settings.shadow;
        if (directional_settings.map_resolution == 0) {
            directional_settings.map_resolution = kDefaultShadowMapResolution;
        }
        if (!ensureDirectionalTarget(directional_settings) ||
            !ensurePointTarget(settings.point_shadow) ||
            !ensureRectTarget(settings.rect_shadow)) {
            shutdown();
            return false;
        }

        VulkanShadowPassContext pass_context;
        pass_context.device = context.resources.device;
        pass_context.depth_format = m_directional_target.getDepthFormat();
        pass_context.pipeline_cache = context.pipeline_cache;
        if (!m_pass.init(pass_context)) {
            shutdown();
            return false;
        }
        return true;
    }

    void VulkanShadowFeature::shutdown() {
        m_pass.shutdown();
        m_rect_target.shutdown();
        m_point_target.shutdown();
        m_directional_target.shutdown();
        m_context = {};
    }

    bool VulkanShadowFeature::prepare(const RenderSettings& settings) {
        return ensureDirectionalTarget(settings.shadow) &&
               ensurePointTarget(settings.point_shadow) &&
               ensureRectTarget(settings.rect_shadow);
    }

    bool VulkanShadowFeature::updateLightingResource(
        VulkanFrameLightingResource& lighting_resource) const {
        return isReady() &&
               lighting_resource.updateShadowMap(
                   m_directional_target.getDepthImageView(),
                   m_directional_target.getSampler()) &&
               lighting_resource.updatePointShadowMap(
                   m_point_target.getDepthImageView(),
                   m_point_target.getSampler()) &&
               lighting_resource.updateRectShadowMap(
                   m_rect_target.getDepthImageView(),
                   m_rect_target.getSampler());
    }

    VulkanShadowFeatureFrames VulkanShadowFeature::buildFrames(
        const RenderView& view,
        const VulkanDrawList& draw_list,
        const RenderSettings& settings,
        const RenderEffectDebugSettings& debug_settings) const {
        VulkanShadowFeatureFrames frames;
        if (m_directional_target.isReady()) {
            frames.directional = m_frame_builder.buildDirectionalShadowFrame(
                view,
                draw_list.directional_light,
                settings.shadow,
                debug_settings,
                getDirectionalMapSize());
        }
        frames.point = m_frame_builder.buildPointShadowFrame(
            draw_list.point_lights,
            settings.point_shadow,
            getPointLightCapacity());
        frames.rect = m_frame_builder.buildRectShadowFrame(
            draw_list.rect_lights,
            settings.rect_shadow,
            getRectLightCapacity());
        return frames;
    }

    VulkanShadowFeatureGraphResources VulkanShadowFeature::addGraphResources(
        VulkanPassGraph& graph) {
        VulkanShadowFeatureGraphResources resources;
        resources.directional_depth = addDepthImage(graph, "ShadowDepth", m_directional_target);
        resources.point_depth = addDepthImage(graph, "PointShadowDepth", m_point_target);
        resources.rect_depth = addDepthImage(graph, "RectShadowDepth", m_rect_target);
        return resources;
    }

    bool VulkanShadowFeature::addDirectionalPass(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle depth,
        const VulkanDrawList& draw_list,
        const RenderShadowCascadeFrame& frame) {
        if (!depth.valid() || !m_directional_target.isReady()) {
            return false;
        }
        const uint32_t cascade_count = std::clamp(
            frame.cascade_count,
            1u,
            std::min(m_directional_target.getLayerCount(), kMaxRenderShadowCascadeCount));
        graph.addPass("DirectionalShadowMap")
            .writeImage(depth, VulkanGraphImageUsage::DepthStencilAttachment)
            .execute([this, &draw_list, frame, cascade_count](VkCommandBuffer command_buffer) {
                for (uint32_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index) {
                    if (!m_pass.record(
                            command_buffer,
                            m_directional_target.getRenderTarget(cascade_index),
                            draw_list,
                            frame.light_view_projections[cascade_index])) {
                        return false;
                    }
                }
                return true;
            });
        return true;
    }

    bool VulkanShadowFeature::addPointPass(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle depth,
        const VulkanDrawList& draw_list,
        const RenderPointShadowFrame& frame) {
        if (!depth.valid() || !m_point_target.isReady()) {
            return false;
        }
        graph.addPass("PointShadowMap")
            .writeImage(depth, VulkanGraphImageUsage::DepthStencilAttachment)
            .execute([this, &draw_list, frame](VkCommandBuffer command_buffer) {
                if (!frame.enabled || frame.face_count == 0) {
                    return true;
                }
                const uint32_t face_count = std::min(frame.face_count, m_point_target.getLayerCount());
                for (uint32_t layer_index = 0; layer_index < face_count; ++layer_index) {
                    if (!m_pass.record(
                            command_buffer,
                            m_point_target.getRenderTarget(layer_index),
                            draw_list,
                            frame.light_view_projections[layer_index])) {
                        return false;
                    }
                }
                return true;
            });
        return true;
    }

    bool VulkanShadowFeature::addRectPass(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle depth,
        const VulkanDrawList& draw_list,
        const RenderRectShadowFrame& frame) {
        if (!depth.valid() || !m_rect_target.isReady()) {
            return false;
        }
        graph.addPass("RectShadowMap")
            .writeImage(depth, VulkanGraphImageUsage::DepthStencilAttachment)
            .execute([this, &draw_list, frame](VkCommandBuffer command_buffer) {
                if (!frame.enabled || frame.shadowed_light_count == 0) {
                    return true;
                }
                const uint32_t layer_count =
                    std::min(frame.shadowed_light_count, m_rect_target.getLayerCount());
                for (uint32_t layer_index = 0; layer_index < layer_count; ++layer_index) {
                    if (!m_pass.record(
                            command_buffer,
                            m_rect_target.getRenderTarget(layer_index),
                            draw_list,
                            frame.light_view_projections[layer_index])) {
                        return false;
                    }
                }
                return true;
            });
        return true;
    }

    bool VulkanShadowFeature::recordCapturePasses(
        VkCommandBuffer command_buffer,
        const VulkanDrawList& draw_list,
        const VulkanShadowFeatureFrames& frames,
        const RenderSettings& settings) {
        if (m_directional_target.isReady()) {
            const bool enabled = settings.shadow.enabled && draw_list.directional_light.cast_shadow;
            if (enabled) {
                transitionDepthToAttachment(
                    command_buffer,
                    m_directional_target.getDepthImage(),
                    m_directional_target.getDepthLayout(),
                    m_directional_target.getLayerCount());
                m_directional_target.setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                const uint32_t cascade_count = std::clamp(
                    frames.directional.cascade_count,
                    1u,
                    std::min(m_directional_target.getLayerCount(), kMaxRenderShadowCascadeCount));
                for (uint32_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index) {
                    if (!m_pass.record(
                            command_buffer,
                            m_directional_target.getRenderTarget(cascade_index),
                            draw_list,
                            frames.directional.light_view_projections[cascade_index])) {
                        return false;
                    }
                }
            }
            transitionDepthToShaderRead(
                command_buffer,
                m_directional_target.getDepthImage(),
                m_directional_target.getDepthLayout(),
                m_directional_target.getLayerCount());
            m_directional_target.setDepthLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        if (m_point_target.isReady()) {
            if (frames.point.enabled && frames.point.face_count > 0) {
                transitionDepthToAttachment(
                    command_buffer,
                    m_point_target.getDepthImage(),
                    m_point_target.getDepthLayout(),
                    m_point_target.getLayerCount());
                m_point_target.setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                const uint32_t face_count =
                    std::min(frames.point.face_count, m_point_target.getLayerCount());
                for (uint32_t layer_index = 0; layer_index < face_count; ++layer_index) {
                    if (!m_pass.record(
                            command_buffer,
                            m_point_target.getRenderTarget(layer_index),
                            draw_list,
                            frames.point.light_view_projections[layer_index])) {
                        return false;
                    }
                }
            }
            transitionDepthToShaderRead(
                command_buffer,
                m_point_target.getDepthImage(),
                m_point_target.getDepthLayout(),
                m_point_target.getLayerCount());
            m_point_target.setDepthLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        if (m_rect_target.isReady()) {
            if (frames.rect.enabled && frames.rect.shadowed_light_count > 0) {
                transitionDepthToAttachment(
                    command_buffer,
                    m_rect_target.getDepthImage(),
                    m_rect_target.getDepthLayout(),
                    m_rect_target.getLayerCount());
                m_rect_target.setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                const uint32_t layer_count =
                    std::min(frames.rect.shadowed_light_count, m_rect_target.getLayerCount());
                for (uint32_t layer_index = 0; layer_index < layer_count; ++layer_index) {
                    if (!m_pass.record(
                            command_buffer,
                            m_rect_target.getRenderTarget(layer_index),
                            draw_list,
                            frames.rect.light_view_projections[layer_index])) {
                        return false;
                    }
                }
            }
            transitionDepthToShaderRead(
                command_buffer,
                m_rect_target.getDepthImage(),
                m_rect_target.getDepthLayout(),
                m_rect_target.getLayerCount());
            m_rect_target.setDepthLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        return true;
    }

    float VulkanShadowFeature::getDirectionalMapSize() const {
        return m_directional_target.isReady() ?
            static_cast<float>(m_directional_target.getExtent().width) : 1.0f;
    }

    float VulkanShadowFeature::getPointMapSize() const {
        return m_point_target.isReady() ?
            static_cast<float>(m_point_target.getExtent().width) : 1.0f;
    }

    float VulkanShadowFeature::getRectMapSize() const {
        return m_rect_target.isReady() ?
            static_cast<float>(m_rect_target.getExtent().width) : 1.0f;
    }

    uint32_t VulkanShadowFeature::getPointLightCapacity() const {
        return m_point_target.isReady() ? m_point_target.getShadowedLightCapacity() : 0u;
    }

    uint32_t VulkanShadowFeature::getRectLightCapacity() const {
        return m_rect_target.isReady() ? m_rect_target.getLayerCount() : 0u;
    }

    VulkanShadowFeatureInput VulkanShadowFeature::getPostProcessInput() const {
        VulkanShadowFeatureInput input;
        if (!isReady()) {
            return input;
        }
        input.directional_view = m_directional_target.getDepthImageView();
        input.directional_sampler = m_directional_target.getSampler();
        input.directional_layer_count = m_directional_target.getLayerCount();
        input.point_view = m_point_target.getDepthImageView();
        input.point_sampler = m_point_target.getSampler();
        input.point_layer_count = m_point_target.getLayerCount();
        input.rect_view = m_rect_target.getDepthImageView();
        input.rect_sampler = m_rect_target.getSampler();
        input.rect_layer_count = m_rect_target.getLayerCount();
        return input;
    }

    RendererDebugShadowTargetStats VulkanShadowFeature::buildDirectionalDebugStats() const {
        return buildDebugStats(
            m_directional_target.isReady(),
            m_directional_target.getExtent(),
            m_directional_target.getLayerCount(),
            m_directional_target.getDepthFormat());
    }

    RendererDebugShadowTargetStats VulkanShadowFeature::buildPointDebugStats() const {
        return buildDebugStats(
            m_point_target.isReady(),
            m_point_target.getExtent(),
            m_point_target.getLayerCount(),
            m_point_target.getDepthFormat());
    }

    RendererDebugShadowTargetStats VulkanShadowFeature::buildRectDebugStats() const {
        return buildDebugStats(
            m_rect_target.isReady(),
            m_rect_target.getExtent(),
            m_rect_target.getLayerCount(),
            m_rect_target.getDepthFormat());
    }

    bool VulkanShadowFeature::ensureDirectionalTarget(
        const RenderShadowSettings& settings) {
        const uint32_t resolution = sanitizeDirectionalResolution(settings.map_resolution);
        const uint32_t layer_count = sanitizeCascadeCount(settings);
        if (m_directional_target.isReady() &&
            m_directional_target.getExtent().width == resolution &&
            m_directional_target.getLayerCount() == layer_count) {
            return true;
        }
        if (m_context.resources.device == VK_NULL_HANDLE) {
            return false;
        }
        vkDeviceWaitIdle(m_context.resources.device);
        m_directional_target.shutdown();
        return m_directional_target.init(m_context.resources, resolution, layer_count);
    }

    bool VulkanShadowFeature::ensurePointTarget(
        const RenderPointShadowSettings& settings) {
        const uint32_t resolution = sanitizePointResolution(settings.map_resolution);
        const uint32_t capacity = std::clamp(
            std::max(1u, settings.max_shadowed_lights),
            1u,
            kMaxRenderPointShadowLights);
        if (m_point_target.isReady() &&
            m_point_target.getExtent().width == resolution &&
            m_point_target.getShadowedLightCapacity() == capacity) {
            return true;
        }
        if (m_context.resources.device == VK_NULL_HANDLE) {
            return false;
        }
        vkDeviceWaitIdle(m_context.resources.device);
        m_point_target.shutdown();
        return m_point_target.init(m_context.resources, resolution, capacity);
    }

    bool VulkanShadowFeature::ensureRectTarget(
        const RenderRectShadowSettings& settings) {
        const uint32_t resolution = sanitizeRectResolution(settings.map_resolution);
        const uint32_t capacity = std::clamp(
            std::max(1u, settings.max_shadowed_lights),
            1u,
            kMaxRenderRectShadowLights);
        if (m_rect_target.isReady() &&
            m_rect_target.getExtent().width == resolution &&
            m_rect_target.getLayerCount() == capacity) {
            return true;
        }
        if (m_context.resources.device == VK_NULL_HANDLE) {
            return false;
        }
        vkDeviceWaitIdle(m_context.resources.device);
        m_rect_target.shutdown();
        return m_rect_target.init(m_context.resources, resolution, capacity);
    }

    VulkanGraphImageHandle VulkanShadowFeature::addDepthImage(
        VulkanPassGraph& graph,
        const char* name,
        VulkanShadowMapTarget& target) {
        if (!target.isReady()) {
            return {};
        }
        VulkanGraphImageDesc desc;
        desc.name = name;
        desc.image = target.getDepthImage();
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        desc.subresource_range.layer_count = target.getLayerCount();
        desc.initial_layout = target.getDepthLayout();
        desc.commit_layout = [&target](VkImageLayout layout) {
            target.setDepthLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    VulkanGraphImageHandle VulkanShadowFeature::addDepthImage(
        VulkanPassGraph& graph,
        const char* name,
        VulkanPointShadowTarget& target) {
        if (!target.isReady()) {
            return {};
        }
        VulkanGraphImageDesc desc;
        desc.name = name;
        desc.image = target.getDepthImage();
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        desc.subresource_range.layer_count = target.getLayerCount();
        desc.initial_layout = target.getDepthLayout();
        desc.commit_layout = [&target](VkImageLayout layout) {
            target.setDepthLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    void VulkanShadowFeature::transitionDepthToAttachment(
        VkCommandBuffer command_buffer,
        VkImage image,
        VkImageLayout old_layout,
        uint32_t layer_count) const {
        if (old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            return;
        }
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = layer_count;
        barrier.srcAccessMask = old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ?
            VK_ACCESS_SHADER_READ_BIT : 0;
        barrier.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(
            command_buffer,
            old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ?
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
    }

    void VulkanShadowFeature::transitionDepthToShaderRead(
        VkCommandBuffer command_buffer,
        VkImage image,
        VkImageLayout old_layout,
        uint32_t layer_count) const {
        if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            return;
        }
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = layer_count;
        barrier.srcAccessMask = old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ?
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            command_buffer,
            old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ?
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
    }
} // namespace NexAur
