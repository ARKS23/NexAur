#pragma once

#include "Core/Base.h"
#include "Core/Time/TimeStep.h"

namespace NexAur {
    class InputActionService;
    class SceneV2;

    class NEXAUR_API PlayerControlSystem final {
    public:
        void update(SceneV2& scene, const InputActionService& input_actions, TimeStep delta_time);
    };

    class NEXAUR_API EnemySystem final {
    public:
        void update(SceneV2& scene, TimeStep delta_time);
    };

    class NEXAUR_API MovementSystem final {
    public:
        void update(SceneV2& scene, TimeStep delta_time);
    };

    class NEXAUR_API LifetimeSystem final {
    public:
        void update(SceneV2& scene, TimeStep delta_time);
    };

    class NEXAUR_API HealthSystem final {
    public:
        void update(SceneV2& scene);
    };
} // namespace NexAur
