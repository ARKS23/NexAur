#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Input/KeyCode/key_codes.h"
#include "Function/Input/KeyCode/mouse_codes.h"

namespace NexAur {
    class InputService;

    namespace DefaultInputActions {
        inline constexpr std::string_view Move = "Move";
        inline constexpr std::string_view Jump = "Jump";
        inline constexpr std::string_view Fire = "Fire";
        inline constexpr std::string_view Pause = "Pause";
        inline constexpr std::string_view Interact = "Interact";
    } // namespace DefaultInputActions

    enum class InputBindingDevice {
        Keyboard,
        MouseButton
    };

    struct InputButtonBinding {
        static InputButtonBinding key(KeyCode key_code);
        static InputButtonBinding mouse(MouseCode mouse_code);

        InputBindingDevice device = InputBindingDevice::Keyboard;
        KeyCode key_code = KeyCode::Space;
        MouseCode mouse_code = MouseCode::Button0;
    };

    struct DigitalActionBinding {
        std::string action;
        std::vector<InputButtonBinding> buttons;
    };

    struct Axis1DBinding {
        std::string axis;
        InputButtonBinding negative;
        InputButtonBinding positive;
    };

    struct Axis2DBinding {
        std::string axis;
        InputButtonBinding negative_x;
        InputButtonBinding positive_x;
        InputButtonBinding negative_y;
        InputButtonBinding positive_y;
    };

    class NEXAUR_API InputActionService {
    public:
        virtual ~InputActionService() = default;

        virtual bool isHeld(std::string_view action) const = 0;
        virtual bool wasPressed(std::string_view action) const = 0;
        virtual bool wasReleased(std::string_view action) const = 0;

        virtual float getAxis1D(std::string_view axis) const = 0;
        virtual glm::vec2 getAxis2D(std::string_view axis) const = 0;
    };

    class NEXAUR_API InputActionSystem final : public InputActionService {
    public:
        InputActionSystem() = default;
        explicit InputActionSystem(std::shared_ptr<InputService> input_service);

        void setInputService(std::shared_ptr<InputService> input_service);
        void clearBindings();
        void configureDefaultBindings();

        void bindDigital(std::string_view action, InputButtonBinding button);
        void bindAxis1D(std::string_view axis, InputButtonBinding negative, InputButtonBinding positive);
        void bindAxis2D(
            std::string_view axis,
            InputButtonBinding negative_x,
            InputButtonBinding positive_x,
            InputButtonBinding negative_y,
            InputButtonBinding positive_y);

        void update();

        bool isHeld(std::string_view action) const override;
        bool wasPressed(std::string_view action) const override;
        bool wasReleased(std::string_view action) const override;
        float getAxis1D(std::string_view axis) const override;
        glm::vec2 getAxis2D(std::string_view axis) const override;

    private:
        struct DigitalActionState {
            bool held = false;
            bool pressed = false;
            bool released = false;
        };

        bool readButton(const InputButtonBinding& binding) const;
        static std::string makeKey(std::string_view name);

    private:
        std::shared_ptr<InputService> m_input_service;
        std::unordered_map<std::string, DigitalActionBinding> m_digital_bindings;
        std::unordered_map<std::string, Axis1DBinding> m_axis1d_bindings;
        std::unordered_map<std::string, Axis2DBinding> m_axis2d_bindings;

        std::unordered_map<std::string, DigitalActionState> m_digital_states;
        std::unordered_map<std::string, DigitalActionState> m_previous_digital_states;
        std::unordered_map<std::string, float> m_axis1d_values;
        std::unordered_map<std::string, glm::vec2> m_axis2d_values;
    };
} // namespace NexAur
