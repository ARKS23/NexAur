#pragma once
#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Core/Time/TimeStep.h"
#include "Function/Scene/entity.h"

#include <memory>
#include <vector>

namespace NexAur {
    class SceneV2;
    class EditorPanel;
} // namespace NexAur


namespace NexAur {
    class NEXAUR_API EditorLayer {
    public:
        EditorLayer();
        ~EditorLayer();

        void onUpdate(TimeStep delta_time);

        void onUIRender();

        void onEvent(Event& event);

        void setActiveScene(std::shared_ptr<SceneV2> scene) { m_active_scene = scene; }

    private:
        void init();
        void shutdown();
        void beginDockSpace();
        void endDockSpace();
        
    private:
        Entity m_selected_entity;
        std::shared_ptr<SceneV2> m_active_scene;
        std::vector<std::shared_ptr<EditorPanel>> m_panels; // TODO: 编辑器控制面板列表, 后续OnUIRender更新
        bool m_show_properties_panel = true; // 测试变量
    };
} // namespace NexAur
