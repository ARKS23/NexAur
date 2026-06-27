#pragma once

#include "Core/Base.h"
#include "Core/Time/TimeStep.h"

namespace NexAur {
    class SceneV2;

    class NEXAUR_API RuntimeCameraControllerSystem final {
    public:
        void update(SceneV2& scene, TimeStep delta_time);
    };
} // namespace NexAur
