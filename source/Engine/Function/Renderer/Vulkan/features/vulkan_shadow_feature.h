#pragma once

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/features/vulkan_render_feature_context.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/passes/vulkan_shadow_pass.h"
#include "Function/Renderer/Vulkan/targets/vulkan_point_shadow_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_shadow_map_target.h"
#include "Function/Renderer/frontend/render_shadow_frame_builder.h"
#include "Function/Renderer/renderer_debug_service.h"

namespace NexAur {
    class VulkanFrameLightingResource;
    class VulkanPassGraph;
    struct VulkanDrawList;

    struct VulkanShadowFeatureFrames {
        RenderShadowCascadeFrame directional;
        RenderPointShadowFrame point;
        RenderRectShadowFrame rect;
    };

    struct VulkanShadowFeatureGraphResources {
        VulkanGraphImageHandle directional_depth;
        VulkanGraphImageHandle point_depth;
        VulkanGraphImageHandle rect_depth;

        bool valid() const {
            return directional_depth.valid() && point_depth.valid() && rect_depth.valid();
        }
    };

    struct VulkanShadowFeatureInput {
        VkImageView directional_view = VK_NULL_HANDLE;
        VkSampler directional_sampler = VK_NULL_HANDLE;
        uint32_t directional_layer_count = 1;
        VkImageView point_view = VK_NULL_HANDLE;
        VkSampler point_sampler = VK_NULL_HANDLE;
        uint32_t point_layer_count = 1;
        VkImageView rect_view = VK_NULL_HANDLE;
        VkSampler rect_sampler = VK_NULL_HANDLE;
        uint32_t rect_layer_count = 1;
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        bool valid() const {
            return directional_view != VK_NULL_HANDLE &&
                   directional_sampler != VK_NULL_HANDLE &&
                   point_view != VK_NULL_HANDLE &&
                   point_sampler != VK_NULL_HANDLE &&
                   rect_view != VK_NULL_HANDLE &&
                   rect_sampler != VK_NULL_HANDLE &&
                   layout != VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };

    class VulkanShadowFeature final {
    public:
        VulkanShadowFeature() = default;
        ~VulkanShadowFeature();

        VulkanShadowFeature(const VulkanShadowFeature&) = delete;
        VulkanShadowFeature& operator=(const VulkanShadowFeature&) = delete;

        bool init(
            const VulkanRenderFeatureContext& context,
            const RenderSettings& settings);
        void shutdown();

        bool prepare(const RenderSettings& settings);
        bool updateLightingResource(VulkanFrameLightingResource& lighting_resource) const;
        VulkanShadowFeatureFrames buildFrames(
            const RenderView& view,
            const VulkanDrawList& draw_list,
            const RenderSettings& settings,
            const RenderEffectDebugSettings& debug_settings) const;

        bool isDirectionalReady() const { return m_directional_target.isReady(); }
        bool isPointReady() const { return m_point_target.isReady(); }
        bool isRectReady() const { return m_rect_target.isReady(); }
        bool isReady() const {
            return m_pass.isReady() &&
                   isDirectionalReady() &&
                   isPointReady() &&
                   isRectReady();
        }

        VulkanShadowFeatureGraphResources addGraphResources(VulkanPassGraph& graph);
        bool addDirectionalPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle depth,
            const VulkanDrawList& draw_list,
            const RenderShadowCascadeFrame& frame);
        bool addPointPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle depth,
            const VulkanDrawList& draw_list,
            const RenderPointShadowFrame& frame);
        bool addRectPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle depth,
            const VulkanDrawList& draw_list,
            const RenderRectShadowFrame& frame);
        bool recordCapturePasses(
            VkCommandBuffer command_buffer,
            const VulkanDrawList& draw_list,
            const VulkanShadowFeatureFrames& frames,
            const RenderSettings& settings);

        float getDirectionalMapSize() const;
        float getPointMapSize() const;
        float getRectMapSize() const;
        uint32_t getPointLightCapacity() const;
        uint32_t getRectLightCapacity() const;
        VulkanShadowFeatureInput getPostProcessInput() const;

        RendererDebugShadowTargetStats buildDirectionalDebugStats() const;
        RendererDebugShadowTargetStats buildPointDebugStats() const;
        RendererDebugShadowTargetStats buildRectDebugStats() const;

    private:
        bool ensureDirectionalTarget(const RenderShadowSettings& settings);
        bool ensurePointTarget(const RenderPointShadowSettings& settings);
        bool ensureRectTarget(const RenderRectShadowSettings& settings);
        VulkanGraphImageHandle addDepthImage(
            VulkanPassGraph& graph,
            const char* name,
            VulkanShadowMapTarget& target);
        VulkanGraphImageHandle addDepthImage(
            VulkanPassGraph& graph,
            const char* name,
            VulkanPointShadowTarget& target);
        void transitionDepthToAttachment(
            VkCommandBuffer command_buffer,
            VkImage image,
            VkImageLayout old_layout,
            uint32_t layer_count) const;
        void transitionDepthToShaderRead(
            VkCommandBuffer command_buffer,
            VkImage image,
            VkImageLayout old_layout,
            uint32_t layer_count) const;

    private:
        VulkanRenderFeatureContext m_context;
        RenderShadowFrameBuilder m_frame_builder;
        VulkanShadowPass m_pass;
        VulkanShadowMapTarget m_directional_target;
        VulkanPointShadowTarget m_point_target;
        VulkanShadowMapTarget m_rect_target;
    };
} // namespace NexAur
