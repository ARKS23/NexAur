#include "pch.h"
#include "vulkan_render_data_translator.h"

#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>

namespace NexAur {
    namespace {
        bool isFinite(float value) {
            return std::isfinite(value);
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

    VulkanRenderView VulkanRenderDataTranslator::buildRenderView(const RenderView& render_view) const {
        VulkanRenderView view;
        view.viewport_width = render_view.viewport_width;
        view.viewport_height = render_view.viewport_height;
        view.near_clip = render_view.near_clip;
        view.far_clip = render_view.far_clip;
        view.view_matrix = render_view.view_matrix;
        view.projection_matrix = toVulkanProjection(render_view.projection_matrix);
        view.view_projection_matrix = view.projection_matrix * view.view_matrix;
        view.inverse_view_matrix = render_view.inverse_view_matrix;
        view.inverse_projection_matrix = safeInverse(view.projection_matrix);
        view.camera_position = render_view.camera_position;
        return view;
    }
} // namespace NexAur
