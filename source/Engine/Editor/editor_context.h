#pragma once
#include "Core/Base.h"
#include "Function/Scene/entity.h"

namespace NexAur {
    class SceneV2;
    class RendererSystem;

    struct EditorContext {
        std::shared_ptr<SceneV2> active_scene;
        std::shared_ptr<RendererSystem> renderer_system;
        Entity selected_entity;
    };
} // namespace NexAur
