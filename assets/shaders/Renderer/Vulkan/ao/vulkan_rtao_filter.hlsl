#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float> g_source_ao;

[[vk::binding(1, 0)]]
SamplerState g_source_sampler;

[[vk::binding(2, 0)]]
Texture2D<float> g_scene_depth;

struct RtaoFilterPushConstants {
    float4x4 inverse_view_projection;
    float4 camera_position_depth_threshold; // xyz: camera, w: world depth threshold
    float4 texture_params; // xy: AO texel size, zw: depth texel size
    float4 filter_params; // x: normal threshold, y: enabled, zw: unused
};

[[vk::push_constant]]
RtaoFilterPushConstants g_filter;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float3 reconstructWorldPosition(float2 uv, float depth) {
    const float2 ndc = uv * 2.0f - 1.0f;
    float4 world_position = mul(
        g_filter.inverse_view_projection,
        float4(ndc, depth, 1.0f));
    world_position.xyz /= max(abs(world_position.w), 0.00001f);
    return world_position.xyz;
}

float3 chooseDerivative(
    float3 negative_position,
    bool negative_valid,
    float3 center_position,
    float3 positive_position,
    bool positive_valid) {
    const float3 negative_derivative = center_position - negative_position;
    const float3 positive_derivative = positive_position - center_position;
    if (negative_valid && positive_valid) {
        return dot(negative_derivative, negative_derivative) <
                dot(positive_derivative, positive_derivative) ?
            negative_derivative : positive_derivative;
    }
    if (negative_valid) {
        return negative_derivative;
    }
    if (positive_valid) {
        return positive_derivative;
    }
    return 0.0f;
}

float3 reconstructWorldNormal(float2 uv, float3 center_position) {
    const float2 texel = g_filter.texture_params.zw;
    const float2 left_uv = saturate(uv - float2(texel.x, 0.0f));
    const float2 right_uv = saturate(uv + float2(texel.x, 0.0f));
    const float2 up_uv = saturate(uv - float2(0.0f, texel.y));
    const float2 down_uv = saturate(uv + float2(0.0f, texel.y));

    const float left_depth = g_scene_depth.SampleLevel(g_source_sampler, left_uv, 0.0f);
    const float right_depth = g_scene_depth.SampleLevel(g_source_sampler, right_uv, 0.0f);
    const float up_depth = g_scene_depth.SampleLevel(g_source_sampler, up_uv, 0.0f);
    const float down_depth = g_scene_depth.SampleLevel(g_source_sampler, down_uv, 0.0f);

    const float3 horizontal = chooseDerivative(
        reconstructWorldPosition(left_uv, left_depth),
        left_depth < 0.99999f,
        center_position,
        reconstructWorldPosition(right_uv, right_depth),
        right_depth < 0.99999f);
    const float3 vertical = chooseDerivative(
        reconstructWorldPosition(up_uv, up_depth),
        up_depth < 0.99999f,
        center_position,
        reconstructWorldPosition(down_uv, down_depth),
        down_depth < 0.99999f);

    const float3 view_direction = normalize(
        g_filter.camera_position_depth_threshold.xyz - center_position);
    float3 normal = cross(horizontal, vertical);
    if (dot(normal, normal) <= 0.0000001f) {
        return view_direction;
    }
    normal = normalize(normal);
    return dot(normal, view_direction) < 0.0f ? -normal : normal;
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float center_ao = g_source_ao.SampleLevel(
        g_source_sampler,
        input.uv,
        0.0f);
    if (g_filter.filter_params.y < 0.5f) {
        return float4(center_ao, center_ao, center_ao, 1.0f);
    }

    const float center_depth = g_scene_depth.SampleLevel(
        g_source_sampler,
        input.uv,
        0.0f);
    if (center_depth >= 0.99999f) {
        return 1.0f;
    }

    const float3 camera_position = g_filter.camera_position_depth_threshold.xyz;
    const float3 center_position = reconstructWorldPosition(input.uv, center_depth);
    const float3 center_normal = reconstructWorldNormal(input.uv, center_position);
    const float center_distance = length(camera_position - center_position);
    const float depth_threshold = max(g_filter.camera_position_depth_threshold.w, 0.001f);
    const float normal_threshold = saturate(g_filter.filter_params.x);
    const float normal_range = max(1.0f - normal_threshold, 0.001f);

    float weighted_ao = 0.0f;
    float weight_sum = 0.0f;
    [unroll]
    for (int offset_y = -1; offset_y <= 1; ++offset_y) {
        [unroll]
        for (int offset_x = -1; offset_x <= 1; ++offset_x) {
            const float2 offset = float2((float)offset_x, (float)offset_y);
            const float2 sample_uv = saturate(
                input.uv + offset * g_filter.texture_params.xy);
            const float sample_depth = g_scene_depth.SampleLevel(
                g_source_sampler,
                sample_uv,
                0.0f);
            if (sample_depth >= 0.99999f) {
                continue;
            }

            const float3 sample_position = reconstructWorldPosition(sample_uv, sample_depth);
            const float3 sample_normal = reconstructWorldNormal(sample_uv, sample_position);
            const float sample_distance = length(camera_position - sample_position);
            const float depth_delta = abs(sample_distance - center_distance);
            const float depth_weight = exp(-depth_delta / depth_threshold);
            float normal_weight = saturate(
                (dot(center_normal, sample_normal) - normal_threshold) /
                normal_range);
            normal_weight *= normal_weight;
            const float spatial_weight = exp(-0.5f * dot(offset, offset));
            const float weight = depth_weight * normal_weight * spatial_weight;
            const float sample_ao = g_source_ao.SampleLevel(
                g_source_sampler,
                sample_uv,
                0.0f);
            weighted_ao += sample_ao * weight;
            weight_sum += weight;
        }
    }

    const float filtered_ao = weight_sum > 0.00001f ?
        weighted_ao / weight_sum : center_ao;
    return float4(filtered_ao, filtered_ao, filtered_ao, 1.0f);
}
