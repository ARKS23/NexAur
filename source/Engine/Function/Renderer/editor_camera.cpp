#include "pch.h"
#include "editor_camera.h"
#include "Function/Input/input_system.h"
#include "Function/Global/global_context.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace NexAur {
    EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
        : m_fov(fov), m_aspect_ratio(aspectRatio), m_near_clip(nearClip), m_far_clip(farClip) {
        updateProjection();
        updateView();
    }

    void EditorCamera::updateProjection() {
        m_aspect_ratio = m_viewport_width / m_viewport_height;
        m_projection = glm::perspective(glm::radians(m_fov), m_aspect_ratio, m_near_clip, m_far_clip);
    }

    void EditorCamera::updateView() {
        // FPS模式
        glm::quat orientation = glm::quat(glm::vec3(-m_pitch, -m_yaw, 0.0f));
        m_view = glm::translate(glm::mat4(1.0f), m_position) * glm::toMat4(orientation);
        m_view = glm::inverse(m_view); 
    }

    void EditorCamera::setViewportSize(float width, float height) {
        m_viewport_width = width;
        m_viewport_height = height;
        updateProjection();
    }

    void EditorCamera::onUpdate(TimeStep deltaTime) {
        std::shared_ptr<InputSystem> input_system = g_runtime_global_context.m_input_system;

        // 鼠标控制视角旋转
        const glm::vec2& mouse{ input_system->getMousePositionX(), input_system->getMousePositionY() };
        glm::vec2 delta = (mouse - m_init_mouse_position) * 0.003f; // 灵敏度系数，可提取为变量
        m_init_mouse_position = mouse;

        onMouseRotate(delta);

        // WSAD移动
        float speed = 5.f * (float)(deltaTime);
        if (input_system->isKeyPressed(KeyCode::W)) 
            m_position += getForwardDirection() * speed;
        if (input_system->isKeyPressed(KeyCode::S)) 
            m_position -= getForwardDirection() * speed;
        if (input_system->isKeyPressed(KeyCode::A))
            m_position -= getRightDirection() * speed;
        if (input_system->isKeyPressed(KeyCode::D))
            m_position += getRightDirection() * speed;

        updateView();
    }

    void EditorCamera::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<MouseScrolledEvent>(NX_BIND_EVENT_FN(EditorCamera::onMouseScroll));
        dispatcher.dispatch<WindowResizeEvent>(NX_BIND_EVENT_FN(EditorCamera::onWindowResize));
    }

    bool EditorCamera::onWindowResize(WindowResizeEvent& event) {
        setViewportSize((float)event.getWidth(), (float)event.getHeight());
        return false;
    }

    bool EditorCamera::onMouseScroll(MouseScrolledEvent& event) {
        float delta = event.GetYOffset() * zoomSpeed();
        onMouseZoom(delta);
        updateView();

        return false;
    }

    void EditorCamera::onMouseRotate(const glm::vec2& delta) {
        float yaw_sign = getUpDirection().y < 0 ? -1.0f : 1.0f;

        m_yaw += yaw_sign * delta.x * rotationSpeed();
        m_pitch += delta.y * rotationSpeed();

        // 限制俯仰角度，防止翻转
        m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

        updateView();
    }

    void EditorCamera::onMouseZoom(float delta) {
        //TODO
    }

    float EditorCamera::zoomSpeed() const {
        return 2.0f;
    }

    float EditorCamera::rotationSpeed() const {
        return 0.8f;
    }

    void EditorCamera::onMousePan(const glm::vec2& delta) {
        // 暂时不用
    }

    glm::vec3 EditorCamera::calculatePosition() const {
        // 暂时不用
        return m_position;
    }

    std::pair<float, float> EditorCamera::panSpeed() const {
        // 暂时不用
        return { 0.0f, 0.0f };
    }

    glm::quat EditorCamera::getOrientation() const {
        return glm::quat(glm::vec3(-m_pitch, -m_yaw, 0.0f));
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
