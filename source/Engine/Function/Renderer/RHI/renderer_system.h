#pragma once

#include "Core/Base.h"
#include "Core/Events/window_event.h"
#include "Core/Time/TimeStep.h"
#include "Function/Renderer/RHI/framebuffer.h"
#include "Function/Renderer/RHI/render_device.h"
#include "Function/Renderer/RHI/render_pass_context.h"
#include "Function/Renderer/RHI/renderer_service.h"
#include "Function/Renderer/data/render_data.h"

namespace NexAur {
    class RenderForwardPipeline;

    // RendererModule 的内部主系统，对外只暴露 RendererService。
    class NEXAUR_API RendererSystem : public RendererService {
    public:
        RendererSystem() = default;
        ~RendererSystem() = default;

        void init();
        void shutdown();

        void tick(TimeStep ts, const RenderDataPacket& render_data);
        void render(TimeStep ts, const RenderDataPacket& render_data) override { tick(ts, render_data); }
        void onEvent(Event& e);

        RendererBackendType getBackendType() const override { return RendererBackendType::OpenGLLegacy; }
        void setViewportSize(uint32_t width, uint32_t height) override;
        std::pair<uint32_t, uint32_t> getViewportSize() const override { return { m_viewport_width, m_viewport_height }; }
        ViewportOutput getViewportOutput() const override;
        ViewportPickResult pickViewport(const ViewportPickRequest& request) override;

    private:
        ResolvedRenderDataPacket resolveRenderData(const RenderDataPacket& render_data);
        RenderPassContext createRenderPassContext() const;
        bool onWindowResize(WindowResizeEvent& e);

    private:
        std::shared_ptr<RenderDevice> m_render_device;
        std::shared_ptr<RenderForwardPipeline> m_forward_pipeline;

        std::shared_ptr<Framebuffer> m_viewport_framebuffer;
        uint32_t m_viewport_width = 1280;
        uint32_t m_viewport_height = 720;
    };
} // namespace NexAur
