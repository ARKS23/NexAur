#pragma once
#include "Core/Base.h"
#include "editor_panel.h"
#include "Function/Scene/entity.h"

namespace NexAur {
    class PropertiesPanel : public EditorPanel {
    public:
        PropertiesPanel(const std::string& name = "Properties") : EditorPanel(name) {}
        ~PropertiesPanel() override = default;

        virtual void onUpdate(TimeStep delta_time) override;
        virtual void onUIRender() override;
        virtual void onEvent(Event& event) override;

    private:
        void drawEntityHeader(Entity entity);   // 显示组件ID
        void drawAddComponentMenu(Entity entity);
        void drawTagComponent(Entity entity);
        void drawTransformComponent(Entity entity);
        void drawCameraComponent(Entity entity);
        void drawActiveCameraComponent(Entity entity);
        void drawMeshRendererComponent(Entity entity);
        void drawDirectionalLightComponent(Entity entity);
        void drawPointLightComponent(Entity entity);
        void drawRectLightComponent(Entity entity);
        void drawEnvironmentComponent(Entity entity);
        void drawReflectionProbeComponent(Entity entity);
    };
} // namespace NexAur
