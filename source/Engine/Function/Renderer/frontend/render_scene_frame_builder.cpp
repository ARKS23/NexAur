#include "pch.h"
#include "render_scene_frame_builder.h"

#include "Function/Renderer/data/render_data.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace NexAur {
    namespace {
        constexpr size_t kMaxPointLights = 64;
        constexpr float kDefaultEnvironmentIntensity = 0.65f;
        constexpr float kDefaultSkyboxIntensity = 0.75f;
        constexpr float kDefaultIblIntensity = 0.65f;
        constexpr glm::vec3 kDefaultLightDirection{ -0.2f, -1.0f, -0.3f };

        bool isFinite(float value) {
            return std::isfinite(value);
        }

        bool isFinite(const glm::vec3& value) {
            return isFinite(value.x) && isFinite(value.y) && isFinite(value.z);
        }

        bool isFinite(const glm::vec2& value) {
            return isFinite(value.x) && isFinite(value.y);
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

        float sanitizeNonNegative(float value, float fallback) {
            return isFinite(value) && value >= 0.0f ? value : fallback;
        }

        float sanitizeScale(float value) {
            return sanitizeNonNegative(value, 1.0f);
        }

        float sanitizeUnit(float value, float fallback) {
            if (!isFinite(value)) {
                return fallback;
            }
            return std::clamp(value, 0.0f, 1.0f);
        }

        float sanitizeMin(float value, float fallback, float minimum) {
            return isFinite(value) && value >= minimum ? value : fallback;
        }

        glm::vec3 sanitizeColor(const glm::vec3& color) {
            if (!isFinite(color)) {
                return glm::vec3{ 1.0f };
            }
            return glm::max(color, glm::vec3{ 0.0f });
        }

        glm::vec3 sanitizeDirection(const glm::vec3& direction) {
            if (!isFinite(direction) || glm::dot(direction, direction) <= 0.000001f) {
                return glm::normalize(kDefaultLightDirection);
            }
            return glm::normalize(direction);
        }

        glm::vec3 sanitizeAxis(const glm::vec3& axis, const glm::vec3& fallback) {
            if (!isFinite(axis) || glm::dot(axis, axis) <= 0.000001f) {
                return fallback;
            }
            return glm::normalize(axis);
        }

        glm::mat4 sanitizeTransform(const glm::mat4& transform) {
            return isFinite(transform) ? transform : glm::mat4{ 1.0f };
        }

        RenderFrameDirectionalLight buildDirectionalLight(
            const RendererDirectionalLightData& source,
            const RenderLightingCalibrationSettings& lighting) {
            RenderFrameDirectionalLight light;
            light.direction = sanitizeDirection(source.direction);
            light.color = sanitizeColor(source.color);
            light.intensity =
                sanitizeNonNegative(source.intensity, 1.0f) *
                sanitizeScale(lighting.directional_light_intensity_scale);
            light.cast_shadow = source.cast_shadow;
            light.shadow_strength = sanitizeUnit(source.shadow_strength, 0.65f);
            light.shadow_bias = sanitizeMin(source.shadow_bias, 0.002f, 0.0f);
            light.shadow_distance = sanitizeMin(source.shadow_distance, 30.0f, 1.0f);
            if (light.intensity <= 0.0001f) {
                light.cast_shadow = false;
            }
            return light;
        }

        RenderFramePointLight buildPointLight(
            const RendererPointLightData& source,
            const RenderLightingCalibrationSettings& lighting,
            const RenderPointShadowSettings& point_shadow_settings,
            uint32_t& assigned_shadow_slots) {
            RenderFramePointLight light;
            light.position = isFinite(source.position) ? source.position : glm::vec3{ 0.0f };
            light.color = sanitizeColor(source.color);
            light.intensity =
                sanitizeNonNegative(source.intensity, 1.0f) *
                sanitizeScale(lighting.point_light_intensity_scale);
            light.constant = sanitizeNonNegative(source.constant, 1.0f);
            light.linear = sanitizeNonNegative(source.linear, 0.09f);
            light.quadratic = sanitizeNonNegative(source.quadratic, 0.032f);
            light.shadow_range = sanitizeMin(source.shadow_range, 8.0f, 0.1f);
            light.shadow_strength = sanitizeUnit(source.shadow_strength, 0.85f);
            light.shadow_requested = source.cast_shadow && light.intensity > 0.0001f;

            const uint32_t shadow_budget = std::clamp(
                point_shadow_settings.max_shadowed_lights,
                0u,
                kMaxRenderPointShadowLights);
            const bool can_cast_shadow =
                point_shadow_settings.enabled &&
                light.shadow_requested &&
                assigned_shadow_slots < shadow_budget;
            light.cast_shadow = can_cast_shadow;
            light.shadow_slot = can_cast_shadow ? static_cast<int32_t>(assigned_shadow_slots++) : -1;
            return light;
        }

        RenderFrameRectLight buildRectLight(
            const RendererRectLightData& source,
            const RenderLightingCalibrationSettings& lighting,
            const RenderRectShadowSettings& rect_shadow_settings,
            uint32_t& assigned_shadow_slots) {
            RenderFrameRectLight light;
            light.position = isFinite(source.position) ? source.position : glm::vec3{ 0.0f };
            light.right = sanitizeAxis(source.right, glm::vec3{ 1.0f, 0.0f, 0.0f });
            light.up = sanitizeAxis(source.up, glm::vec3{ 0.0f, 0.0f, 1.0f });
            light.normal = sanitizeAxis(source.normal, glm::vec3{ 0.0f, -1.0f, 0.0f });
            if (std::abs(glm::dot(light.right, light.up)) > 0.98f) {
                light.up = sanitizeAxis(glm::cross(light.normal, light.right), glm::vec3{ 0.0f, 0.0f, 1.0f });
            }
            light.size = isFinite(source.size)
                ? glm::max(source.size, glm::vec2{ 0.01f })
                : glm::vec2{ 1.0f };
            light.color = sanitizeColor(source.color);
            light.intensity =
                sanitizeNonNegative(source.intensity, 8.0f) *
                sanitizeScale(lighting.rect_light_intensity_scale);
            light.range = sanitizeMin(source.range, 8.0f, 0.1f);
            light.two_sided = source.two_sided;
            light.shadow_strength = sanitizeUnit(source.shadow_strength, 0.85f);
            light.shadow_requested = source.cast_shadow && light.intensity > 0.0001f;

            const uint32_t shadow_budget = std::clamp(
                rect_shadow_settings.max_shadowed_lights,
                0u,
                kMaxRenderRectShadowLights);
            const bool can_cast_shadow =
                rect_shadow_settings.enabled &&
                light.shadow_requested &&
                assigned_shadow_slots < shadow_budget;
            light.cast_shadow = can_cast_shadow;
            light.shadow_slot = can_cast_shadow ? static_cast<int32_t>(assigned_shadow_slots++) : -1;
            return light;
        }

        RenderFrameReflectionProbe buildReflectionProbe(const RendererReflectionProbeData& source) {
            RenderFrameReflectionProbe probe;
            probe.environment_asset = source.environment_asset;
            probe.position = isFinite(source.position) ? source.position : glm::vec3{ 0.0f };
            probe.box_extents = isFinite(source.box_extents)
                ? glm::max(source.box_extents, glm::vec3{ 0.05f })
                : glm::vec3{ 4.0f, 3.0f, 4.0f };
            probe.intensity = sanitizeNonNegative(source.intensity, 1.0f);
            probe.blend_distance = sanitizeMin(source.blend_distance, 0.75f, 0.0f);
            probe.capture_resolution = std::clamp(source.capture_resolution, 32u, 1024u);
            probe.capture_near_clip = sanitizeMin(source.capture_near_clip, 0.1f, 0.001f);
            probe.capture_far_clip =
                std::max(sanitizeMin(source.capture_far_clip, 40.0f, 0.01f), probe.capture_near_clip + 0.001f);
            probe.entity_id = source.entity_id;
            probe.box_projection = source.box_projection;
            probe.capture_include_skybox = source.capture_include_skybox;
            probe.capture_dirty = source.capture_dirty;
            return probe;
        }

        void appendObjects(
            const std::vector<RenderObjectData>& source_objects,
            std::vector<RenderSceneFrameObject>& target_objects,
            const char* list_name) {
            target_objects.reserve(source_objects.size());

            size_t invalid_count = 0;
            for (const RenderObjectData& source : source_objects) {
                if (!source.model_asset) {
                    ++invalid_count;
                    continue;
                }

                RenderSceneFrameObject object;
                object.model_asset = source.model_asset;
                object.transform = sanitizeTransform(source.transform);
                object.entity_id = source.entity_id;
                target_objects.push_back(object);
            }

            if (invalid_count > 0) {
                NX_CORE_WARN(
                    "RenderSceneFrameBuilder skipped {} {} object(s) with invalid model assets.",
                    invalid_count,
                    list_name);
            }
        }

        void appendDebugLines(
            const RenderDebugDrawData& source,
            RenderDebugDrawData& target) {
            constexpr size_t kMaxDebugLines = 65536;

            const size_t line_count = std::min(source.lines.size(), kMaxDebugLines);
            target.lines.reserve(line_count);

            size_t invalid_count = 0;
            for (size_t index = 0; index < line_count; ++index) {
                const RenderDebugLine& source_line = source.lines[index];
                if (!isFinite(source_line.start) ||
                    !isFinite(source_line.end) ||
                    !isFinite(source_line.color)) {
                    ++invalid_count;
                    continue;
                }

                RenderDebugLine line;
                line.start = source_line.start;
                line.end = source_line.end;
                line.color = glm::max(source_line.color, glm::vec4{ 0.0f });
                line.depth_test = source_line.depth_test;
                target.lines.push_back(line);
            }

            if (source.lines.size() > kMaxDebugLines) {
                NX_CORE_WARN(
                    "RenderSceneFrameBuilder truncated debug lines from {} to {}.",
                    source.lines.size(),
                    kMaxDebugLines);
            }

            if (invalid_count > 0) {
                NX_CORE_WARN(
                    "RenderSceneFrameBuilder skipped {} invalid debug line(s).",
                    invalid_count);
            }
        }
    } // namespace

    RenderSceneFrame RenderSceneFrameBuilder::buildRenderSceneFrame(
        const RenderDataPacket& render_data,
        const RenderView& render_view) const {
        RenderSceneFrame frame;
        frame.view = render_view;
        const RenderLightingCalibrationSettings& lighting = render_data.render_settings.lighting;

        appendObjects(render_data.opaque_objects, frame.opaque_objects, "opaque");
        appendObjects(render_data.transparent_objects, frame.transparent_objects, "transparent");

        frame.directional_light = buildDirectionalLight(render_data.directional_light_data, lighting);

        const size_t point_light_count = std::min(render_data.point_lights_data.size(), kMaxPointLights);
        frame.point_lights.reserve(point_light_count);
        uint32_t assigned_shadow_slots = 0;
        for (size_t index = 0; index < point_light_count; ++index) {
            frame.point_lights.push_back(buildPointLight(
                render_data.point_lights_data[index],
                lighting,
                render_data.render_settings.point_shadow,
                assigned_shadow_slots));
        }

        if (render_data.point_lights_data.size() > kMaxPointLights) {
            NX_CORE_WARN(
                "RenderSceneFrameBuilder truncated point lights from {} to {}.",
                render_data.point_lights_data.size(),
                kMaxPointLights);
        }

        if (render_data.render_settings.rect_light.enabled) {
            const size_t rect_light_budget = std::min<size_t>(
                render_data.render_settings.rect_light.max_lights,
                kMaxRenderRectLights);
            const size_t rect_light_count = std::min(render_data.rect_lights_data.size(), rect_light_budget);
            frame.rect_lights.reserve(rect_light_count);
            uint32_t assigned_rect_shadow_slots = 0;
            for (size_t index = 0; index < rect_light_count; ++index) {
                RenderFrameRectLight rect_light = buildRectLight(
                    render_data.rect_lights_data[index],
                    lighting,
                    render_data.render_settings.rect_shadow,
                    assigned_rect_shadow_slots);
                if (rect_light.intensity > 0.0001f) {
                    frame.rect_lights.push_back(rect_light);
                }
            }

            if (render_data.rect_lights_data.size() > rect_light_budget) {
                NX_CORE_WARN(
                    "RenderSceneFrameBuilder truncated rect lights from {} to {}.",
                    render_data.rect_lights_data.size(),
                    rect_light_budget);
            }
        }

        const size_t probe_count = std::min<size_t>(
            render_data.reflection_probes_data.size(),
            kMaxRenderReflectionProbes);
        frame.reflection_probes.reserve(probe_count);
        for (size_t index = 0; index < probe_count; ++index) {
            const RendererReflectionProbeData& source = render_data.reflection_probes_data[index];
            if (!source.enabled) {
                continue;
            }

            RenderFrameReflectionProbe probe = buildReflectionProbe(source);
            if (probe.intensity > 0.0001f) {
                frame.reflection_probes.push_back(probe);
            }
        }

        if (render_data.reflection_probes_data.size() > kMaxRenderReflectionProbes) {
            NX_CORE_WARN(
                "RenderSceneFrameBuilder truncated reflection probes from {} to {}.",
                render_data.reflection_probes_data.size(),
                kMaxRenderReflectionProbes);
        }

        frame.environment_asset = render_data.environment_data.environment_asset;
        frame.environment_color = sanitizeColor(render_data.environment_data.background_color);
        frame.environment_intensity = sanitizeNonNegative(
            render_data.environment_data.intensity,
            kDefaultEnvironmentIntensity);
        frame.skybox_intensity = sanitizeNonNegative(
            render_data.environment_data.skybox_intensity,
            frame.environment_intensity > 0.0f ? frame.environment_intensity : kDefaultSkyboxIntensity) *
            sanitizeScale(lighting.skybox_intensity_scale);
        frame.ibl_intensity = sanitizeNonNegative(
            render_data.environment_data.ibl_intensity,
            frame.environment_intensity > 0.0f ? frame.environment_intensity : kDefaultIblIntensity) *
            sanitizeScale(lighting.ibl_intensity_scale);
        appendDebugLines(render_data.debug_draw, frame.debug_draw);

        return frame;
    }
} // namespace NexAur
