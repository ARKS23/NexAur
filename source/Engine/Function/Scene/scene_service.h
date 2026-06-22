#pragma once

#include <memory>
#include <string>

#include "Core/Base.h"

namespace NexAur {
    class SceneV2;

    class NEXAUR_API SceneService {
    public:
        virtual ~SceneService() = default;

        virtual std::shared_ptr<SceneV2> getActiveScene() const = 0;
        virtual std::shared_ptr<SceneV2> createScene(const std::string& name) = 0;
        virtual void setActiveScene(std::shared_ptr<SceneV2> new_scene) = 0;
    };
} // namespace NexAur
