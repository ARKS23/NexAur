#include "pch.h"
#include "editor_camera.h"

#include "Function/Input/input_state.h"
#include "Function/Platform/platform_services.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace NexAur {
    EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
        : m_fov(fov), m_aspect_ratio(aspectRatio), m_near_clip(nearClip), m_far_clip(farClip) {
        m_position = { 0.0f, 0.0f, 5.0f };
        updateProjection();
        updateView();
    }

    void EditorCamera::setInputService(std::shared_ptr<InputService> input_service) {
        m_input_service = std::move(input_service);
    }

    void EditorCamera::updateViewProjection() {
        m_view_projection = m_projection * m_view;
    }

    void EditorCamera::updateProjection() {
        m_aspect_ratio = m_viewport_width / m_viewport_height;
        m_projection = glm::perspective(glm::radians(m_fov), m_aspect_ratio, m_near_clip, m_far_clip);
        updateViewProjection();
    }

    void EditorCamera::updateView() {
        glm::quat orientation = getOrientation();
        glm::vec3 front = glm::rotate(orientation, glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 up = glm::rotate(orientation, glm::vec3(0.0f, 1.0f, 0.0f));

        m_view = glm::lookAt(m_position, m_position + front, up);

        updateViewProjection();
    }

    void EditorCamera::setViewportSize(float width, float height) {
        m_viewport_width = width;
        m_viewport_height = height;
        updateProjection();
    }

    void EditorCamera::onUpdate(TimeStep deltaTime) {
        std::shared_ptr<InputService> input_service = m_input_service.lock();
        if (!input_service) {
            return;
        }

        const InputState& input_state = input_service->getState();
        const glm::vec2 mouse{ input_state.mouse_x, input_state.mouse_y };

        if (input_state.isMouseButtonPressed(MouseCode::ButtonRight)) {
            glm::vec2 delta = (mouse - m_init_mouse_position) * 0.003f;
            m_init_mouse_position = mouse;

            onMouseRotate(delta);
        } else {
            m_init_mouse_position = mouse;
        }

        float speed = m_move_speed * static_cast<float>(deltaTime);
        if (input_state.isKeyPressed(KeyCode::W)) {
            m_position += getForwardDirection() * speed;
        }
        if (input_state.isKeyPressed(KeyCode::S)) {
            m_position -= getForwardDirection() * speed;
        }
        if (input_state.isKeyPressed(KeyCode::A)) {
            m_position -= getRightDirection() * speed;
        }
        if (input_state.isKeyPressed(KeyCode::D)) {
            m_position += getRightDirection() * speed;
        }
        if (input_state.isKeyPressed(KeyCode::Q)) {
            m_position += getUpDirection() * speed;
        }
        if (input_state.isKeyPressed(KeyCode::E)) {
            m_position -= getUpDirection() * speed;
        }

        updateView();
    }

    void EditorCamera::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<MouseScrolledEvent>(NX_BIND_EVENT_FN(EditorCamera::onMouseScroll));
        dispatcher.dispatch<WindowResizeEvent>(NX_BIND_EVENT_FN(EditorCamera::onWindowResize));
    }

    bool EditorCamera::onWindowResize(WindowResizeEvent& event) {
        setViewportSize(static_cast<float>(event.getWidth()), static_cast<float>(event.getHeight()));
        return false;
    }

    bool EditorCamera::onMouseScroll(MouseScrolledEvent& event) {
        float delta = event.GetYOffset() * zoomSpeed();
        onMouseZoom(delta);
        updateView();

        return false;
    }

    void EditorCamera::onMouseRotate(const glm::vec2& delta) {
        float yaw_sign = getUpDirection().y < 0.0f ? -1.0f : 1.0f;

        m_yaw += yaw_sign * delta.x * rotationSpeed();
        m_pitch += delta.y * rotationSpeed();
        m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

        updateView();
    }

    void EditorCamera::onMouseZoom(float delta) {
        (void)delta;
    }

    float EditorCamera::zoomSpeed() const {
        return 2.0f;
    }

    float EditorCamera::rotationSpeed() const {
        return 30.0f;
    }

    glm::quat EditorCamera::getOrientation() const {
        return glm::quat(glm::vec3(glm::radians(-m_pitch), glm::radians(-m_yaw), 0.0f));
    }

    glm::vec3 EditorCamera::getUpDirection() const {
        return glm::rotate(getOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::vec3 EditorCamera::getRightDirection() const {
        return glm::rotate(getOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 EditorCamera::getForwardDirection() const {
        return glm::rotate(getOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
    }
} // namespace NexAur
