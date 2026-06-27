#include "pch.h"
#include "input_action_system.h"

#include "Function/Platform/platform_services.h"

#include <algorithm>

namespace NexAur {
    InputButtonBinding InputButtonBinding::key(KeyCode key_code) {
        InputButtonBinding binding;
        binding.device = InputBindingDevice::Keyboard;
        binding.key_code = key_code;
        return binding;
    }

    InputButtonBinding InputButtonBinding::mouse(MouseCode mouse_code) {
        InputButtonBinding binding;
        binding.device = InputBindingDevice::MouseButton;
        binding.mouse_code = mouse_code;
        return binding;
    }

    InputActionSystem::InputActionSystem(std::shared_ptr<InputService> input_service)
        : m_input_service(std::move(input_service)) {}

    void InputActionSystem::setInputService(std::shared_ptr<InputService> input_service) {
        m_input_service = std::move(input_service);
    }

    void InputActionSystem::clearBindings() {
        m_digital_bindings.clear();
        m_axis1d_bindings.clear();
        m_axis2d_bindings.clear();
        m_digital_states.clear();
        m_previous_digital_states.clear();
        m_axis1d_values.clear();
        m_axis2d_values.clear();
    }

    void InputActionSystem::configureDefaultBindings() {
        clearBindings();

        bindAxis2D(
            DefaultInputActions::Move,
            InputButtonBinding::key(KeyCode::A),
            InputButtonBinding::key(KeyCode::D),
            InputButtonBinding::key(KeyCode::S),
            InputButtonBinding::key(KeyCode::W));

        bindDigital(DefaultInputActions::Jump, InputButtonBinding::key(KeyCode::Space));
        bindDigital(DefaultInputActions::Fire, InputButtonBinding::mouse(MouseCode::ButtonLeft));
        bindDigital(DefaultInputActions::Pause, InputButtonBinding::key(KeyCode::Escape));
        bindDigital(DefaultInputActions::Interact, InputButtonBinding::key(KeyCode::E));
    }

    void InputActionSystem::bindDigital(std::string_view action, InputButtonBinding button) {
        const std::string key = makeKey(action);
        DigitalActionBinding& binding = m_digital_bindings[key];
        binding.action = key;
        binding.buttons.push_back(button);
    }

    void InputActionSystem::bindAxis1D(
        std::string_view axis,
        InputButtonBinding negative,
        InputButtonBinding positive) {
        const std::string key = makeKey(axis);
        m_axis1d_bindings[key] = Axis1DBinding{ key, negative, positive };
    }

    void InputActionSystem::bindAxis2D(
        std::string_view axis,
        InputButtonBinding negative_x,
        InputButtonBinding positive_x,
        InputButtonBinding negative_y,
        InputButtonBinding positive_y) {
        const std::string key = makeKey(axis);
        m_axis2d_bindings[key] = Axis2DBinding{ key, negative_x, positive_x, negative_y, positive_y };
    }

    void InputActionSystem::update() {
        m_previous_digital_states = m_digital_states;
        m_digital_states.clear();
        m_axis1d_values.clear();
        m_axis2d_values.clear();

        for (const auto& [action, binding] : m_digital_bindings) {
            bool held = false;
            for (const InputButtonBinding& button : binding.buttons) {
                held = held || readButton(button);
            }

            const auto previous = m_previous_digital_states.find(action);
            const bool previous_held =
                previous != m_previous_digital_states.end() && previous->second.held;

            m_digital_states[action] = DigitalActionState{
                held,
                held && !previous_held,
                !held && previous_held
            };
        }

        for (const auto& [axis, binding] : m_axis1d_bindings) {
            float value = 0.0f;
            if (readButton(binding.negative)) {
                value -= 1.0f;
            }
            if (readButton(binding.positive)) {
                value += 1.0f;
            }
            m_axis1d_values[axis] = std::clamp(value, -1.0f, 1.0f);
        }

        for (const auto& [axis, binding] : m_axis2d_bindings) {
            glm::vec2 value{ 0.0f };
            if (readButton(binding.negative_x)) {
                value.x -= 1.0f;
            }
            if (readButton(binding.positive_x)) {
                value.x += 1.0f;
            }
            if (readButton(binding.negative_y)) {
                value.y -= 1.0f;
            }
            if (readButton(binding.positive_y)) {
                value.y += 1.0f;
            }
            m_axis2d_values[axis] = {
                std::clamp(value.x, -1.0f, 1.0f),
                std::clamp(value.y, -1.0f, 1.0f),
            };
        }
    }

    bool InputActionSystem::isHeld(std::string_view action) const {
        const auto it = m_digital_states.find(makeKey(action));
        return it != m_digital_states.end() && it->second.held;
    }

    bool InputActionSystem::wasPressed(std::string_view action) const {
        const auto it = m_digital_states.find(makeKey(action));
        return it != m_digital_states.end() && it->second.pressed;
    }

    bool InputActionSystem::wasReleased(std::string_view action) const {
        const auto it = m_digital_states.find(makeKey(action));
        return it != m_digital_states.end() && it->second.released;
    }

    float InputActionSystem::getAxis1D(std::string_view axis) const {
        const auto it = m_axis1d_values.find(makeKey(axis));
        return it != m_axis1d_values.end() ? it->second : 0.0f;
    }

    glm::vec2 InputActionSystem::getAxis2D(std::string_view axis) const {
        const auto it = m_axis2d_values.find(makeKey(axis));
        return it != m_axis2d_values.end() ? it->second : glm::vec2{ 0.0f };
    }

    bool InputActionSystem::readButton(const InputButtonBinding& binding) const {
        if (!m_input_service) {
            return false;
        }

        switch (binding.device) {
        case InputBindingDevice::Keyboard:
            return m_input_service->isKeyPressed(binding.key_code);
        case InputBindingDevice::MouseButton:
            return m_input_service->isMouseButtonPressed(binding.mouse_code);
        default:
            return false;
        }
    }

    std::string InputActionSystem::makeKey(std::string_view name) {
        return std::string(name);
    }
} // namespace NexAur
