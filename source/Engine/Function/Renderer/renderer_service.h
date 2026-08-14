#pragma once

#include "Core/Base.h"
#include "Core/Time/TimeStep.h"
#include "Function/Renderer/renderer_service_types.h"

namespace NexAur {
    struct RenderDataPacket;

    // Core frame-rendering facade. Optional editor workflows use narrower services.
    class NEXAUR_API RendererService {
    public:
        virtual ~RendererService() = default;

        virtual RendererBackendType getBackendType() const = 0;
        virtual void render(TimeStep ts, const RenderDataPacket& render_data) = 0;

        virtual void onUIContextInitialized() {}
        virtual void beginUIFrame() {}
        virtual void onUIContextShutdown() {}
    };
} // namespace NexAur
