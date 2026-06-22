#pragma once

#include <cstdint>
#include <utility>

#include "Core/Base.h"
#include "Function/Input/KeyCode/key_codes.h"
#include "Function/Input/KeyCode/mouse_codes.h"
#include "Function/Input/input_state.h"

namespace NexAur {
    class NEXAUR_API WindowService {
    public:
        virtual ~WindowService() = default;

        virtual void* getNativeWindow() const = 0;
        virtual std::pair<uint32_t, uint32_t> getSize() const = 0;
        virtual void setTitle(const char* title) = 0;
        virtual void present() = 0;
        virtual void pollEvents() = 0;
    };

    class NEXAUR_API InputService {
    public:
        virtual ~InputService() = default;

        virtual void update() = 0;
        virtual const InputState& getState() const = 0;

        virtual bool isKeyPressed(KeyCode key_code) const {
            return getState().isKeyPressed(key_code);
        }

        virtual bool isMouseButtonPressed(MouseCode mouse_code) const {
            return getState().isMouseButtonPressed(mouse_code);
        }

        virtual std::pair<float, float> getMousePosition() const {
            return getState().getMousePosition();
        }
    };
} // namespace NexAur
