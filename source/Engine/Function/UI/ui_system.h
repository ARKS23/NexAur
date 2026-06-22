#pragma once
#include "Core/Base.h"
#include "Core/Events/event.h"

struct ImGuiContext;

namespace NexAur {
    // UI 对外暴露的帧生命周期和输入捕获状态。
    // 事件路由只需要知道“UI 是否想吃掉输入”，不需要知道 ImGui 后端细节。
    class NEXAUR_API UIService {
    public:
            virtual ~UIService() = default;

            virtual void beginFrame() = 0;
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
    
            void init();
            void shutdown();

            void beginFrame() override;
            void endFrameAndRender() override;
    
            void onEvent(Event& event) override;
            bool isConsumingInput() const override;
            bool isConsumeingInput() const;
            bool wantsCaptureMouse() const override;
            bool wantsCaptureKeyboard() const override;
            bool wantsTextInput() const override;
    };
} // namespace NexAur
