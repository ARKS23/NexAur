#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_scene_color;

[[vk::binding(1, 0)]]
Texture2D<float> g_scene_depth;

[[vk::binding(2, 0)]]
SamplerState g_scene_sampler;

struct SsrTracePushConstants {
    float4x4 inverse_projection;
    float4 trace_params; // x: max distance, y: thickness, z: stride, w: unused
    float4 texture_params; // x/y: texel size, z: max steps, w: edge fade
    float4 output_params; // x: hit-mask output, y: roughness fade placeholder, zw: projection scale
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

float2 projectViewPosition(float3 view_position) {
    const float view_w = max(-view_position.z, 0.00001f);
    const float2 ndc = float2(
        view_position.x * g_ssr.output_params.z / view_w,
        view_position.y * g_ssr.output_params.w / view_w);
    return ndc * 0.5f + 0.5f;
}

bool insideScreen(float2 uv) {
    return all(uv > 0.0f) && all(uv < 1.0f);
}

float3 reconstructViewNormal(float2 uv, float center_depth, float3 center_view_position) {
    const float2 texel_size = max(g_ssr.texture_params.xy, float2(0.000001f, 0.000001f));
    const float2 uv_left = saturate(uv - float2(texel_size.x, 0.0f));
    const float2 uv_right = saturate(uv + float2(texel_size.x, 0.0f));
    const float2 uv_up = saturate(uv - float2(0.0f, texel_size.y));
    const float2 uv_down = saturate(uv + float2(0.0f, texel_size.y));
    const float depth_left = g_scene_depth.SampleLevel(g_scene_sampler, uv_left, 0.0f);
    const float depth_right = g_scene_depth.SampleLevel(g_scene_sampler, uv_right, 0.0f);
    const float depth_up = g_scene_depth.SampleLevel(g_scene_sampler, uv_up, 0.0f);
    const float depth_down = g_scene_depth.SampleLevel(g_scene_sampler, uv_down, 0.0f);

    const bool valid_left = depth_left < 0.99999f;
    const bool valid_right = depth_right < 0.99999f;
    const bool valid_up = depth_up < 0.99999f;
    const bool valid_down = depth_down < 0.99999f;

    if ((!valid_left && !valid_right) || (!valid_up && !valid_down)) {
        return float3(0.0f, 0.0f, 1.0f);
    }

    const float3 view_position_left = valid_left ? reconstructViewPosition(uv_left, depth_left) : center_view_position;
    const float3 view_position_right = valid_right ? reconstructViewPosition(uv_right, depth_right) : center_view_position;
    const float3 view_position_up = valid_up ? reconstructViewPosition(uv_up, depth_up) : center_view_position;
    const float3 view_position_down = valid_down ? reconstructViewPosition(uv_down, depth_down) : center_view_position;
    const float3 dx = valid_right && (!valid_left || abs(depth_right - center_depth) <= abs(depth_left - center_depth)) ?
        view_position_right - center_view_position :
        center_view_position - view_position_left;
    const float3 dy = valid_down && (!valid_up || abs(depth_down - center_depth) <= abs(depth_up - center_depth)) ?
        view_position_down - center_view_position :
        center_view_position - view_position_up;

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

float computeGrazingFade(float3 view_normal, float3 view_ray) {
    const float n_dot_v = saturate(dot(view_normal, -view_ray));
    return lerp(0.35f, 1.0f, pow(1.0f - n_dot_v, 2.0f));
}

float4 encodeDebugColor(float3 color, float normalized_steps) {
    return float4(max(color, 0.0f), saturate(normalized_steps));
}

float refineHitDistance(
    float3 ray_origin,
    float3 reflection_ray,
    float low_distance,
    float high_distance,
    float thickness) {
    [unroll]
    for (uint refine_index = 0u; refine_index < 4u; ++refine_index) {
        const float mid_distance = (low_distance + high_distance) * 0.5f;
        const float3 ray_position = ray_origin + reflection_ray * mid_distance;
        const float2 ray_uv = projectViewPosition(ray_position);
        if (!insideScreen(ray_uv)) {
            high_distance = mid_distance;
            continue;
        }

        const float sample_depth = g_scene_depth.SampleLevel(g_scene_sampler, ray_uv, 0.0f);
        if (sample_depth >= 0.99999f) {
            low_distance = mid_distance;
            continue;
        }

        const float sample_view_depth = -reconstructViewPosition(ray_uv, sample_depth).z;
        const float ray_view_depth = max(-ray_position.z, 0.0001f);
        const float depth_delta = ray_view_depth - sample_view_depth;
        if (depth_delta >= -thickness) {
            high_distance = mid_distance;
        } else {
            low_distance = mid_distance;
        }
    }

    return high_distance;
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float depth = g_scene_depth.SampleLevel(g_scene_sampler, input.uv, 0.0f);
    if (depth >= 0.99999f) {
        return 0.0f;
    }

    const float surface_reflection_mask =
        saturate(g_scene_color.SampleLevel(g_scene_sampler, input.uv, 0.0f).a);
    if (surface_reflection_mask <= 0.0001f) {
        return 0.0f;
    }

    const float3 view_position = reconstructViewPosition(input.uv, depth);
    const float3 view_normal = reconstructViewNormal(input.uv, depth, view_position);
    const float3 view_ray = normalize(view_position);
    const float3 reflection_ray = normalize(reflect(view_ray, view_normal));

    if (reflection_ray.z >= -0.01f) {
        return 0.0f;
    }

    const uint max_steps = clamp((uint)round(g_ssr.texture_params.z), 1u, SSR_MAX_STEPS);
    const float max_distance = max(g_ssr.trace_params.x, 0.001f);
    const float thickness = max(g_ssr.trace_params.y, 0.0001f);
    const float stride = max(g_ssr.trace_params.z, 0.1f);
    const float step_distance = max_distance / (float)max_steps;
    const float min_trace_distance = max(thickness * 2.0f, step_distance * 0.5f);
    const float edge_weight = computeEdgeFade(input.uv);
    const float grazing_weight = computeGrazingFade(view_normal, view_ray);

    float2 hit_uv = input.uv;
    float hit_confidence = 0.0f;
    float normalized_steps = 0.0f;
    float previous_distance = min_trace_distance;

    [loop]
    for (uint step_index = 1u; step_index <= SSR_MAX_STEPS; ++step_index) {
        if (step_index > max_steps) {
            break;
        }

        const float ray_distance = min_trace_distance + step_distance * (float)step_index * stride;
        if (ray_distance > max_distance) {
            break;
        }

        const float3 ray_position = view_position + reflection_ray * ray_distance;
        const float2 ray_uv = projectViewPosition(ray_position);
        if (!insideScreen(ray_uv)) {
            break;
        }

        const float sample_depth = g_scene_depth.SampleLevel(g_scene_sampler, ray_uv, 0.0f);
        if (sample_depth >= 0.99999f) {
            continue;
        }

        const float sample_view_depth = -reconstructViewPosition(ray_uv, sample_depth).z;
        const float ray_view_depth = max(-ray_position.z, 0.0001f);
        const float depth_delta = ray_view_depth - sample_view_depth;
        const float adaptive_thickness = thickness * max(1.0f, ray_view_depth * 0.04f);
        if (depth_delta >= -thickness && depth_delta <= adaptive_thickness) {
            const float hit_distance = refineHitDistance(
                view_position,
                reflection_ray,
                previous_distance,
                ray_distance,
                adaptive_thickness);
            hit_uv = projectViewPosition(view_position + reflection_ray * hit_distance);
            normalized_steps = saturate(hit_distance / max_distance);
            const float distance_weight = 1.0f - normalized_steps;
            const float hit_edge_weight = computeEdgeFade(hit_uv);
            const float facing_weight = saturate(-reflection_ray.z);
            const float thickness_weight = 1.0f - saturate(abs(depth_delta) / max(adaptive_thickness, thickness));
            hit_confidence =
                surface_reflection_mask *
                edge_weight *
                hit_edge_weight *
                distance_weight *
                thickness_weight *
                facing_weight *
                grazing_weight;
            break;
        }

        previous_distance = ray_distance;
    }

    const bool hit = hit_confidence > 0.0001f;
    if (g_ssr.output_params.x > 0.5f) {
        return float4(hit_confidence, hit ? 1.0f : 0.0f, normalized_steps, 1.0f);
    }

    const float3 raw_reflection = hit ?
        g_scene_color.SampleLevel(g_scene_sampler, hit_uv, 0.0f).rgb :
        0.0f;
    return encodeDebugColor(raw_reflection, hit ? normalized_steps : 0.0f);
}
