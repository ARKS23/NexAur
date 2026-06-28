#pragma once

#include "Core/Base.h"
#include "Function/Physics/physics_types.h"

namespace NexAur {
    class SceneV2;

    // Lightweight trigger overlap support for gameplay. This is intentionally
    // not a full physics simulation owner or fixed-step PhysicsModule.
    class NEXAUR_API TriggerOverlapSystem final {
    public:
        void update(SceneV2& scene);
        const TriggerOverlapFrame& getFrame() const { return m_frame; }

    private:
        TriggerOverlapFrame m_frame;
    };
} // namespace NexAur
