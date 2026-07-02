#pragma once

#include <cstdint>
#include <utility>

#include "Core/Base.h"
#include "Core/Time/TimeStep.h"
#include "Function/Renderer/renderer_service_types.h"

namespace NexAur {
    struct RenderDataPacket;

    // RendererModule 的对外门面。Editor/Runtime 只依赖这个接口，
    // 不应该知道当前具体后端实现和图形 API 细节。
    class NEXAUR_API RendererService {
    public:
        virtual ~RendererService() = default;

        virtual RendererBackendType getBackendType() const = 0;
        virtual void render(TimeStep ts, const RenderDataPacket& render_data) = 0;
        virtual void setViewportSize(uint32_t width, uint32_t height) = 0;
        virtual std::pair<uint32_t, uint32_t> getViewportSize() const = 0;
        virtual ViewportOutput getViewportOutput() const = 0;
        virtual ViewportPickResult pickViewport(const ViewportPickRequest& request) = 0;
        virtual bool requestReflectionProbeCapture(const ReflectionProbeCaptureRequest& request) {
            (void)request;
            return false;
        }
        virtual ReflectionProbeCaptureState getReflectionProbeCaptureState(int entity_id) const {
            (void)entity_id;
            return {};
        }

        virtual void onUIContextInitialized() {}
        virtual void beginUIFrame() {}
        virtual void onUIContextShutdown() {}
    };
} // namespace NexAur
