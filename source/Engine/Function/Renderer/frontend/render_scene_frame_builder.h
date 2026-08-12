#pragma once

#include <cstdint>

#include "Core/Base.h"
#include "Function/Renderer/data/render_scene_frame.h"

namespace NexAur {
    struct RenderDataPacket;

    class NEXAUR_API RenderSceneFrameBuilder {
    public:
        RenderSceneFrame buildRenderSceneFrame(
            const RenderDataPacket& render_data,
            uint32_t viewport_width,
            uint32_t viewport_height) const;
    };
} // namespace NexAur
