#pragma once

#include <cstdint>

#include "Core/Base.h"
#include "Function/RendererV2/render_view.h"

namespace NexAur {
    struct RenderDataPacket;

    // Converts engine-facing frame data into RendererV2 CPU-side descriptions.
    class NEXAUR_API VulkanRenderDataTranslator {
    public:
        void resetFrame();

        RenderView buildRenderView(
            const RenderDataPacket& render_data,
            uint32_t viewport_width,
            uint32_t viewport_height) const;
    };
} // namespace NexAur
