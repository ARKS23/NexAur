#pragma once
#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Core/Time/TimeStep.h"
// #include "Function/Scene/scene_v2.h"
#include "Function/Scene/entity.h"

#include <memory>

namespace NexAur {
    class SceneV2;

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
        
    private:
        std::shared_ptr<SceneV2> m_active_scene;
        Entity m_selected_entity;
        bool m_show_properties_panel = true;
    };
} // namespace NexAur
