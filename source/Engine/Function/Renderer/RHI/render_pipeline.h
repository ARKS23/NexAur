#pragma once

#include "Core/Base.h"

namespace NexAur {
    struct RenderPassContext;
    struct ResolvedRenderDataPacket;

    class NEXAUR_API RenderPipeline {
    public:
        virtual ~RenderPipeline() = default;

        virtual void init() = 0;
        virtual void render(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) = 0;
    };
} // namespace NexAur
