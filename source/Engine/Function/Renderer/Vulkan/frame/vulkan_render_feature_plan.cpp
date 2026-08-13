#include "pch.h"
#include "vulkan_render_feature_plan.h"

namespace NexAur {
    namespace {
        bool isBloomDebugView(RenderEffectDebugView view) {
            return view == RenderEffectDebugView::BloomComposite ||
                   view == RenderEffectDebugView::BloomDownsampleMip ||
                   view == RenderEffectDebugView::BloomUpsampleMip;
        }

        bool isAoDebugView(RenderEffectDebugView view) {
            return view == RenderEffectDebugView::AoRaw ||
                   view == RenderEffectDebugView::AoBlurred;
        }

        bool isSmaaDebugView(RenderEffectDebugView view) {
            return view == RenderEffectDebugView::SmaaEdgeMask ||
                   view == RenderEffectDebugView::SmaaBlendWeight ||
                   view == RenderEffectDebugView::SmaaOutput;
        }

        bool isSsrDebugView(RenderEffectDebugView view) {
            return view == RenderEffectDebugView::SsrHitMask ||
                   view == RenderEffectDebugView::SsrRaySteps ||
                   view == RenderEffectDebugView::SsrRawReflection ||
                   view == RenderEffectDebugView::SsrSurfaceMask;
        }

        RenderEffectDebugView resolveDebugView(
            RenderEffectDebugView requested_view,
            const VulkanRenderFeatureAvailability& availability) {
            if (isBloomDebugView(requested_view) && !availability.bloom) {
                return RenderEffectDebugView::FinalLit;
            }
            if (isAoDebugView(requested_view) && !availability.ao) {
                return RenderEffectDebugView::FinalLit;
            }
            if (isSsrDebugView(requested_view) && !availability.ssr) {
                return RenderEffectDebugView::FinalLit;
            }
            if (isSmaaDebugView(requested_view) && !availability.smaa) {
                return RenderEffectDebugView::FinalLit;
            }
            if ((requested_view == RenderEffectDebugView::ShadowMap ||
                 requested_view == RenderEffectDebugView::ShadowCascades) &&
                !availability.directional_shadow) {
                return RenderEffectDebugView::FinalLit;
            }
            if (requested_view == RenderEffectDebugView::PointShadowMap &&
                !availability.point_shadow) {
                return RenderEffectDebugView::FinalLit;
            }
            if (requested_view == RenderEffectDebugView::RectShadowMap &&
                !availability.rect_shadow) {
                return RenderEffectDebugView::FinalLit;
            }
            return requested_view;
        }
    } // namespace

    VulkanRenderFeaturePlan VulkanRenderFeaturePlan::build(
        const RenderSettings& settings,
        VulkanRenderFeatureAvailability availability) {
        RenderEffectDebugSettings debug_settings = settings.effects_debug;
        debug_settings.view = resolveDebugView(debug_settings.view, availability);
        const RenderEffectDebugView debug_view = debug_settings.view;
        const bool isolate_forward_debug =
            settings.ibl_debug.mode != RenderIblDebugMode::FinalLit &&
            debug_view == RenderEffectDebugView::FinalLit;

        const bool final_bloom_output =
            debug_view == RenderEffectDebugView::FinalLit ||
            debug_view == RenderEffectDebugView::ShadowCascades ||
            debug_view == RenderEffectDebugView::PostToneMap ||
            debug_view == RenderEffectDebugView::ColorGraded ||
            isSmaaDebugView(debug_view);
        const bool render_bloom =
            !isolate_forward_debug &&
            availability.bloom &&
            ((final_bloom_output &&
              settings.post_process.bloom_enabled &&
              settings.post_process.bloom_intensity > 0.0f) ||
             isBloomDebugView(debug_view));
        const bool render_ao =
            !isolate_forward_debug &&
            availability.ao &&
            (settings.ao.enabled || isAoDebugView(debug_view));
        const bool render_ssr =
            !isolate_forward_debug &&
            availability.ssr &&
            (settings.ssr.enabled || isSsrDebugView(debug_view));
        const bool render_smaa =
            !isolate_forward_debug &&
            availability.smaa &&
            ((debug_view == RenderEffectDebugView::FinalLit &&
              settings.anti_aliasing.mode == RenderAntiAliasingMode::SMAA) ||
             isSmaaDebugView(debug_view));

        RenderEffectDebugSettings post_process_debug_settings = debug_settings;
        if (isSmaaDebugView(post_process_debug_settings.view)) {
            post_process_debug_settings.view = RenderEffectDebugView::ColorGraded;
        }

        return VulkanRenderFeaturePlan(
            availability.viewport_output ?
                VulkanFrameOutputRoute::Viewport :
                VulkanFrameOutputRoute::DirectSwapchain,
            availability,
            debug_settings,
            post_process_debug_settings,
            isolate_forward_debug,
            render_ao,
            render_ssr,
            render_bloom,
            render_smaa);
    }

    VulkanRenderFeaturePlan::VulkanRenderFeaturePlan(
        VulkanFrameOutputRoute output_route,
        VulkanRenderFeatureAvailability availability,
        RenderEffectDebugSettings debug_settings,
        RenderEffectDebugSettings post_process_debug_settings,
        bool isolate_forward_debug,
        bool render_ao,
        bool render_ssr,
        bool render_bloom,
        bool render_smaa)
        : m_output_route(output_route),
          m_availability(availability),
          m_debug_settings(debug_settings),
          m_post_process_debug_settings(post_process_debug_settings),
          m_isolate_forward_debug(isolate_forward_debug),
          m_render_ao(render_ao),
          m_render_ssr(render_ssr),
          m_render_bloom(render_bloom),
          m_render_smaa(render_smaa) {}
} // namespace NexAur
