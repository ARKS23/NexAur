#pragma once

#include "Core/Base.h"
#include "Function/Game/game_state.h"

namespace NexAur {
    class NEXAUR_API RuntimeHud final {
    public:
        void draw(const GameStateSnapshot& snapshot) const;
    };
} // namespace NexAur
