#pragma once

#include "Core/Base.h"
#include "Function/RendererV2/render_scene_frame.h"

namespace NexAur {
    struct RenderDataPacket;

    class NEXAUR_API RenderSceneFrameBuilder {
    public:
        RenderSceneFrame buildRenderSceneFrame(
            const RenderDataPacket& render_data,
            const RenderView& render_view) const;
    };
} // namespace NexAur
