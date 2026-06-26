#pragma once

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Renderer/data/render_debug_draw.h"

namespace NexAur {
    class NEXAUR_API RenderDebugDrawBuilder {
    public:
        static void addLine(
            RenderDebugDrawData& debug_draw,
            const glm::vec3& start,
            const glm::vec3& end,
            const glm::vec4& color,
            bool depth_test = true);

        static void addAABB(
            RenderDebugDrawData& debug_draw,
            const glm::vec3& min,
            const glm::vec3& max,
            const glm::vec4& color,
            bool depth_test = true);

        static void addAABB(
            RenderDebugDrawData& debug_draw,
            const glm::mat4& transform,
            const glm::vec3& local_min,
            const glm::vec3& local_max,
            const glm::vec4& color,
            bool depth_test = true);

        static void addFrustum(
            RenderDebugDrawData& debug_draw,
            const glm::mat4& view_projection,
            const glm::vec4& color,
            bool depth_test = true);

        static void addDirectionalLightGizmo(
            RenderDebugDrawData& debug_draw,
            const glm::vec3& position,
            const glm::vec3& direction,
            const glm::vec4& color,
            bool depth_test = true);

        static void addPointLightGizmo(
            RenderDebugDrawData& debug_draw,
            const glm::vec3& position,
            float radius,
            const glm::vec4& color,
            bool depth_test = true);
    };
} // namespace NexAur
