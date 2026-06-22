#pragma once
#include "Core/Base.h"
#include "Core/Events/event.h"

struct ImGuiContext;

namespace NexAur {
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
