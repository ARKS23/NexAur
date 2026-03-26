#pragma once
#include "editor_panel.h"

#include <glm/glm.hpp>

namespace NexAur {
    class ViewportPanel : public EditorPanel {
    public:
        ViewportPanel();
        ~ViewportPanel() override = default;

        virtual void onUpdate(TimeStep delta_time) override;
        virtual void onUIRender() override;
        virtual void onEvent(Event& event) override;

    private:
        glm::vec2 m_viewport_size{ 1280, 720 };
        glm::vec2 m_viewport_bounds[2];
        bool m_viewport_focused = false;
        bool m_viewport_hovered = false;
    };
} // namespace NexAur
