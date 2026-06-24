#pragma once

#include <memory>

#include "Core/Base.h"
#include "Core/Events/event.h"

struct ImGuiContext;

namespace NexAur {
    class RendererService;
    class WindowService;

    // UI 对外暴露的帧生命周期和输入捕获状态。
    class NEXAUR_API UIService {
    public:
        virtual ~UIService() = default;

        virtual void beginFrame() = 0;
        virtual void finalizeFrame() = 0;
        virtual void renderBackend() = 0;
        virtual void endFrameAndRender() = 0;

        virtual void onEvent(Event& event) = 0;
        virtual bool isConsumingInput() const = 0;
        virtual bool wantsCaptureMouse() const = 0;
        virtual bool wantsCaptureKeyboard() const = 0;
        virtual bool wantsTextInput() const = 0;
    };

    class NEXAUR_API UISystem : public UIService {
    public:
        UISystem() = default;
        ~UISystem() = default;

        void init(std::shared_ptr<WindowService> window_service, std::shared_ptr<RendererService> renderer_service);
        void shutdown();

        void beginFrame() override;
        void finalizeFrame() override;
        void renderBackend() override;
        void endFrameAndRender() override;

        void onEvent(Event& event) override;
        bool isConsumingInput() const override;
        bool isConsumeingInput() const;
        bool wantsCaptureMouse() const override;
        bool wantsCaptureKeyboard() const override;
        bool wantsTextInput() const override;

    private:
        enum class Backend {
            None,
            OpenGL,
            GlfwOnly
        };

        std::shared_ptr<WindowService> m_window_service;
        std::weak_ptr<RendererService> m_renderer_service;
        Backend m_backend{Backend::None};
        bool m_context_initialized{false};
    };
} // namespace NexAur
