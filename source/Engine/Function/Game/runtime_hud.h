#pragma once

#include "Core/Base.h"
#include "Function/Game/game_state.h"

namespace NexAur {
    // ImGui-backed v1 HUD. Keep the UI dependency inside runtime_hud.cpp so
    // gameplay systems and components remain independent from ImGui.
    class NEXAUR_API RuntimeHud final {
    public:
        void draw(const GameStateSnapshot& snapshot) const;
    };
} // namespace NexAur
