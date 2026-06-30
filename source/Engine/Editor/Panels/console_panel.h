#pragma once

#include "editor_panel.h"

#include <array>

namespace NexAur {
    class NEXAUR_API ConsolePanel : public EditorPanel {
    public:
        explicit ConsolePanel(const std::string& name = "Console");
        ~ConsolePanel() override = default;

        void onUIRender() override;

    private:
        void drawMessage(const char* level, const char* text) const;

    private:
        std::array<char, 128> m_filter{};
    };
} // namespace NexAur
