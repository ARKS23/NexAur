#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_scene_color;

[[vk::binding(1, 0)]]
Texture2D<float> g_scene_depth;

[[vk::binding(2, 0)]]
SamplerState g_scene_sampler;

struct SsrTracePushConstants {
    float4x4 inverse_projection;
    float4 trace_params; // x: max distance, y: thickness, z: stride, w: intensity
    float4 texture_params; // x/y: texel size, z: max steps, w: edge fade
    float4 output_params; // x: hit-mask output, y: roughness fade placeholder, zw: unused
};

[[vk::push_constant]]
SsrTracePushConstants g_ssr;

static const uint SSR_MAX_STEPS = 96u;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float3 reconstructViewPosition(float2 uv, float depth) {
    const float2 ndc = uv * 2.0f - 1.0f;
    float4 view_position = mul(g_ssr.inverse_projection, float4(ndc, depth, 1.0f));
    view_position.xyz /= max(abs(view_position.w), 0.00001f);
    return view_position.xyz;
}

float3 reconstructViewNormal(float2 uv, float center_depth, float3 center_view_position) {
    const float2 texel_size = max(g_ssr.texture_params.xy, float2(0.000001f, 0.000001f));
    const float2 uv_x = saturate(uv + float2(texel_size.x, 0.0f));
    const float2 uv_y = saturate(uv + float2(0.0f, texel_size.y));
    const float depth_x = g_scene_depth.SampleLevel(g_scene_sampler, uv_x, 0.0f);
    const float depth_y = g_scene_depth.SampleLevel(g_scene_sampler, uv_y, 0.0f);

    if (depth_x >= 0.99999f || depth_y >= 0.99999f) {
        return float3(0.0f, 0.0f, 1.0f);
    }

    const float3 view_position_x = reconstructViewPosition(uv_x, depth_x);
    const float3 view_position_y = reconstructViewPosition(uv_y, depth_y);
    const float3 dx = view_position_x - center_view_position;
    const float3 dy = view_position_y - center_view_position;
    float3 normal = normalize(cross(dx, dy));
    if (!all(isfinite(normal)) || dot(normal, normal) <= 0.000001f) {
        return float3(0.0f, 0.0f, 1.0f);
    }
    if (normal.z < 0.0f) {
        normal = -normal;
    }
    return normal;
}

float computeEdgeFade(float2 uv) {
    const float edge_distance = min(min(uv.x, uv.y), min(1.0f - uv.x, 1.0f - uv.y));
    const float edge_fade = max(g_ssr.texture_params.w, 0.0001f);
    return saturate(edge_distance / edge_fade);
}

float4 encodeDebugColor(float3 color, float normalized_steps) {
    return float4(max(color, 0.0f), saturate(normalized_steps));
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float depth = g_scene_depth.SampleLevel(g_scene_sampler, input.uv, 0.0f);
    if (depth >= 0.99999f) {
        return 0.0f;
    }

    const float3 view_position = reconstructViewPosition(input.uv, depth);
    const float center_view_depth = max(-view_position.z, 0.0001f);
    const float3 view_normal = reconstructViewNormal(input.uv, depth, view_position);
    const float3 view_ray = normalize(view_position);
    const float3 reflection_ray = normalize(reflect(view_ray, view_normal));
    const float2 screen_ray = reflection_ray.xy;
    const float screen_ray_length = length(screen_ray);

    if (reflection_ray.z >= -0.01f || screen_ray_length <= 0.000001f) {
        return 0.0f;
    }

    const uint max_steps = clamp((uint)round(g_ssr.texture_params.z), 1u, SSR_MAX_STEPS);
    const float max_distance = max(g_ssr.trace_params.x, 0.001f);
    const float thickness = max(g_ssr.trace_params.y, 0.0001f);
    const float stride = max(g_ssr.trace_params.z, 0.1f);
    const float2 step_uv =
        normalize(screen_ray) *
        max(g_ssr.texture_params.xy, float2(0.000001f, 0.000001f)) *
        stride;
    const float ray_depth_step = max_distance / (float)max_steps * max(-reflection_ray.z, 0.05f);
    const float edge_weight = computeEdgeFade(input.uv);

    float2 hit_uv = input.uv;
    float hit_confidence = 0.0f;
    float normalized_steps = 0.0f;

    [loop]
    for (uint step_index = 1u; step_index <= SSR_MAX_STEPS; ++step_index) {
        if (step_index > max_steps) {
            break;
        }

        const float2 ray_uv = input.uv + step_uv * (float)step_index;
        if (any(ray_uv <= 0.0f) || any(ray_uv >= 1.0f)) {
            break;
        }

        const float sample_depth = g_scene_depth.SampleLevel(g_scene_sampler, ray_uv, 0.0f);
        if (sample_depth >= 0.99999f) {
            continue;
        }

        const float sample_view_depth = -reconstructViewPosition(ray_uv, sample_depth).z;
        const float ray_view_depth = center_view_depth + ray_depth_step * (float)step_index;
        const float depth_delta = ray_view_depth - sample_view_depth;
        const float adaptive_thickness = thickness * max(1.0f, ray_view_depth * 0.04f);
        if (depth_delta >= -thickness && depth_delta <= adaptive_thickness) {
            hit_uv = ray_uv;
            normalized_steps = (float)step_index / (float)max_steps;
            const float step_weight = 1.0f - normalized_steps;
            const float hit_edge_weight = computeEdgeFade(ray_uv);
            const float facing_weight = saturate(-reflection_ray.z);
            hit_confidence =
                saturate(g_ssr.trace_params.w) *
                edge_weight *
                hit_edge_weight *
                step_weight *
                facing_weight;
            break;
        }
    }

    const bool hit = hit_confidence > 0.0001f;
    if (g_ssr.output_params.x > 0.5f) {
        return float4(hit_confidence, hit ? 1.0f : 0.0f, normalized_steps, 1.0f);
    }

    const float3 raw_reflection = hit ?
        g_scene_color.SampleLevel(g_scene_sampler, hit_uv, 0.0f).rgb * hit_confidence :
        0.0f;
    return encodeDebugColor(raw_reflection, hit ? normalized_steps : 0.0f);
}
