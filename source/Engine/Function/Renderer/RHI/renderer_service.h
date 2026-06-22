#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include "Core/Base.h"
#include "Core/Time/TimeStep.h"

namespace NexAur {
    class Framebuffer;
    struct RenderDataPacket;

    class NEXAUR_API RendererService {
    public:
        virtual ~RendererService() = default;

        virtual void render(TimeStep ts, const RenderDataPacket& render_data) = 0;
        virtual void setViewportSize(uint32_t width, uint32_t height) = 0;
        virtual std::pair<uint32_t, uint32_t> getViewportSize() const = 0;
        virtual uint32_t getViewportColorAttachment() const = 0;
        virtual int readViewportEntityID(int x, int y) = 0;

        // Compatibility bridge for existing render passes. PR8 should remove this
        // from pass-level code in favor of an explicit RenderPassContext.
        virtual std::shared_ptr<Framebuffer> getViewportFramebuffer() const = 0;
    };
} // namespace NexAur
