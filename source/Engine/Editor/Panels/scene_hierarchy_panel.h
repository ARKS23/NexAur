#pragma once
#include "Core/Base.h"
#include "editor_panel.h"

namespace NexAur {
    class SceneHierarchyPanel : public EditorPanel {
    public:
        SceneHierarchyPanel(const std::string& name);
        ~SceneHierarchyPanel() override = default;

        virtual void onUpdate(TimeStep delta_time) override;
        virtual void onUIRender() override;
        virtual void onEvent(Event& event) override;

    private:
        void drawEntityNode(Entity entity);
    };
} // namespace NexAur
