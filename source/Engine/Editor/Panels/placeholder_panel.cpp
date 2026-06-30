#include "pch.h"
#include "placeholder_panel.h"

#include <imgui.h>

#include <utility>

namespace NexAur {
    PlaceholderPanel::PlaceholderPanel(std::string name, std::string message)
        : EditorPanel(std::move(name)),
          m_message(std::move(message)) {}

    void PlaceholderPanel::onUIRender() {
        bool& open_flag = getOpenFlag();
        if (!ImGui::Begin(getName().c_str(), &open_flag)) {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("%s", m_message.c_str());

        ImGui::End();
    }
} // namespace NexAur
