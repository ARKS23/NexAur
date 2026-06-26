#include "pch.h"
#include "render_scene_frame_builder.h"

#include "Function/Renderer/data/render_data.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace NexAur {
    namespace {
        constexpr size_t kMaxPointLights = 64;
        constexpr float kDefaultEnvironmentIntensity = 1.0f;
        constexpr glm::vec3 kDefaultLightDirection{ -0.2f, -1.0f, -0.3f };

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

        float sanitizeNonNegative(float value, float fallback) {
            return isFinite(value) && value >= 0.0f ? value : fallback;
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

        glm::mat4 sanitizeTransform(const glm::mat4& transform) {
            return isFinite(transform) ? transform : glm::mat4{ 1.0f };
        }

        RenderFrameDirectionalLight buildDirectionalLight(const RendererDirectionalLightData& source) {
            RenderFrameDirectionalLight light;
            light.direction = sanitizeDirection(source.direction);
            light.color = sanitizeColor(source.color);
            light.intensity = sanitizeNonNegative(source.intensity, 1.0f);
            light.cast_shadow = source.cast_shadow;
            light.shadow_strength = sanitizeUnit(source.shadow_strength, 0.65f);
            light.shadow_bias = sanitizeMin(source.shadow_bias, 0.002f, 0.0f);
            light.shadow_distance = sanitizeMin(source.shadow_distance, 30.0f, 1.0f);
            return light;
        }

        RenderFramePointLight buildPointLight(const RendererPointLightData& source) {
            RenderFramePointLight light;
            light.position = isFinite(source.position) ? source.position : glm::vec3{ 0.0f };
            light.color = sanitizeColor(source.color);
            light.intensity = sanitizeNonNegative(source.intensity, 1.0f);
            light.constant = sanitizeNonNegative(source.constant, 1.0f);
            light.linear = sanitizeNonNegative(source.linear, 0.09f);
            light.quadratic = sanitizeNonNegative(source.quadratic, 0.032f);
            return light;
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
    } // namespace

    RenderSceneFrame RenderSceneFrameBuilder::buildRenderSceneFrame(
        const RenderDataPacket& render_data,
        const RenderView& render_view) const {
        RenderSceneFrame frame;
        frame.view = render_view;

        appendObjects(render_data.opaque_objects, frame.opaque_objects, "opaque");
        appendObjects(render_data.transparent_objects, frame.transparent_objects, "transparent");

        frame.directional_light = buildDirectionalLight(render_data.directional_light_data);

        const size_t point_light_count = std::min(render_data.point_lights_data.size(), kMaxPointLights);
        frame.point_lights.reserve(point_light_count);
        for (size_t index = 0; index < point_light_count; ++index) {
            frame.point_lights.push_back(buildPointLight(render_data.point_lights_data[index]));
        }

        if (render_data.point_lights_data.size() > kMaxPointLights) {
            NX_CORE_WARN(
                "RenderSceneFrameBuilder truncated point lights from {} to {}.",
                render_data.point_lights_data.size(),
                kMaxPointLights);
        }

        frame.environment_asset = render_data.environment_data.environment_asset;
        frame.environment_color = sanitizeColor(render_data.environment_data.background_color);
        frame.environment_intensity = sanitizeNonNegative(
            render_data.environment_data.intensity,
            kDefaultEnvironmentIntensity);

        return frame;
    }
} // namespace NexAur
