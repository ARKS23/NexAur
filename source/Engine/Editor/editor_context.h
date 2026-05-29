#pragma once
#include "Core/Base.h"
#include "Function/Scene/entity.h"

namespace NexAur {
    class EditorCamera;
    class SceneV2;
    class RendererSystem;

    struct EditorContext {
        std::shared_ptr<SceneV2> active_scene;
        std::shared_ptr<RendererSystem> renderer_system;
        std::shared_ptr<EditorCamera> viewport_camera; // 编辑器视口观察相机，独立于场景 CameraComponent。
        Entity selected_entity;
        std::string selection_source;   // 通过哪个面板选中实体记录
        bool viewport_focused = false;
        bool viewport_hovered = false;
    };
} // namespace NexAur
