#pragma once

#include "editor_panel.h"
#include "Core/Log/log_system.h"

#include <array>

namespace NexAur {
    class NEXAUR_API ConsolePanel : public EditorPanel {
    public:
        explicit ConsolePanel(const std::string& name = "Console");
        ~ConsolePanel() override = default;

        void onUIRender() override;

    private:
        void drawToolbar();
        void drawMessages();
        bool passesFilter(const LogMessage& message) const;
        void drawMessage(const LogMessage& message) const;

    private:
        std::array<char, 128> m_filter{};
        bool m_auto_scroll = true;
    };
} // namespace NexAur
