static const uint NX_SHADOW_FILTER_HARD = 0u;
static const uint NX_SHADOW_FILTER_PCF_3X3 = 1u;
static const uint NX_SHADOW_FILTER_PCF_5X5 = 2u;
static const uint NX_SHADOW_FILTER_POISSON_PCF = 3u;
static const uint NX_SHADOW_FILTER_PCSS = 4u;
static const uint NX_SHADOW_POISSON_SAMPLE_COUNT = 16u;

static const float2 NX_SHADOW_POISSON_DISK[16] = {
    float2(-0.94201624f, -0.39906216f),
    float2( 0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f),
    float2( 0.34495938f,  0.29387760f),
    float2(-0.91588581f,  0.45771432f),
    float2(-0.81544232f, -0.87912464f),
    float2(-0.38277543f,  0.27676845f),
    float2( 0.97484398f,  0.75648379f),
    float2( 0.44323325f, -0.97511554f),
    float2( 0.53742981f, -0.47373420f),
    float2(-0.26496911f, -0.41893023f),
    float2( 0.79197514f,  0.19090188f),
    float2(-0.24188840f,  0.99706507f),
    float2(-0.81409955f,  0.91437590f),
    float2( 0.19984126f,  0.78641367f),
    float2( 0.14383161f, -0.14100790f)
};

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

float NxShadowTexelSize() {
    return 1.0f / max(g_frame.shadow_params.w, 1.0f);
}

float NxSampleShadowDepth(float2 uv, uint cascade_index) {
    return g_shadow_map.SampleLevel(g_shadow_sampler, float3(uv, (float)cascade_index), 0.0f);
}

