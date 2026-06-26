#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace NexAur {
    struct RenderDebugVisualizationOptions {
        bool enabled = false;
        bool camera_frustum = true;
        bool light_gizmos = true;
    };

    struct RenderDebugLine {
        glm::vec3 start{ 0.0f };
        glm::vec3 end{ 0.0f };
        glm::vec4 color{ 1.0f };
        bool depth_test = true;
    };

    struct RenderDebugDrawData {
        std::vector<RenderDebugLine> lines;

        void clear() {
            lines.clear();
        }

        bool empty() const {
            return lines.empty();
        }
    };
} // namespace NexAur
