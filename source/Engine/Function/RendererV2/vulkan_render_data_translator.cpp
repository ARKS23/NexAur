#include "pch.h"
#include "vulkan_render_data_translator.h"

#include "Function/Renderer/data/render_data.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>

namespace NexAur {
    namespace {
        constexpr float kDefaultNearClip = 0.1f;
        constexpr float kDefaultFarClip = 1000.0f;
        constexpr float kMinClipDistance = 0.001f;

        bool isFinite(float value) {
            return std::isfinite(value);
        }

        bool isFinite(const glm::vec3& value) {
            return isFinite(value.x) && isFinite(value.y) && isFinite(value.z);
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

        glm::mat4 sanitizeMatrix(const glm::mat4& value, const glm::mat4& fallback) {
            return isFinite(value) ? value : fallback;
        }

        glm::vec3 sanitizePosition(const glm::vec3& value) {
            return isFinite(value) ? value : glm::vec3{ 0.0f };
        }

        float sanitizeNearClip(float value) {
            return isFinite(value) && value > 0.0f ? value : kDefaultNearClip;
        }

        float sanitizeFarClip(float value, float near_clip) {
            if (isFinite(value) && value > near_clip + kMinClipDistance) {
                return value;
            }
            return std::max(kDefaultFarClip, near_clip + kMinClipDistance);
        }

        glm::mat4 toVulkanProjection(const glm::mat4& engine_projection) {
            glm::mat4 clip_transform{ 1.0f };
            clip_transform[1][1] = -1.0f;
            clip_transform[2][2] = 0.5f;
            clip_transform[3][2] = 0.5f;
            return clip_transform * engine_projection;
        }

        glm::mat4 safeInverse(const glm::mat4& value) {
            const glm::mat4 inverse = glm::inverse(value);
            return isFinite(inverse) ? inverse : glm::mat4{ 1.0f };
        }
    } // namespace

    void VulkanRenderDataTranslator::resetFrame() {}

    RenderView VulkanRenderDataTranslator::buildRenderView(
        const RenderDataPacket& render_data,
        uint32_t viewport_width,
        uint32_t viewport_height) const {
        const RendererCameraData& camera = render_data.camera_data;

        RenderView view;
        view.viewport_width = std::max(1u, viewport_width);
        view.viewport_height = std::max(1u, viewport_height);

        view.near_clip = sanitizeNearClip(camera.near_clip);
        view.far_clip = sanitizeFarClip(camera.far_clip, view.near_clip);

        view.view_matrix = sanitizeMatrix(camera.view_matrix, glm::mat4{ 1.0f });
        const glm::mat4 engine_projection = sanitizeMatrix(camera.projection_matrix, glm::mat4{ 1.0f });
        view.projection_matrix = sanitizeMatrix(toVulkanProjection(engine_projection), glm::mat4{ 1.0f });
        view.view_projection_matrix = view.projection_matrix * view.view_matrix;

        view.inverse_view_matrix = safeInverse(view.view_matrix);
        view.inverse_projection_matrix = safeInverse(view.projection_matrix);
        view.camera_position = sanitizePosition(camera.position);

        return view;
    }
} // namespace NexAur
