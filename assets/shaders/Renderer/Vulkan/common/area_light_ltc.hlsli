#ifndef NEXAUR_AREA_LIGHT_LTC_HLSLI
#define NEXAUR_AREA_LIGHT_LTC_HLSLI

struct NxRectLightGeometry {
    float3 position;
    float3 right;
    float3 up;
    float3 normal;
    float width;
    float height;
    float range;
    float3 color;
    float intensity;
    bool two_sided;
};

float3 NxAreaSafeNormalize(float3 value, float3 fallback) {
    const float length_sq = dot(value, value);
    return length_sq > 0.000001f ? value * rsqrt(length_sq) : fallback;
}

NxRectLightGeometry NxDecodeRectLight(RectLightData light) {
    NxRectLightGeometry decoded;
    decoded.position = light.position_intensity.xyz;
    decoded.intensity = max(light.position_intensity.w, 0.0f);
    decoded.right = NxAreaSafeNormalize(light.right_width.xyz, float3(1.0f, 0.0f, 0.0f));
    decoded.up = NxAreaSafeNormalize(light.up_height.xyz, float3(0.0f, 0.0f, 1.0f));
    decoded.normal = NxAreaSafeNormalize(light.normal_range.xyz, float3(0.0f, -1.0f, 0.0f));
    decoded.width = max(light.right_width.w, 0.01f);
    decoded.height = max(light.up_height.w, 0.01f);
    decoded.range = max(light.normal_range.w, 0.1f);
    decoded.color = light.color_flags.rgb;
    decoded.two_sided = light.color_flags.w > 0.5f;
    return decoded;
}

float3 NxRectLightCorner(NxRectLightGeometry light, int corner_index) {
    const float3 half_right = light.right * (light.width * 0.5f);
    const float3 half_up = light.up * (light.height * 0.5f);

    if (corner_index == 0) {
        return light.position - half_right - half_up;
    }
    if (corner_index == 1) {
        return light.position + half_right - half_up;
    }
    if (corner_index == 2) {
        return light.position + half_right + half_up;
    }
    return light.position - half_right + half_up;
}

bool NxEvaluateRectLightRepresentative(
    NxRectLightGeometry light,
    float3 world_position,
    out float3 light_dir,
    out float distance_to_light,
    out float attenuation,
    out float light_facing,
    out float solid_angle) {
    float3 surface_to_light_center = world_position - light.position;
    const float x = clamp(dot(surface_to_light_center, light.right), -light.width * 0.5f, light.width * 0.5f);
    const float y = clamp(dot(surface_to_light_center, light.up), -light.height * 0.5f, light.height * 0.5f);
    const float3 representative_point = light.position + light.right * x + light.up * y;

    const float3 light_vector = representative_point - world_position;
    const float distance2 = max(dot(light_vector, light_vector), 0.0001f);
    distance_to_light = sqrt(distance2);
    light_dir = light_vector / max(distance_to_light, 0.0001f);

    light_facing = light.two_sided
        ? abs(dot(light.normal, -light_dir))
        : saturate(dot(light.normal, -light_dir));

    attenuation = saturate(1.0f - distance_to_light / light.range);
    attenuation *= attenuation;

    const float area = light.width * light.height;
    solid_angle = min(area * light_facing / distance2, 2.0f * NX_PI);

    return light.intensity > 0.0001f && light_facing > 0.0001f && attenuation > 0.0001f;
}

float3 NxEvaluateRectLightFallbackDirect(
    NxMaterialSample material,
    float3 world_position,
    float3 view_dir,
    RectLightData light_data) {
    NxRectLightGeometry light = NxDecodeRectLight(light_data);

    float3 light_dir = 0.0f;
    float distance_to_light = 0.0f;
    float attenuation = 0.0f;
    float light_facing = 0.0f;
    float solid_angle = 0.0f;
    if (!NxEvaluateRectLightRepresentative(
            light,
            world_position,
            light_dir,
            distance_to_light,
            attenuation,
            light_facing,
            solid_angle)) {
        return 0.0f;
    }

    const float area = light.width * light.height;
    const float apparent_radius = 0.5f * sqrt(area / NX_PI);
    const float roughness_widen = apparent_radius / max(distance_to_light, 0.001f);
    const float rect_roughness = saturate(sqrt(
        material.roughness * material.roughness +
        roughness_widen * roughness_widen));
    const float3 radiance = light.color * light.intensity * solid_angle * attenuation;

    return NxEvaluateDirectLight(
        material.base_color.rgb,
        material.metallic,
        rect_roughness,
        material.normal,
        view_dir,
        light_dir,
        radiance);
}

float3 NxEvaluateRectLightDiffuseApprox(
    NxMaterialSample material,
    float3 world_position,
    float3 view_dir,
    RectLightData light_data) {
    NxRectLightGeometry light = NxDecodeRectLight(light_data);

    float3 light_dir = 0.0f;
    float distance_to_light = 0.0f;
    float attenuation = 0.0f;
    float light_facing = 0.0f;
    float solid_angle = 0.0f;
    if (!NxEvaluateRectLightRepresentative(
            light,
            world_position,
            light_dir,
            distance_to_light,
            attenuation,
            light_facing,
            solid_angle)) {
        return 0.0f;
    }

    const float ndotl = saturate(dot(material.normal, light_dir));
    if (ndotl <= 0.0001f) {
        return 0.0f;
    }

    const float3 half_vector = NxAreaSafeNormalize(view_dir + light_dir, material.normal);
    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), material.base_color.rgb, material.metallic);
    const float3 fresnel = NxFresnelSchlick(saturate(dot(half_vector, view_dir)), f0);
    const float3 diffuse = (1.0f - fresnel) * (1.0f - material.metallic) * material.base_color.rgb / NX_PI;
    const float3 radiance = light.color * light.intensity * solid_angle * attenuation;
    return diffuse * radiance * ndotl;
}

