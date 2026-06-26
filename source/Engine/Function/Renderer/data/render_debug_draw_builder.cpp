#include "pch.h"
#include "render_debug_draw_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <glm/gtc/matrix_inverse.hpp>

namespace NexAur {
    namespace {
        bool isFinite(float value) {
            return std::isfinite(value);
        }

        bool isFinite(const glm::vec3& value) {
            return isFinite(value.x) && isFinite(value.y) && isFinite(value.z);
        }

        bool isFinite(const glm::vec4& value) {
            return isFinite(value.x) && isFinite(value.y) && isFinite(value.z) && isFinite(value.w);
        }

        bool isFinite(const glm::mat4& value) {
            for (int column = 0; column < 4; ++column) {
                for (int row = 0; row < 4; ++row) {
                    if (!isFinite(value[column][row])) {
                        return false;
                    }
                }
            }
            return true;
        }

        glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
            if (!isFinite(value) || glm::dot(value, value) <= 0.000001f) {
                return fallback;
            }

            return glm::normalize(value);
        }

        glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point) {
            const glm::vec4 transformed = transform * glm::vec4(point, 1.0f);
            if (std::abs(transformed.w) <= 0.000001f) {
                return glm::vec3(transformed);
            }

            return glm::vec3(transformed) / transformed.w;
        }

        void addBoxEdges(
            RenderDebugDrawData& debug_draw,
            const std::array<glm::vec3, 8>& corners,
            const glm::vec4& color,
            bool depth_test) {
            constexpr std::array<std::pair<size_t, size_t>, 12> kEdges{
                std::pair<size_t, size_t>{ 0, 1 },
                std::pair<size_t, size_t>{ 1, 3 },
                std::pair<size_t, size_t>{ 3, 2 },
                std::pair<size_t, size_t>{ 2, 0 },
                std::pair<size_t, size_t>{ 4, 5 },
                std::pair<size_t, size_t>{ 5, 7 },
                std::pair<size_t, size_t>{ 7, 6 },
                std::pair<size_t, size_t>{ 6, 4 },
                std::pair<size_t, size_t>{ 0, 4 },
                std::pair<size_t, size_t>{ 1, 5 },
                std::pair<size_t, size_t>{ 2, 6 },
                std::pair<size_t, size_t>{ 3, 7 },
            };

            for (const auto& [start, end] : kEdges) {
                RenderDebugDrawBuilder::addLine(debug_draw, corners[start], corners[end], color, depth_test);
            }
        }
    } // namespace

    void RenderDebugDrawBuilder::addLine(
        RenderDebugDrawData& debug_draw,
        const glm::vec3& start,
        const glm::vec3& end,
        const glm::vec4& color,
        bool depth_test) {
        if (!isFinite(start) || !isFinite(end) || !isFinite(color)) {
            return;
        }

        RenderDebugLine line;
        line.start = start;
        line.end = end;
        line.color = glm::max(color, glm::vec4{ 0.0f });
        line.depth_test = depth_test;
        debug_draw.lines.push_back(line);
    }

    void RenderDebugDrawBuilder::addAABB(
        RenderDebugDrawData& debug_draw,
        const glm::vec3& min,
        const glm::vec3& max,
        const glm::vec4& color,
        bool depth_test) {
        if (!isFinite(min) || !isFinite(max)) {
            return;
        }

        const glm::vec3 box_min = glm::min(min, max);
        const glm::vec3 box_max = glm::max(min, max);
        const std::array<glm::vec3, 8> corners{
            glm::vec3{ box_min.x, box_min.y, box_min.z },
            glm::vec3{ box_max.x, box_min.y, box_min.z },
            glm::vec3{ box_min.x, box_max.y, box_min.z },
            glm::vec3{ box_max.x, box_max.y, box_min.z },
            glm::vec3{ box_min.x, box_min.y, box_max.z },
            glm::vec3{ box_max.x, box_min.y, box_max.z },
            glm::vec3{ box_min.x, box_max.y, box_max.z },
            glm::vec3{ box_max.x, box_max.y, box_max.z },
        };

        addBoxEdges(debug_draw, corners, color, depth_test);
    }

    void RenderDebugDrawBuilder::addAABB(
        RenderDebugDrawData& debug_draw,
        const glm::mat4& transform,
        const glm::vec3& local_min,
        const glm::vec3& local_max,
        const glm::vec4& color,
        bool depth_test) {
        if (!isFinite(transform) || !isFinite(local_min) || !isFinite(local_max)) {
            return;
        }

        const glm::vec3 box_min = glm::min(local_min, local_max);
        const glm::vec3 box_max = glm::max(local_min, local_max);
        const std::array<glm::vec3, 8> corners{
            transformPoint(transform, { box_min.x, box_min.y, box_min.z }),
            transformPoint(transform, { box_max.x, box_min.y, box_min.z }),
            transformPoint(transform, { box_min.x, box_max.y, box_min.z }),
            transformPoint(transform, { box_max.x, box_max.y, box_min.z }),
            transformPoint(transform, { box_min.x, box_min.y, box_max.z }),
            transformPoint(transform, { box_max.x, box_min.y, box_max.z }),
            transformPoint(transform, { box_min.x, box_max.y, box_max.z }),
            transformPoint(transform, { box_max.x, box_max.y, box_max.z }),
        };

        addBoxEdges(debug_draw, corners, color, depth_test);
    }

    void RenderDebugDrawBuilder::addFrustum(
        RenderDebugDrawData& debug_draw,
        const glm::mat4& view_projection,
        const glm::vec4& color,
        bool depth_test) {
        if (!isFinite(view_projection) || std::abs(glm::determinant(view_projection)) <= 0.000001f) {
            return;
        }

        const glm::mat4 inverse_view_projection = glm::inverse(view_projection);
        const std::array<glm::vec3, 8> ndc_corners{
            glm::vec3{ -1.0f, -1.0f, -1.0f },
            glm::vec3{  1.0f, -1.0f, -1.0f },
            glm::vec3{ -1.0f,  1.0f, -1.0f },
            glm::vec3{  1.0f,  1.0f, -1.0f },
            glm::vec3{ -1.0f, -1.0f,  1.0f },
            glm::vec3{  1.0f, -1.0f,  1.0f },
            glm::vec3{ -1.0f,  1.0f,  1.0f },
            glm::vec3{  1.0f,  1.0f,  1.0f },
        };

        std::array<glm::vec3, 8> world_corners{};
        for (size_t index = 0; index < ndc_corners.size(); ++index) {
            world_corners[index] = transformPoint(inverse_view_projection, ndc_corners[index]);
        }

        addBoxEdges(debug_draw, world_corners, color, depth_test);
    }

    void RenderDebugDrawBuilder::addDirectionalLightGizmo(
        RenderDebugDrawData& debug_draw,
        const glm::vec3& position,
        const glm::vec3& direction,
        const glm::vec4& color,
        bool depth_test) {
        const glm::vec3 light_direction = safeNormalize(direction, glm::vec3{ -0.2f, -1.0f, -0.3f });
        const glm::vec3 start = position - light_direction * 1.5f;
        const glm::vec3 end = position;
        const glm::vec3 tangent = safeNormalize(glm::cross(light_direction, glm::vec3{ 0.0f, 1.0f, 0.0f }), glm::vec3{ 1.0f, 0.0f, 0.0f });
        const glm::vec3 bitangent = safeNormalize(glm::cross(light_direction, tangent), glm::vec3{ 0.0f, 0.0f, 1.0f });

        addLine(debug_draw, start, end, color, depth_test);
        addLine(debug_draw, start - tangent * 0.25f, start + tangent * 0.25f, color, depth_test);
        addLine(debug_draw, start - bitangent * 0.25f, start + bitangent * 0.25f, color, depth_test);
        addLine(debug_draw, end, end - light_direction * 0.25f + tangent * 0.2f, color, depth_test);
        addLine(debug_draw, end, end - light_direction * 0.25f - tangent * 0.2f, color, depth_test);
    }

    void RenderDebugDrawBuilder::addPointLightGizmo(
        RenderDebugDrawData& debug_draw,
        const glm::vec3& position,
        float radius,
        const glm::vec4& color,
        bool depth_test) {
        if (!isFinite(position) || !isFinite(radius)) {
            return;
        }

        radius = std::max(radius, 0.05f);
        addLine(debug_draw, position - glm::vec3{ radius, 0.0f, 0.0f }, position + glm::vec3{ radius, 0.0f, 0.0f }, color, depth_test);
        addLine(debug_draw, position - glm::vec3{ 0.0f, radius, 0.0f }, position + glm::vec3{ 0.0f, radius, 0.0f }, color, depth_test);
        addLine(debug_draw, position - glm::vec3{ 0.0f, 0.0f, radius }, position + glm::vec3{ 0.0f, 0.0f, radius }, color, depth_test);
        addAABB(
            debug_draw,
            position - glm::vec3{ radius },
            position + glm::vec3{ radius },
            glm::vec4{ color.r, color.g, color.b, color.a * 0.65f },
            depth_test);
    }
} // namespace NexAur
