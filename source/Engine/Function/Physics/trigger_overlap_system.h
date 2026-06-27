#pragma once

#include "Core/Base.h"
#include "Function/Physics/physics_types.h"

namespace NexAur {
    class SceneV2;

    class NEXAUR_API TriggerOverlapSystem final {
    public:
        void update(SceneV2& scene);
        const TriggerOverlapFrame& getFrame() const { return m_frame; }

    private:
        TriggerOverlapFrame m_frame;
    };
} // namespace NexAur
