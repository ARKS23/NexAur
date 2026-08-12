#pragma once

#include <cstdint>
#include <vector>

#include "Core/Base.h"
#include "Function/Renderer/data/render_scene_frame.h"
#include "Function/Renderer/data/render_shadow_cascade.h"

namespace NexAur {
    class NEXAUR_API RenderShadowFrameBuilder {
    public:
        RenderShadowCascadeFrame buildDirectionalShadowFrame(
            const RenderView& view,
            const RenderFrameDirectionalLight& light,
            const RenderShadowSettings& settings,
            const RenderEffectDebugSettings& debug_settings,
            float shadow_map_size) const;

        RenderPointShadowFrame buildPointShadowFrame(
            const std::vector<RenderFramePointLight>& lights,
            const RenderPointShadowSettings& settings,
            uint32_t light_capacity) const;

        RenderRectShadowFrame buildRectShadowFrame(
            const std::vector<RenderFrameRectLight>& lights,
            const RenderRectShadowSettings& settings,
            uint32_t light_capacity) const;
    };
} // namespace NexAur