void NxBuildLtcFrame(
    float3 normal,
    float3 view_dir,
    out float3 tangent,
    out float3 bitangent,
    out float3 reflection_axis) {
    reflection_axis = NxAreaSafeNormalize(reflect(-view_dir, normal), normal);
    tangent = NxAreaSafeNormalize(cross(normal, reflection_axis), float3(1.0f, 0.0f, 0.0f));
    if (dot(tangent, tangent) <= 0.000001f || abs(dot(tangent, reflection_axis)) > 0.99f) {
        const float3 fallback_up = abs(reflection_axis.y) < 0.99f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        tangent = NxAreaSafeNormalize(cross(fallback_up, reflection_axis), float3(1.0f, 0.0f, 0.0f));
    }
    bitangent = NxAreaSafeNormalize(cross(reflection_axis, tangent), float3(0.0f, 1.0f, 0.0f));
}

float3 NxWorldToLtcLocal(float3 value, float3 tangent, float3 bitangent, float3 axis) {
    return float3(dot(value, tangent), dot(value, bitangent), dot(value, axis));
}

float NxLtcEdgeIntegral(float3 v1, float3 v2) {
    const float cos_theta = clamp(dot(v1, v2), -0.9999f, 0.9999f);
    const float theta = acos(cos_theta);
    const float sin_theta = max(sin(theta), 0.0001f);
    return cross(v1, v2).z * theta / sin_theta;
}

float NxLtcPolygonFormFactor(float3 p0, float3 p1, float3 p2, float3 p3) {
    if (p0.z <= 0.0f && p1.z <= 0.0f && p2.z <= 0.0f && p3.z <= 0.0f) {
        return 0.0f;
    }

    p0.z = max(p0.z, 0.0001f);
    p1.z = max(p1.z, 0.0001f);
    p2.z = max(p2.z, 0.0001f);
    p3.z = max(p3.z, 0.0001f);

    p0 = normalize(p0);
    p1 = normalize(p1);
    p2 = normalize(p2);
    p3 = normalize(p3);

    const float integral =
        NxLtcEdgeIntegral(p0, p1) +
        NxLtcEdgeIntegral(p1, p2) +
        NxLtcEdgeIntegral(p2, p3) +
        NxLtcEdgeIntegral(p3, p0);
    return saturate(abs(integral) / (2.0f * NX_PI));
}

float3 NxEvaluateRectLightSpecularLtc(
    NxMaterialSample material,
    float3 world_position,
    float3 view_dir,
    RectLightData light_data,
    float specular_scale) {
    NxRectLightGeometry light = NxDecodeRectLight(light_data);
    if (light.intensity <= 0.0001f) {
        return 0.0f;
    }

    const float NoV = saturate(dot(material.normal, view_dir));
    if (NoV <= 0.0001f) {
        return 0.0f;
    }

    const float3 center_dir = NxAreaSafeNormalize(light.position - world_position, material.normal);
    const float light_facing = light.two_sided
        ? abs(dot(light.normal, -center_dir))
        : saturate(dot(light.normal, -center_dir));
    if (light_facing <= 0.0001f) {
        return 0.0f;
    }

    const float center_distance = length(light.position - world_position);
    float attenuation = saturate(1.0f - center_distance / light.range);
    attenuation *= attenuation;
    if (attenuation <= 0.0001f) {
        return 0.0f;
    }

    float3 tangent = 0.0f;
    float3 bitangent = 0.0f;
    float3 reflection_axis = 0.0f;
    NxBuildLtcFrame(material.normal, view_dir, tangent, bitangent, reflection_axis);

    const float roughness = clamp(material.roughness, 0.045f, 1.0f);
    const float alpha = roughness * roughness;
    const float grazing = 1.0f - NoV;
    const float lobe_scale = max(0.035f, lerp(0.07f, 1.0f, alpha) * (1.0f + grazing * (1.0f - alpha)));

    float3 p0 = NxWorldToLtcLocal(NxRectLightCorner(light, 0) - world_position, tangent, bitangent, reflection_axis);
    float3 p1 = NxWorldToLtcLocal(NxRectLightCorner(light, 1) - world_position, tangent, bitangent, reflection_axis);
    float3 p2 = NxWorldToLtcLocal(NxRectLightCorner(light, 2) - world_position, tangent, bitangent, reflection_axis);
    float3 p3 = NxWorldToLtcLocal(NxRectLightCorner(light, 3) - world_position, tangent, bitangent, reflection_axis);

    p0.xy /= lobe_scale;
    p1.xy /= lobe_scale;
    p2.xy /= lobe_scale;
    p3.xy /= lobe_scale;

    const float form_factor = NxLtcPolygonFormFactor(p0, p1, p2, p3);
    if (form_factor <= 0.0001f) {
        return 0.0f;
    }

    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), material.base_color.rgb, material.metallic);
    const float3 fresnel = NxFresnelSchlick(NoV, f0);
    const float visibility = saturate(NoV / max(NoV * (1.0f - alpha) + alpha, 0.0001f));
    const float amplitude = lerp(1.0f, 0.65f, alpha) * visibility;
    const float3 radiance = light.color * light.intensity * attenuation * light_facing;
    return radiance * fresnel * form_factor * amplitude * max(specular_scale, 0.0f);
}

#endif
