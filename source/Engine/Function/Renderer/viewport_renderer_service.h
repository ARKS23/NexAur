#pragma once

#include <cstdint>
#include <utility>

#include "Core/Base.h"
#include "Function/Renderer/renderer_service_types.h"

namespace NexAur {
    // Viewport presentation and picking contract used by editor-facing tools.
    class NEXAUR_API ViewportRendererService {
    public:
        virtual ~ViewportRendererService() = default;

        virtual void setViewportSize(uint32_t width, uint32_t height) = 0;
        virtual std::pair<uint32_t, uint32_t> getViewportSize() const = 0;
        virtual ViewportOutput getViewportOutput() const = 0;
        virtual ViewportPickResult pickViewport(const ViewportPickRequest& request) = 0;
    };
} // namespace NexAur
