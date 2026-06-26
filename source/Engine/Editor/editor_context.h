#pragma once

#include <memory>
#include <string>

#include "Core/Base.h"
#include "Function/Scene/entity.h"

namespace NexAur {
    class EditorCamera;
    class InputService;
    class RenderContext;
    class RendererService;
    class SceneService;
    class SceneV2;
    class SelectionService;
    class UIService;

    enum class EditorViewportViewMode {
        SceneView,
        GameView
    };

    // EditorLayer 和各个 Panel 共享的小上下文。
    // 这里放编辑器真正需要的窄服务，不把 ModuleRegistry 或全局上下文传进面板。
    struct EditorContext {
        std::shared_ptr<SceneV2> active_scene;
        std::shared_ptr<SceneService> scene_service;
        std::shared_ptr<RendererService> renderer_service;
        std::shared_ptr<InputService> input_service;
        std::shared_ptr<RenderContext> render_context;
        std::shared_ptr<UIService> ui_service;
        std::shared_ptr<EditorCamera> viewport_camera;
        std::weak_ptr<SelectionService> selection_service;
        Entity selected_entity;
        std::string selection_source;
        EditorViewportViewMode viewport_view_mode = EditorViewportViewMode::SceneView;
        bool viewport_focused = false;
        bool viewport_hovered = false;
    };
} // namespace NexAur