float NxEvaluatePcfShadow(float2 uv, uint cascade_index, float current_depth, float bias, int radius, float filter_radius) {
    const float sample_stride = NxShadowTexelSize() * max(filter_radius, 0.0f);

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

float NxEvaluatePoissonPcfShadow(
    float2 uv,
    uint cascade_index,
    float current_depth,
    float bias,
    float filter_radius) {
    const float sample_stride = NxShadowTexelSize() * max(filter_radius, 0.0f);

    float occlusion = 0.0f;
    [unroll]
    for (uint index = 0u; index < NX_SHADOW_POISSON_SAMPLE_COUNT; ++index) {
        const float2 sample_uv = uv + NX_SHADOW_POISSON_DISK[index] * sample_stride;
        occlusion += NxSampleShadowOcclusion(sample_uv, cascade_index, current_depth, bias);
    }

    return occlusion / (float)NX_SHADOW_POISSON_SAMPLE_COUNT;
}

float NxFindAverageBlockerDepth(
    float2 uv,
    uint cascade_index,
    float current_depth,
    float bias,
    float search_radius,
    out float blocker_count) {
    const float sample_stride = NxShadowTexelSize() * max(search_radius, 0.0f);
    const float receiver_depth = current_depth - bias;

    blocker_count = 0.0f;
    float blocker_depth_sum = 0.0f;

    [unroll]
    for (uint index = 0u; index < NX_SHADOW_POISSON_SAMPLE_COUNT; ++index) {
        const float2 sample_uv = uv + NX_SHADOW_POISSON_DISK[index] * sample_stride;
        const float blocker_depth = NxSampleShadowDepth(sample_uv, cascade_index);
        if (receiver_depth > blocker_depth) {
            blocker_depth_sum += blocker_depth;
            blocker_count += 1.0f;
        }
    }

    return blocker_count > 0.0f ? blocker_depth_sum / blocker_count : 1.0f;
}

float NxEstimatePcssFilterRadius(float receiver_depth, float blocker_depth) {
    const float light_radius = max(g_frame.shadow_pcss_params.x, 0.0f);
    const float search_radius = max(g_frame.shadow_pcss_params.y, 0.0f);
    const float min_radius = max(g_frame.shadow_pcss_params.z, 0.0f);
    const float max_radius = max(g_frame.shadow_pcss_params.w, min_radius);
    const float safe_blocker_depth = max(blocker_depth, 0.001f);
    const float penumbra = max((receiver_depth - blocker_depth) / safe_blocker_depth, 0.0f);
    return clamp(penumbra * light_radius * search_radius, min_radius, max_radius);
}

float NxEvaluatePcssShadow(float2 uv, uint cascade_index, float current_depth, float bias) {
    float blocker_count = 0.0f;
    const float average_blocker_depth = NxFindAverageBlockerDepth(
        uv,
        cascade_index,
        current_depth,
        bias,
        g_frame.shadow_pcss_params.y,
        blocker_count);

    if (blocker_count <= 0.0f) {
        return 0.0f;
    }

    const float filter_radius = NxEstimatePcssFilterRadius(current_depth - bias, average_blocker_depth);
    return NxEvaluatePoissonPcfShadow(uv, cascade_index, current_depth, bias, filter_radius);
}

float NxComputeReceiverBias(float3 normal, float3 light_dir) {
    const float constant_bias = max(g_frame.shadow_params.z, 0.0f);
    const float slope_bias = max(g_frame.shadow_quality_params.w, 0.0f);
    const float3 safe_normal = NxSafeNormalizeShadowVector(normal, float3(0.0f, 1.0f, 0.0f));
    const float3 safe_light_dir = NxSafeNormalizeShadowVector(light_dir, float3(0.0f, 1.0f, 0.0f));
    const float normal_dot_light = saturate(dot(safe_normal, safe_light_dir));
    return constant_bias + slope_bias * (1.0f - normal_dot_light);
}

uint NxSelectPointShadowFace(float3 light_to_fragment) {
    const float3 abs_dir = abs(light_to_fragment);
    if (abs_dir.x >= abs_dir.y && abs_dir.x >= abs_dir.z) {
        return light_to_fragment.x >= 0.0f ? 0u : 1u;
    }
    if (abs_dir.y >= abs_dir.x && abs_dir.y >= abs_dir.z) {
        return light_to_fragment.y >= 0.0f ? 2u : 3u;
    }
    return light_to_fragment.z >= 0.0f ? 4u : 5u;
}

float NxComputeLocalShadowBias(float constant_bias, float3 normal, float3 light_dir) {
    const float3 safe_normal = NxSafeNormalizeShadowVector(normal, float3(0.0f, 1.0f, 0.0f));
    const float3 safe_light_dir = NxSafeNormalizeShadowVector(light_dir, float3(0.0f, 1.0f, 0.0f));
    const float normal_dot_light = saturate(dot(safe_normal, safe_light_dir));
    return max(constant_bias, 0.0f) * (1.0f + 0.5f * (1.0f - normal_dot_light));
}

float NxApplyLocalShadowVisibility(float strength, float occlusion) {
    return lerp(1.0f, 1.0f - saturate(strength), saturate(occlusion));
}

bool NxProjectPointShadowSample(
    float3 sample_world_position,
    float3 light_position,
    float range,
    uint shadow_slot,
    out uint layer_index,
    out float2 uv,
    out float current_depth) {
    const float3 light_to_sample = sample_world_position - light_position;
    if (dot(light_to_sample, light_to_sample) > range * range) {
        layer_index = 0u;
        uv = 0.0f;
        current_depth = 1.0f;
        return false;
    }

    const uint face_index = NxSelectPointShadowFace(light_to_sample);
    layer_index = shadow_slot * 6u + face_index;
    const float4 shadow_position =
        mul(g_frame.point_shadow_view_projection[layer_index], float4(sample_world_position, 1.0f));
    if (shadow_position.w <= 0.0f) {
        uv = 0.0f;
        current_depth = 1.0f;
        return false;
    }

    const float3 projected = shadow_position.xyz / shadow_position.w;
    uv = projected.xy * 0.5f + 0.5f;
    current_depth = projected.z;
    if (uv.x < 0.0f || uv.x > 1.0f ||
        uv.y < 0.0f || uv.y > 1.0f ||
        current_depth < 0.0f || current_depth > 1.0f) {
        return false;
    }

    return true;
}

float NxSamplePointShadowDepth(float2 uv, uint layer_index) {
    return g_point_shadow_map.SampleLevel(
        g_point_shadow_sampler,
        float3(uv, (float)layer_index),
        0.0f);
}

float NxSamplePointShadowOcclusionAtProjected(float2 uv, uint layer_index, float current_depth, float bias) {
    const float closest_depth = NxSamplePointShadowDepth(uv, layer_index);
    return current_depth - bias > closest_depth ? 1.0f : 0.0f;
}

float NxSamplePointShadowOcclusion(
    float3 sample_world_position,
    float3 light_position,
    float range,
    uint shadow_slot,
    float bias) {
    uint layer_index = 0u;
    float2 uv = 0.0f;
    float current_depth = 1.0f;
    if (!NxProjectPointShadowSample(
            sample_world_position,
            light_position,
            range,
            shadow_slot,
            layer_index,
            uv,
            current_depth)) {
        return 0.0f;
    }

    return NxSamplePointShadowOcclusionAtProjected(uv, layer_index, current_depth, bias);
}

float NxEvaluatePointPcfShadow(
    float3 biased_world_position,
    float3 light_position,
    float range,
    uint shadow_slot,
    float bias) {
    const float filter_radius = max(g_frame.point_shadow_quality_params.x, 0.0f);
    if (filter_radius <= 0.0f) {
        return NxSamplePointShadowOcclusion(
            biased_world_position,
            light_position,
            range,
            shadow_slot,
            bias);
    }

    const float3 light_to_fragment = biased_world_position - light_position;
    const float distance_to_light = max(length(light_to_fragment), 0.001f);
    const float3 ray_dir = light_to_fragment / distance_to_light;
    const float3 reference_axis = abs(ray_dir.y) < 0.95f ?
        float3(0.0f, 1.0f, 0.0f) :
        float3(1.0f, 0.0f, 0.0f);
    const float3 tangent = NxSafeNormalizeShadowVector(cross(reference_axis, ray_dir), float3(1.0f, 0.0f, 0.0f));
    const float3 bitangent = NxSafeNormalizeShadowVector(cross(ray_dir, tangent), float3(0.0f, 1.0f, 0.0f));
    const float world_texel = (2.0f * distance_to_light) / max(g_frame.point_shadow_params.y, 1.0f);
    const float sample_stride = world_texel * filter_radius;

    float occlusion = 0.0f;
    float sample_count = 0.0f;

    [loop]
    for (int y = -1; y <= 1; ++y) {
        [loop]
        for (int x = -1; x <= 1; ++x) {
            const float3 sample_position =
                biased_world_position +
                (tangent * (float)x + bitangent * (float)y) * sample_stride;
            occlusion += NxSamplePointShadowOcclusion(
                sample_position,
                light_position,
                range,
                shadow_slot,
                bias);
            sample_count += 1.0f;
        }
    }

    return occlusion / max(sample_count, 1.0f);
}

float NxEvaluatePointShadowVisibility(
    float3 world_position,
    float3 world_normal,
    float3 light_position,
    float4 shadow_data) {
    if (g_frame.point_shadow_params.x < 0.5f || shadow_data.w < 0.5f || shadow_data.x < 0.0f) {
        return 1.0f;
    }

    const uint shadow_slot = (uint)round(shadow_data.x);
    if (shadow_slot >= 4u || shadow_slot >= (uint)round(g_frame.point_shadow_quality_params.y)) {
        return 1.0f;
    }

    const float3 safe_normal = NxSafeNormalizeShadowVector(world_normal, float3(0.0f, 1.0f, 0.0f));
    const float3 biased_world_position =
        world_position + safe_normal * max(g_frame.point_shadow_params.w, 0.0f);
    const float range = max(shadow_data.y, 0.1f);
    uint center_layer_index = 0u;
    float2 center_uv = 0.0f;
    float center_depth = 1.0f;
    if (!NxProjectPointShadowSample(
            biased_world_position,
            light_position,
            range,
            shadow_slot,
            center_layer_index,
            center_uv,
            center_depth)) {
        return 1.0f;
    }

    const float3 fragment_to_light = NxSafeNormalizeShadowVector(
        light_position - world_position,
        float3(0.0f, 1.0f, 0.0f));
    const float bias = NxComputeLocalShadowBias(g_frame.point_shadow_params.z, safe_normal, fragment_to_light);
    const float closest_depth = NxSamplePointShadowDepth(center_uv, center_layer_index);
    const float occlusion = NxEvaluatePointPcfShadow(
        biased_world_position,
        light_position,
        range,
        shadow_slot,
        bias);
    float strength = saturate(shadow_data.z);
    if (g_frame.contact_shadow_params.x > 0.5f && center_depth - bias > closest_depth) {
        const float depth_gap = max(center_depth - bias - closest_depth, 0.0f);
        const float contact_window = max(
            g_frame.contact_shadow_params.w,
            g_frame.contact_shadow_params.z * 0.25f) / range;
        const float contact_weight = 1.0f - saturate(depth_gap / max(contact_window, 0.0001f));
        strength = saturate(strength + contact_weight * saturate(g_frame.contact_shadow_params.y));
    }
    return NxApplyLocalShadowVisibility(strength, occlusion);
}

float NxRectShadowTexelSize() {
    return 1.0f / max(g_frame.rect_shadow_params.y, 1.0f);
}

float NxSampleRectShadowDepth(float2 uv, uint layer_index) {
    return g_rect_shadow_map.SampleLevel(
        g_rect_shadow_sampler,
        float3(uv, (float)layer_index),
        0.0f);
}

float NxSampleRectShadowOcclusion(float2 uv, uint layer_index, float current_depth, float bias) {
    const float closest_depth = NxSampleRectShadowDepth(uv, layer_index);
    return current_depth - bias > closest_depth ? 1.0f : 0.0f;
}

uint NxRectShadowTapCount(float requested_taps) {
    const uint safe_taps = (uint)round(max(requested_taps, 1.0f));
    return min(max(safe_taps, 1u), NX_SHADOW_POISSON_SAMPLE_COUNT);
}

float NxEvaluateRectPcfShadow(
    float2 uv,
    uint layer_index,
    float current_depth,
    float bias,
    float filter_radius) {
    const float sample_stride = NxRectShadowTexelSize() * max(filter_radius, 0.0f);

    float occlusion = 0.0f;
    float sample_count = 0.0f;

    [loop]
    for (int y = -1; y <= 1; ++y) {
        [loop]
        for (int x = -1; x <= 1; ++x) {
            const float2 sample_uv = uv + float2((float)x, (float)y) * sample_stride;
            occlusion += NxSampleRectShadowOcclusion(sample_uv, layer_index, current_depth, bias);
            sample_count += 1.0f;
        }
    }

    return occlusion / max(sample_count, 1.0f);
}

float NxEvaluateRectPoissonPcfShadow(
    float2 uv,
    uint layer_index,
    float current_depth,
    float bias,
    float filter_radius,
    uint tap_count) {
    const float sample_stride = NxRectShadowTexelSize() * max(filter_radius, 0.0f);

    float occlusion = NxSampleRectShadowOcclusion(uv, layer_index, current_depth, bias);
    float sample_count = 1.0f;
    const uint poisson_taps = tap_count > 1u ? tap_count - 1u : 0u;
    [loop]
    for (uint index = 0u; index < poisson_taps; ++index) {
        const float2 sample_uv = uv + NX_SHADOW_POISSON_DISK[index] * sample_stride;
        occlusion += NxSampleRectShadowOcclusion(sample_uv, layer_index, current_depth, bias);
        sample_count += 1.0f;
    }

    return occlusion / max(sample_count, 1.0f);
}

float NxFindRectShadowBlocker(
    float2 uv,
    uint layer_index,
    float current_depth,
    float bias,
    float search_radius,
    uint tap_count,
    out float blocker_count) {
    const float sample_stride = NxRectShadowTexelSize() * max(search_radius, 0.0f);
    const float receiver_depth = current_depth - bias;

    blocker_count = 0.0f;
    float blocker_depth_sum = 0.0f;
    const float center_blocker_depth = NxSampleRectShadowDepth(uv, layer_index);
    if (receiver_depth > center_blocker_depth) {
        blocker_depth_sum += center_blocker_depth;
        blocker_count += 1.0f;
    }

    const uint poisson_taps = tap_count > 1u ? tap_count - 1u : 0u;
    [loop]
    for (uint index = 0u; index < poisson_taps; ++index) {
        const float2 sample_uv = uv + NX_SHADOW_POISSON_DISK[index] * sample_stride;
        const float blocker_depth = NxSampleRectShadowDepth(sample_uv, layer_index);
        if (receiver_depth > blocker_depth) {
            blocker_depth_sum += blocker_depth;
            blocker_count += 1.0f;
        }
    }

    return blocker_count > 0.0f ? blocker_depth_sum / blocker_count : 1.0f;
}

float NxEstimateRectPenumbraRadius(float receiver_depth, float blocker_depth, RectLightData light) {
    const float safe_blocker_depth = max(blocker_depth, 0.001f);
    const float light_extent = max(max(light.right_width.w, light.up_height.w), 0.01f);
    const float light_radius = max(g_frame.rect_shadow_pcss_params.y, 0.0f) * light_extent;
    const float search_radius = max(g_frame.rect_shadow_pcss_params.z, 0.0f);
    const float min_radius = max(g_frame.rect_shadow_pcss_params.w, 0.0f);
    const float max_radius = max(g_frame.rect_shadow_pcss_quality_params.x, min_radius);
    const float penumbra = max(receiver_depth - blocker_depth, 0.0f) / safe_blocker_depth;
    return clamp(penumbra * light_radius * search_radius, min_radius, max_radius);
}

float NxSampleRectShadowPcss(
    float2 uv,
    uint layer_index,
    float current_depth,
    float bias,
    RectLightData light) {
    const uint blocker_taps = NxRectShadowTapCount(g_frame.rect_shadow_pcss_quality_params.y);
    const uint filter_taps = NxRectShadowTapCount(g_frame.rect_shadow_pcss_quality_params.z);

    float blocker_count = 0.0f;
    const float average_blocker_depth = NxFindRectShadowBlocker(
        uv,
        layer_index,
        current_depth,
        bias,
        g_frame.rect_shadow_pcss_params.z,
        blocker_taps,
        blocker_count);

    if (blocker_count <= 0.0f) {
        return 0.0f;
    }

    const float filter_radius = NxEstimateRectPenumbraRadius(
        current_depth - bias,
        average_blocker_depth,
        light);
    return NxEvaluateRectPoissonPcfShadow(
        uv,
        layer_index,
        current_depth,
        bias,
        filter_radius,
        filter_taps);
}

float NxEvaluateRectShadowVisibility(
    float3 world_position,
    float3 world_normal,
    RectLightData light) {
    if (g_frame.rect_shadow_params.x < 0.5f || light.shadow.z < 0.5f || light.shadow.x < 0.0f) {
        return 1.0f;
    }

    const uint shadow_slot = (uint)round(light.shadow.x);
    if (shadow_slot >= 4u || shadow_slot >= (uint)round(g_frame.rect_shadow_quality_params.y)) {
        return 1.0f;
    }

    const float3 safe_normal = NxSafeNormalizeShadowVector(world_normal, float3(0.0f, 1.0f, 0.0f));
    const float3 biased_world_position =
        world_position + safe_normal * max(g_frame.rect_shadow_params.w, 0.0f);
    const float4 shadow_position =
        mul(g_frame.rect_shadow_view_projection[shadow_slot], float4(biased_world_position, 1.0f));
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

    const float3 fragment_to_light = NxSafeNormalizeShadowVector(
        -light.normal_range.xyz,
        float3(0.0f, 1.0f, 0.0f));
    const float bias = NxComputeLocalShadowBias(g_frame.rect_shadow_params.z, safe_normal, fragment_to_light);
    float occlusion = 0.0f;
    if (g_frame.rect_shadow_pcss_params.x > 0.5f) {
        occlusion = NxSampleRectShadowPcss(uv, shadow_slot, current_depth, bias, light);
    } else if (g_frame.rect_shadow_quality_params.x > 0.0f) {
        occlusion = NxEvaluateRectPcfShadow(
            uv,
            shadow_slot,
            current_depth,
            bias,
            g_frame.rect_shadow_quality_params.x);
    } else {
        occlusion = NxSampleRectShadowOcclusion(uv, shadow_slot, current_depth, bias);
    }
    const float strength = saturate(light.shadow.y);
    return NxApplyLocalShadowVisibility(strength, occlusion);
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
    if (filter_mode == NX_SHADOW_FILTER_PCSS) {
        occlusion = NxEvaluatePcssShadow(uv, cascade_index, current_depth, bias);
    } else if (filter_mode == NX_SHADOW_FILTER_POISSON_PCF) {
        occlusion = NxEvaluatePoissonPcfShadow(uv, cascade_index, current_depth, bias, g_frame.shadow_quality_params.y);
    } else if (filter_mode == NX_SHADOW_FILTER_PCF_5X5) {
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
