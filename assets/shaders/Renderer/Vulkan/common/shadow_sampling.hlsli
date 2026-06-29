static const uint NX_SHADOW_FILTER_HARD = 0u;
static const uint NX_SHADOW_FILTER_PCF_3X3 = 1u;
static const uint NX_SHADOW_FILTER_PCF_5X5 = 2u;

float3 NxSafeNormalizeShadowVector(float3 value, float3 fallback) {
    const float length_sq = dot(value, value);
    return length_sq > 0.000001f ? value * rsqrt(length_sq) : fallback;
}

uint NxGetShadowCascadeCount() {
    return clamp((uint)round(max(g_frame.shadow_cascade_params.y, 1.0f)), 1u, 4u);
}

uint NxSelectShadowCascade(float view_depth) {
    const uint cascade_count = NxGetShadowCascadeCount();
    uint selected = cascade_count - 1u;

    [unroll]
    for (uint index = 0u; index < 4u; ++index) {
        if (index < cascade_count && view_depth <= g_frame.shadow_cascade_splits[index]) {
            selected = index;
            break;
        }
    }

    return selected;
}

float NxSampleShadowOcclusion(float2 uv, uint cascade_index, float current_depth, float bias) {
    const float closest_depth = g_shadow_map.SampleLevel(g_shadow_sampler, float3(uv, (float)cascade_index), 0.0f);
    return current_depth - bias > closest_depth ? 1.0f : 0.0f;
}

float NxEvaluatePcfShadow(float2 uv, uint cascade_index, float current_depth, float bias, int radius, float filter_radius) {
    const float texel_size = 1.0f / max(g_frame.shadow_params.w, 1.0f);
    const float sample_stride = texel_size * max(filter_radius, 0.0f);

    float occlusion = 0.0f;
    float sample_count = 0.0f;

    [loop]
    for (int y = -radius; y <= radius; ++y) {
        [loop]
        for (int x = -radius; x <= radius; ++x) {
            const float2 sample_uv = uv + float2((float)x, (float)y) * sample_stride;
            occlusion += NxSampleShadowOcclusion(sample_uv, cascade_index, current_depth, bias);
            sample_count += 1.0f;
        }
    }

    return occlusion / max(sample_count, 1.0f);
}

float NxComputeReceiverBias(float3 normal, float3 light_dir) {
    const float constant_bias = max(g_frame.shadow_params.z, 0.0f);
    const float slope_bias = max(g_frame.shadow_quality_params.w, 0.0f);
    const float3 safe_normal = NxSafeNormalizeShadowVector(normal, float3(0.0f, 1.0f, 0.0f));
    const float3 safe_light_dir = NxSafeNormalizeShadowVector(light_dir, float3(0.0f, 1.0f, 0.0f));
    const float normal_dot_light = saturate(dot(safe_normal, safe_light_dir));
    return constant_bias + slope_bias * (1.0f - normal_dot_light);
}

float NxEvaluateShadowVisibility(float3 world_position, float3 world_normal, float3 light_dir, float view_depth) {
    if (g_frame.shadow_params.x < 0.5f) {
        return 1.0f;
    }

    const uint cascade_index = NxSelectShadowCascade(view_depth);
    const float normal_bias = max(g_frame.shadow_quality_params.z, 0.0f);
    const float3 safe_normal = NxSafeNormalizeShadowVector(world_normal, float3(0.0f, 1.0f, 0.0f));
    const float3 biased_world_position = world_position + safe_normal * normal_bias;
    const float4 shadow_position =
        mul(g_frame.shadow_light_view_projection[cascade_index], float4(biased_world_position, 1.0f));
    if (shadow_position.w <= 0.0f) {
        return 1.0f;
    }

    const float3 projected = shadow_position.xyz / shadow_position.w;
    const float2 uv = projected.xy * 0.5f + 0.5f;
    const float current_depth = projected.z;

    if (uv.x < 0.0f || uv.x > 1.0f ||
        uv.y < 0.0f || uv.y > 1.0f ||
        current_depth < 0.0f || current_depth > 1.0f) {
        return 1.0f;
    }

    const float bias = NxComputeReceiverBias(world_normal, light_dir);
    const float strength = saturate(g_frame.shadow_params.y);
    const uint filter_mode = (uint)max(g_frame.shadow_quality_params.x, 0.0f);

    float occlusion = 0.0f;
    if (filter_mode == NX_SHADOW_FILTER_PCF_5X5) {
        occlusion = NxEvaluatePcfShadow(uv, cascade_index, current_depth, bias, 2, g_frame.shadow_quality_params.y);
    } else if (filter_mode == NX_SHADOW_FILTER_PCF_3X3) {
        occlusion = NxEvaluatePcfShadow(uv, cascade_index, current_depth, bias, 1, g_frame.shadow_quality_params.y);
    } else {
        occlusion = NxSampleShadowOcclusion(uv, cascade_index, current_depth, bias);
    }

    return lerp(1.0f, 1.0f - strength, occlusion);
}

float3 NxApplyCascadeDebugOverlay(float3 lit_color, float view_depth) {
    if (g_frame.shadow_cascade_params.z < 0.5f) {
        return lit_color;
    }

    const uint cascade_index = NxSelectShadowCascade(view_depth);
    const float3 tint = g_frame.shadow_cascade_colors[cascade_index].rgb;
    return lerp(lit_color, lit_color * tint, 0.35f);
}
