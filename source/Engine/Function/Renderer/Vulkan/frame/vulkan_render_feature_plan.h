#pragma once

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"

namespace NexAur {
    enum class VulkanFrameOutputRoute {
        Viewport,
        DirectSwapchain
    };

    struct VulkanRenderFeatureAvailability {
        bool viewport_output = false;
        bool post_process = false;
        bool bloom = false;
        bool ao = false;
        bool ssr = false;
        bool smaa = false;
        bool directional_shadow = false;
        bool point_shadow = false;
        bool rect_shadow = false;
    };

    // A frame plan is a value snapshot. It has no mutating API so graph
    // construction, diagnostics, and output routing consume identical choices.
    class NEXAUR_API VulkanRenderFeaturePlan final {
    public:
        static VulkanRenderFeaturePlan build(
            const RenderSettings& settings,
            VulkanRenderFeatureAvailability availability);

        VulkanFrameOutputRoute getOutputRoute() const { return m_output_route; }
        const VulkanRenderFeatureAvailability& getAvailability() const { return m_availability; }
        const RenderEffectDebugSettings& getDebugSettings() const { return m_debug_settings; }
        const RenderEffectDebugSettings& getPostProcessDebugSettings() const {
            return m_post_process_debug_settings;
        }

        bool isolatesForwardDebug() const { return m_isolate_forward_debug; }
        bool rendersAo() const { return m_render_ao; }
        bool rendersSsr() const { return m_render_ssr; }
        bool rendersBloom() const { return m_render_bloom; }
        bool rendersSmaa() const { return m_render_smaa; }

    private:
        VulkanRenderFeaturePlan(
            VulkanFrameOutputRoute output_route,
            VulkanRenderFeatureAvailability availability,
            RenderEffectDebugSettings debug_settings,
            RenderEffectDebugSettings post_process_debug_settings,
            bool isolate_forward_debug,
            bool render_ao,
            bool render_ssr,
            bool render_bloom,
            bool render_smaa);

    private:
        const VulkanFrameOutputRoute m_output_route;
        const VulkanRenderFeatureAvailability m_availability;
        const RenderEffectDebugSettings m_debug_settings;
        const RenderEffectDebugSettings m_post_process_debug_settings;
        const bool m_isolate_forward_debug;
        const bool m_render_ao;
        const bool m_render_ssr;
        const bool m_render_bloom;
        const bool m_render_smaa;
    };
} // namespace NexAur
