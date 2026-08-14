#pragma once

#include <cstdint>

#include "Function/Renderer/data/render_scene_frame.h"

namespace NexAur {
    struct RenderDataPacket;

    class RenderSceneFrameBuilder {
    public:
        RenderSceneFrame buildRenderSceneFrame(
            const RenderDataPacket& render_data,
            uint32_t viewport_width,
            uint32_t viewport_height) const;
    };
} // namespace NexAur
