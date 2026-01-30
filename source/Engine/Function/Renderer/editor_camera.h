#pragma once
#include "glm/glm.hpp"

#include "camera.h"
#include "Core/Time/TimeStep.h"
#include "Core/Events/event.h"
#include "Core/Events/mouse_event.h"
#include "Core/Events/window_event.h"

namespace NexAur {
    class EditorCamera : public Camera {
    public:
        EditorCamera() = default;
        EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

        void onUpdate(TimeStep deltaTime);
        void onEvent(Event& event);

        // 更新视图大小
        void setViewportSize(float width, float height);

        // 摄像机状态
        glm::quat getOrientation() const;
        glm::vec3 getUpDirection() const;
        glm::vec3 getRightDirection() const;
        glm::vec3 getForwardDirection() const;
        const glm::vec3& getPosition() const { return m_position; }

        float getPitch() const { return m_pitch; }
        float getYaw() const { return m_yaw; }

    private:
        void updateProjection();
        void updateView();
        void updateViewProjection();

        bool onWindowResize(WindowResizeEvent& event);
        bool onMouseScroll(MouseScrolledEvent& event);
        void onMouseRotate(const glm::vec2& delta);
        void onMousePan(const glm::vec2& delta);
        void onMouseZoom(float delta);

        glm::vec3 calculatePosition() const;
        std::pair<float, float> panSpeed() const;
        float zoomSpeed() const;
        float rotationSpeed() const;

    private:
        float m_fov = 45.0f;
        float m_aspect_ratio = 1.778f;
        float m_near_clip = 0.1f;
        float m_far_clip = 1000.0f;

        glm::vec3 m_position = { 0.0f, 0.0f, 0.0f };
        glm::vec2 m_init_mouse_position = { 0.0f, 0.0f };

        float m_pitch = 0.0f;
        float m_yaw = 0.0f;

        float m_viewport_width = 1920.0f;
        float m_viewport_height = 1080.0f;
    };
}

