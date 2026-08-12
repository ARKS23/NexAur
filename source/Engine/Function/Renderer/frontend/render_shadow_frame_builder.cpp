#include "render_shadow_frame_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace NexAur {
    namespace {
        constexpr float kDefaultCascadeSplitLambda = 0.65f;

        glm::mat4 toVulkanProjection(const glm::mat4& engine_projection) {
            glm::mat4 clip_transform{ 1.0f };
            clip_transform[1][1] = -1.0f;
            clip_transform[2][2] = 0.5f;
            clip_transform[3][2] = 0.5f;
            return clip_transform * engine_projection;
        }

        glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
            if (glm::dot(value, value) <= 0.000001f) {
                return fallback;
            }
            return glm::normalize(value);
        }

        float sanitizeMin(float value, float fallback, float minimum) {
            return std::isfinite(value) && value >= minimum ? value : fallback;
        }

        float sanitizeUnit(float value, float fallback) {
            if (!std::isfinite(value)) {
                return fallback;
            }
            return std::clamp(value, 0.0f, 1.0f);
        }

        uint32_t sanitizeShadowCascadeCount(const RenderShadowSettings& settings) {
            if (!settings.cascades_enabled) {
                return 1u;
            }

            return std::clamp(settings.cascade_count, 1u, kMaxRenderShadowCascadeCount);
        }

        bool isShadowCascadeDebugView(RenderEffectDebugView view) {
            return view == RenderEffectDebugView::ShadowCascades;
        }

        uint32_t pointShadowFaceLayer(uint32_t shadow_slot, uint32_t face_index) {
            return shadow_slot * kRenderPointShadowCubeFaceCount + face_index;
        }

        glm::mat4 buildPointShadowFaceViewProjection(
            const glm::vec3& light_position,
            float range,
            uint32_t face_index) {
            constexpr std::array<glm::vec3, kRenderPointShadowCubeFaceCount> kFaceDirections{
                glm::vec3{  1.0f,  0.0f,  0.0f },
                glm::vec3{ -1.0f,  0.0f,  0.0f },
                glm::vec3{  0.0f,  1.0f,  0.0f },
                glm::vec3{  0.0f, -1.0f,  0.0f },
                glm::vec3{  0.0f,  0.0f,  1.0f },
                glm::vec3{  0.0f,  0.0f, -1.0f }
            };
            constexpr std::array<glm::vec3, kRenderPointShadowCubeFaceCount> kFaceUps{
                glm::vec3{ 0.0f, -1.0f,  0.0f },
                glm::vec3{ 0.0f, -1.0f,  0.0f },
                glm::vec3{ 0.0f,  0.0f,  1.0f },
                glm::vec3{ 0.0f,  0.0f, -1.0f },
                glm::vec3{ 0.0f, -1.0f,  0.0f },
                glm::vec3{ 0.0f, -1.0f,  0.0f }
            };

            const uint32_t safe_face = std::min(face_index, kRenderPointShadowCubeFaceCount - 1u);
            const float safe_range = std::max(0.1f, range);
            const float near_plane = std::min(0.05f, safe_range * 0.25f);
            const glm::mat4 light_view = glm::lookAt(
                light_position,
                light_position + kFaceDirections[safe_face],
                kFaceUps[safe_face]);
            const glm::mat4 light_projection = glm::perspective(
                glm::radians(90.5f),
                1.0f,
                std::max(0.01f, near_plane),
                safe_range);
            return toVulkanProjection(light_projection) * light_view;
        }

        glm::mat4 buildRectShadowViewProjection(
            const RenderFrameRectLight& light,
            const RenderRectShadowSettings& settings) {
            const glm::vec3 fallback_normal{ 0.0f, -1.0f, 0.0f };
            const glm::vec3 forward = safeNormalize(light.normal, fallback_normal);
            glm::vec3 up = safeNormalize(light.up, glm::vec3{ 0.0f, 0.0f, 1.0f });
            if (std::abs(glm::dot(forward, up)) > 0.98f) {
                up = safeNormalize(light.right, glm::vec3{ 1.0f, 0.0f, 0.0f });
            }

            const glm::mat4 light_view = glm::lookAt(light.position, light.position + forward, up);
            const float width = std::max(0.01f, light.size.x);
            const float height = std::max(0.01f, light.size.y);
            const float margin =
                std::max(width, height) * sanitizeMin(settings.projection_margin, 0.35f, 0.0f);
            const float half_width = width * 0.5f + margin;
            const float half_height = height * 0.5f + margin;
            const float far_plane = sanitizeMin(light.range, 8.0f, 0.1f);
            const glm::mat4 light_projection = glm::ortho(
                -half_width,
                half_width,
                -half_height,
                half_height,
                0.01f,
                far_plane);
            return toVulkanProjection(light_projection) * light_view;
        }

        float computeCascadeSplitDepth(
            float near_clip,
            float far_clip,
            float split_ratio,
            float split_lambda) {
            const float safe_near = std::max(0.001f, near_clip);
            const float safe_far = std::max(safe_near + 0.001f, far_clip);
            const float linear_split = safe_near + (safe_far - safe_near) * split_ratio;
            const float logarithmic_split = safe_near * std::pow(safe_far / safe_near, split_ratio);
            return linear_split * (1.0f - split_lambda) + logarithmic_split * split_lambda;
        }

        float viewDepthToNdcZ(const glm::mat4& projection, float view_depth) {
            const glm::vec4 clip = projection * glm::vec4{ 0.0f, 0.0f, -view_depth, 1.0f };
            if (std::abs(clip.w) <= 0.000001f) {
                return 0.0f;
            }
            return clip.z / clip.w;
        }

        glm::vec3 unprojectNdcPoint(const RenderView& view, const glm::vec3& ndc) {
            glm::vec4 view_position = view.inverse_projection_matrix * glm::vec4{ ndc, 1.0f };
            if (std::abs(view_position.w) > 0.000001f) {
                view_position /= view_position.w;
            }

            glm::vec4 world_position = view.inverse_view_matrix * view_position;
            if (std::abs(world_position.w) > 0.000001f) {
                world_position /= world_position.w;
            }
            return glm::vec3(world_position);
        }

        std::array<glm::vec3, 8> buildCascadeFrustumCorners(
            const RenderView& view,
            float near_depth,
            float far_depth) {
            const float near_z = viewDepthToNdcZ(view.projection_matrix, near_depth);
            const float far_z = viewDepthToNdcZ(view.projection_matrix, far_depth);
            const std::array<glm::vec3, 8> ndc_corners{
                glm::vec3{ -1.0f, -1.0f, near_z },
                glm::vec3{  1.0f, -1.0f, near_z },
                glm::vec3{ -1.0f,  1.0f, near_z },
                glm::vec3{  1.0f,  1.0f, near_z },
                glm::vec3{ -1.0f, -1.0f, far_z },
                glm::vec3{  1.0f, -1.0f, far_z },
                glm::vec3{ -1.0f,  1.0f, far_z },
                glm::vec3{  1.0f,  1.0f, far_z }
            };

            std::array<glm::vec3, 8> corners{};
            for (size_t index = 0; index < ndc_corners.size(); ++index) {
                corners[index] = unprojectNdcPoint(view, ndc_corners[index]);
            }
            return corners;
        }

        glm::mat4 buildCascadeLightViewProjection(
            const std::array<glm::vec3, 8>& corners,
            const glm::vec3& light_direction,
            const glm::vec3& up,
            float shadow_map_size,
            bool stabilize) {
            glm::vec3 center{ 0.0f };
            for (const glm::vec3& corner : corners) {
                center += corner;
            }
            center /= static_cast<float>(corners.size());

            float radius = 0.0f;
            for (const glm::vec3& corner : corners) {
                radius = std::max(radius, glm::length(corner - center));
            }
            radius = std::max(radius, 1.0f);

            const glm::vec3 eye = center - light_direction * (radius + 10.0f);
            const glm::mat4 light_view = glm::lookAt(eye, center, up);

            glm::vec3 min_bounds{ std::numeric_limits<float>::max() };
            glm::vec3 max_bounds{ std::numeric_limits<float>::lowest() };
            for (const glm::vec3& corner : corners) {
                const glm::vec3 light_space_corner = glm::vec3(light_view * glm::vec4(corner, 1.0f));
                min_bounds = glm::min(min_bounds, light_space_corner);
                max_bounds = glm::max(max_bounds, light_space_corner);
            }

            const float half_extent = std::max(
                0.5f,
                std::max(max_bounds.x - min_bounds.x, max_bounds.y - min_bounds.y) * 0.5f);
            glm::vec2 center_xy{
                (min_bounds.x + max_bounds.x) * 0.5f,
                (min_bounds.y + max_bounds.y) * 0.5f
            };

            if (stabilize && shadow_map_size > 1.0f) {
                const float texel_world_size = (half_extent * 2.0f) / shadow_map_size;
                if (texel_world_size > 0.0f && std::isfinite(texel_world_size)) {
                    center_xy.x = std::round(center_xy.x / texel_world_size) * texel_world_size;
                    center_xy.y = std::round(center_xy.y / texel_world_size) * texel_world_size;
                }
            }

            constexpr float kCascadeDepthPadding = 10.0f;
            const float near_plane = std::max(0.01f, -max_bounds.z - kCascadeDepthPadding);
            const float far_plane = std::max(near_plane + 1.0f, -min_bounds.z + kCascadeDepthPadding);
            const glm::mat4 light_projection = glm::ortho(
                center_xy.x - half_extent,
                center_xy.x + half_extent,
                center_xy.y - half_extent,
                center_xy.y + half_extent,
                near_plane,
                far_plane);
            return toVulkanProjection(light_projection) * light_view;
        }

        glm::vec3 stabilizeShadowCenter(
            const glm::vec3& center,
            const glm::vec3& light_direction,
            const glm::vec3& up,
            float radius,
            float shadow_map_size,
            bool enabled) {
            if (!enabled || radius <= 0.0f || shadow_map_size <= 1.0f) {
                return center;
            }

            const float texel_world_size = (radius * 2.0f) / shadow_map_size;
            if (texel_world_size <= 0.0f || !std::isfinite(texel_world_size)) {
                return center;
            }

            const glm::mat4 light_basis = glm::lookAt(glm::vec3{ 0.0f }, light_direction, up);
            glm::vec4 light_space_center = light_basis * glm::vec4(center, 1.0f);
            light_space_center.x = std::round(light_space_center.x / texel_world_size) * texel_world_size;
            light_space_center.y = std::round(light_space_center.y / texel_world_size) * texel_world_size;

            const glm::vec4 snapped_center = glm::inverse(light_basis) * light_space_center;
            return glm::vec3(snapped_center);
        }
    } // namespace

    RenderShadowCascadeFrame RenderShadowFrameBuilder::buildDirectionalShadowFrame(
        const RenderView& view,
        const RenderFrameDirectionalLight& light,
        const RenderShadowSettings& settings,
        const RenderEffectDebugSettings& debug_settings,
        float shadow_map_size) const {
        RenderShadowCascadeFrame frame;
        if (!settings.enabled || !light.cast_shadow) {
            return frame;
        }

        const float distance = sanitizeMin(
            settings.distance,
            light.shadow_distance,
            1.0f);
        const glm::vec3 fallback_light_direction = glm::normalize(glm::vec3{ -0.2f, -1.0f, -0.3f });
        const glm::vec3 light_direction = safeNormalize(light.direction, fallback_light_direction);
        const glm::vec3 camera_forward = safeNormalize(
            -glm::vec3(view.inverse_view_matrix[2]),
            glm::vec3{ 0.0f, 0.0f, -1.0f });

        const glm::vec3 center = view.camera_position + camera_forward * (distance * 0.5f);
        const glm::vec3 world_up{ 0.0f, 1.0f, 0.0f };
        const glm::vec3 up = std::abs(glm::dot(light_direction, world_up)) > 0.95f ?
            glm::vec3{ 0.0f, 0.0f, 1.0f } :
            world_up;

        const uint32_t cascade_count = sanitizeShadowCascadeCount(settings);
        frame.cascade_count = cascade_count;
        frame.cascades_enabled = cascade_count > 1;
        frame.debug_overlay =
            frame.cascades_enabled &&
            (settings.cascade_debug_overlay || isShadowCascadeDebugView(debug_settings.view));

        if (cascade_count > 1) {
            const float near_clip = std::max(0.001f, view.near_clip);
            const float far_clip = std::max(near_clip + 0.001f, distance);
            const float split_lambda = sanitizeUnit(
                settings.cascade_split_lambda,
                kDefaultCascadeSplitLambda);
            float cascade_near = near_clip;
            for (uint32_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index) {
                const float split_ratio = static_cast<float>(cascade_index + 1u) /
                    static_cast<float>(cascade_count);
                const float cascade_far = cascade_index + 1u == cascade_count ?
                    far_clip :
                    computeCascadeSplitDepth(near_clip, far_clip, split_ratio, split_lambda);
                const std::array<glm::vec3, 8> corners =
                    buildCascadeFrustumCorners(view, cascade_near, cascade_far);
                frame.light_view_projections[cascade_index] = buildCascadeLightViewProjection(
                    corners,
                    light_direction,
                    up,
                    shadow_map_size,
                    settings.stabilize);
                frame.split_depths[cascade_index] = cascade_far;
                cascade_near = cascade_far;
            }
            return frame;
        }

        const float radius = std::max(5.0f, distance);
        const glm::vec3 stable_center = stabilizeShadowCenter(
            center,
            light_direction,
            up,
            radius,
            shadow_map_size,
            settings.stabilize);
        const glm::vec3 eye = stable_center - light_direction * distance;
        const glm::mat4 light_view = glm::lookAt(eye, stable_center, up);
        const glm::mat4 light_projection = glm::ortho(
            -radius,
            radius,
            -radius,
            radius,
            0.1f,
            distance * 3.0f);
        frame.light_view_projections[0] = toVulkanProjection(light_projection) * light_view;
        frame.split_depths[0] = distance;
        return frame;
    }

    RenderPointShadowFrame RenderShadowFrameBuilder::buildPointShadowFrame(
        const std::vector<RenderFramePointLight>& lights,
        const RenderPointShadowSettings& settings,
        uint32_t light_capacity) const {
        RenderPointShadowFrame frame;
        if (!settings.enabled) {
            return frame;
        }

        const uint32_t safe_capacity = std::min(light_capacity, kMaxRenderPointShadowLights);
        for (const RenderFramePointLight& light : lights) {
            if (!light.cast_shadow || light.shadow_slot < 0) {
                continue;
            }

            const uint32_t shadow_slot = static_cast<uint32_t>(light.shadow_slot);
            if (shadow_slot >= safe_capacity) {
                continue;
            }

            const float range = sanitizeMin(light.shadow_range, 8.0f, 0.1f);
            for (uint32_t face_index = 0; face_index < kRenderPointShadowCubeFaceCount; ++face_index) {
                frame.light_view_projections[pointShadowFaceLayer(shadow_slot, face_index)] =
                    buildPointShadowFaceViewProjection(light.position, range, face_index);
            }

            frame.shadowed_light_count = std::max(frame.shadowed_light_count, shadow_slot + 1u);
        }

        frame.face_count = frame.shadowed_light_count * kRenderPointShadowCubeFaceCount;
        frame.enabled = frame.shadowed_light_count > 0;
        return frame;
    }

    RenderRectShadowFrame RenderShadowFrameBuilder::buildRectShadowFrame(
        const std::vector<RenderFrameRectLight>& lights,
        const RenderRectShadowSettings& settings,
        uint32_t light_capacity) const {
        RenderRectShadowFrame frame;
        if (!settings.enabled) {
            return frame;
        }

        const uint32_t safe_capacity = std::min(light_capacity, kMaxRenderRectShadowLights);
        for (const RenderFrameRectLight& light : lights) {
            if (!light.cast_shadow || light.shadow_slot < 0) {
                continue;
            }

            const uint32_t shadow_slot = static_cast<uint32_t>(light.shadow_slot);
            if (shadow_slot >= safe_capacity) {
                continue;
            }

            frame.light_view_projections[shadow_slot] =
                buildRectShadowViewProjection(light, settings);
            frame.shadowed_light_count = std::max(frame.shadowed_light_count, shadow_slot + 1u);
        }

        frame.enabled = frame.shadowed_light_count > 0;
        return frame;
    }
} // namespace NexAur
