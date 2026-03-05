#pragma once

#include "glm/glm.hpp"

#include "Core/Base.h"

namespace NexAur {
    class Camera {
    public:
        Camera() = default;
        virtual ~Camera() = default;
        const glm::mat4& getProjection() const { return m_projection; }
        const glm::mat4& getView() const { return m_view; }
        const glm::mat4& getViewProjection() const { return m_view_projection; }
        const glm::vec3& getPosition() const { return m_position; }

    protected:
        glm::mat4 m_projection = glm::mat4(1.0f);
        glm::mat4 m_view = glm::mat4(1.0f);
        glm::mat4 m_view_projection = glm::mat4(1.0f);
        glm::vec3 m_position = { 0.0f, 0.0f, 0.0f };
    };
} // namespace NexAur
