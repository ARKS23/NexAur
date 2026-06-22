#pragma once

#include <array>
#include <utility>

#include "Core/Base.h"
#include "Function/Input/KeyCode/key_codes.h"
#include "Function/Input/KeyCode/mouse_codes.h"

namespace NexAur {
    struct NEXAUR_API InputState {
        static constexpr size_t MaxKeyCodes = 512;
        static constexpr size_t MaxMouseButtons = 8;

        void reset() {
            keys.fill(false);
            mouse_buttons.fill(false);
            mouse_x = 0.0f;
            mouse_y = 0.0f;
        }

        void setKeyPressed(KeyCode key_code, bool pressed) {
            const size_t index = static_cast<size_t>(key_code);
            if (index < keys.size()) {
                keys[index] = pressed;
            }
        }

        void setMouseButtonPressed(MouseCode mouse_code, bool pressed) {
            const size_t index = static_cast<size_t>(mouse_code);
            if (index < mouse_buttons.size()) {
                mouse_buttons[index] = pressed;
            }
        }

        bool isKeyPressed(KeyCode key_code) const {
            const size_t index = static_cast<size_t>(key_code);
            return index < keys.size() && keys[index];
        }

        bool isMouseButtonPressed(MouseCode mouse_code) const {
            const size_t index = static_cast<size_t>(mouse_code);
            return index < mouse_buttons.size() && mouse_buttons[index];
        }

        std::pair<float, float> getMousePosition() const {
            return { mouse_x, mouse_y };
        }

        std::array<bool, MaxKeyCodes> keys{};
        std::array<bool, MaxMouseButtons> mouse_buttons{};
        float mouse_x = 0.0f;
        float mouse_y = 0.0f;
    };
} // namespace NexAur
