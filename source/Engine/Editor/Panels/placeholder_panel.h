#pragma once

#include "editor_panel.h"

#include <string>

namespace NexAur {
    class NEXAUR_API PlaceholderPanel : public EditorPanel {
    public:
        PlaceholderPanel(std::string name, std::string message);
        ~PlaceholderPanel() override = default;

        void onUIRender() override;

    private:
        std::string m_message;
    };
} // namespace NexAur
