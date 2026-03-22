#pragma once
#include "Core/Base.h"
#include "Core/Events/event.h"

struct ImGuiContext;

namespace NexAur {
    class NEXAUR_API UISystem {
    public:
            UISystem() = default;
            ~UISystem() = default;
    
            void init();
            void shutdown();

            void beginFrame();
            void endFrameAndRender();
    
            void onEvent(Event& event);
            bool isConsumeingInput() const;
    };
} // namespace NexAur
