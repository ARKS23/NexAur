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
static const int SSR_SAMPLE_VALID = 0;
static const int SSR_SAMPLE_SKY = 1;
static const int SSR_SAMPLE_OFFSCREEN = 2;
static const int SSR_SAMPLE_NEAR_PLANE = 3;
static const int SSR_SAMPLE_SELF = 4;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float loadSceneDepth(float2 uv) {
    const float2 texel_size = max(g_ssr.texture_params.xy, float2(0.000001f, 0.000001f));
    const uint2 texture_extent = max((uint2)round(1.0f / texel_size), uint2(1u, 1u));
    const uint2 texel_coord = min(
        (uint2)(saturate(uv) * float2(texture_extent)),
        texture_extent - 1u);
    return g_scene_depth.Load(int3(int2(texel_coord), 0));
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
    const float depth_left = loadSceneDepth(uv_left);
    const float depth_right = loadSceneDepth(uv_right);
    const float depth_up = loadSceneDepth(uv_up);
    const float depth_down = loadSceneDepth(uv_down);

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

float computeAdaptiveThickness(float ray_view_depth, float base_thickness) {
    return base_thickness * max(1.0f, ray_view_depth * 0.04f);
}

bool withinHitWindow(float depth_delta, float front_thickness, float back_thickness) {
    return depth_delta >= -front_thickness && depth_delta <= back_thickness;
}

bool crossedHitWindow(
    float previous_delta,
    float current_delta,
    float front_thickness,
    float back_thickness) {
    return (previous_delta < -front_thickness && current_delta > back_thickness) ||
           (previous_delta > back_thickness && current_delta < -front_thickness);
}

int sampleRayDepth(
    float3 ray_origin,
    float3 surface_normal,
    float3 reflection_ray,
    float ray_distance,
    float self_hit_distance,
    out float2 ray_uv,
    out float depth_delta,
    out float ray_view_depth,
    out float surface_distance) {
    ray_uv = 0.0f;
    depth_delta = 0.0f;
    ray_view_depth = 0.0f;
    surface_distance = 0.0f;

    const float3 ray_position = ray_origin + reflection_ray * ray_distance;
    if (ray_position.z >= -0.001f) {
        return SSR_SAMPLE_NEAR_PLANE;
    }

    ray_uv = projectViewPosition(ray_position);
    if (!insideScreen(ray_uv)) {
        return SSR_SAMPLE_OFFSCREEN;
    }

    const float sample_depth = loadSceneDepth(ray_uv);
    if (sample_depth >= 0.99999f) {
        return SSR_SAMPLE_SKY;
    }

    const float3 sample_view_position = reconstructViewPosition(ray_uv, sample_depth);
    surface_distance = abs(dot(sample_view_position - ray_origin, surface_normal));
    if (surface_distance <= self_hit_distance) {
        return SSR_SAMPLE_SELF;
    }

    const float sample_view_depth = -sample_view_position.z;
    ray_view_depth = max(-ray_position.z, 0.0001f);
    depth_delta = ray_view_depth - sample_view_depth;
    return SSR_SAMPLE_VALID;
}

float refineCrossingHitDistance(
    float3 ray_origin,
    float3 surface_normal,
    float3 reflection_ray,
    float low_distance,
    float low_delta,
    float high_distance,
    float high_delta,
    float self_hit_distance,
    out float2 refined_uv,
    out float refined_delta,
    out float refined_ray_view_depth) {
    float best_distance = high_distance;
    refined_delta = high_delta;
    refined_ray_view_depth = max(-(ray_origin + reflection_ray * high_distance).z, 0.0001f);
    refined_uv = projectViewPosition(ray_origin + reflection_ray * high_distance);

    float low = low_distance;
    float high = high_distance;
    const bool low_is_front = low_delta < 0.0f;

    [unroll]
    for (uint refine_index = 0u; refine_index < 4u; ++refine_index) {
        const float mid_distance = (low + high) * 0.5f;
        float2 ray_uv = 0.0f;
        float depth_delta = 0.0f;
        float ray_view_depth = 0.0f;
        float surface_distance = 0.0f;
        const int sample_status = sampleRayDepth(
            ray_origin,
            surface_normal,
            reflection_ray,
            mid_distance,
            self_hit_distance,
            ray_uv,
            depth_delta,
            ray_view_depth,
            surface_distance);

        if (sample_status == SSR_SAMPLE_OFFSCREEN || sample_status == SSR_SAMPLE_NEAR_PLANE) {
            high = mid_distance;
            continue;
        }
        if (sample_status == SSR_SAMPLE_SKY || sample_status == SSR_SAMPLE_SELF) {
            low = mid_distance;
            continue;
        }

        if (abs(depth_delta) < abs(refined_delta)) {
            best_distance = mid_distance;
            refined_delta = depth_delta;
            refined_ray_view_depth = ray_view_depth;
            refined_uv = ray_uv;
        }

        const bool mid_is_front = depth_delta < 0.0f;
        if (mid_is_front == low_is_front) {
            low = mid_distance;
            low_delta = depth_delta;
        } else {
            high = mid_distance;
            high_delta = depth_delta;
        }
    }

    return best_distance;
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float depth = loadSceneDepth(input.uv);
    if (depth >= 0.99999f) {
        return 0.0f;
    }

    const float3 view_position = reconstructViewPosition(input.uv, depth);
    const float3 view_normal = reconstructViewNormal(input.uv, depth, view_position);
    const float3 view_ray = normalize(view_position);
    const float3 reflection_ray = normalize(reflect(view_ray, view_normal));

    if (abs(reflection_ray.z) <= 0.01f) {
        return 0.0f;
    }

    const uint max_steps = clamp((uint)round(g_ssr.texture_params.z), 1u, SSR_MAX_STEPS);
    const float max_distance = max(g_ssr.trace_params.x, 0.001f);
    const float thickness = max(g_ssr.trace_params.y, 0.0001f);
    const float stride = max(g_ssr.trace_params.z, 0.1f);
    const float step_distance = max_distance / (float)max_steps;
    const float ray_step = max(step_distance * stride, 0.01f);
    const float min_trace_distance = max(0.02f, min(thickness * 0.35f, ray_step * 0.25f));
    const float self_hit_distance = clamp(thickness * 0.35f, 0.015f, 0.08f);
    const float front_thickness = max(thickness * 0.5f, 0.01f);
    const float edge_weight = computeEdgeFade(input.uv);
    const float grazing_weight = computeGrazingFade(view_normal, view_ray);

    float2 hit_uv = input.uv;
    float hit_confidence = 0.0f;
    float normalized_steps = 0.0f;
    float hit_depth_delta = 0.0f;
    float hit_ray_view_depth = 0.0f;
    float previous_distance = 0.0f;
    float previous_delta = 0.0f;
    bool previous_valid = false;

    [loop]
    for (uint step_index = 0u; step_index < SSR_MAX_STEPS; ++step_index) {
        if (step_index >= max_steps) {
            break;
        }

        const float ray_distance = min_trace_distance + ray_step * (float)step_index;
        if (ray_distance > max_distance) {
            break;
        }

        float2 ray_uv = 0.0f;
        float depth_delta = 0.0f;
        float ray_view_depth = 0.0f;
        float surface_distance = 0.0f;
        const int sample_status = sampleRayDepth(
            view_position,
            view_normal,
            reflection_ray,
            ray_distance,
            self_hit_distance,
            ray_uv,
            depth_delta,
            ray_view_depth,
            surface_distance);

        if (sample_status == SSR_SAMPLE_OFFSCREEN || sample_status == SSR_SAMPLE_NEAR_PLANE) {
            break;
        }
        if (sample_status == SSR_SAMPLE_SKY || sample_status == SSR_SAMPLE_SELF) {
            previous_valid = false;
            continue;
        }

        const float adaptive_thickness = computeAdaptiveThickness(ray_view_depth, thickness);
        const bool direct_hit = withinHitWindow(depth_delta, front_thickness, adaptive_thickness);
        const bool crossed_hit =
            previous_valid &&
            crossedHitWindow(previous_delta, depth_delta, front_thickness, adaptive_thickness);

        if (direct_hit || crossed_hit) {
            float hit_distance = ray_distance;
            hit_uv = ray_uv;
            hit_depth_delta = depth_delta;
            hit_ray_view_depth = ray_view_depth;

            if (crossed_hit) {
                hit_distance = refineCrossingHitDistance(
                    view_position,
                    view_normal,
                    reflection_ray,
                    previous_distance,
                    previous_delta,
                    ray_distance,
                    depth_delta,
                    self_hit_distance,
                    hit_uv,
                    hit_depth_delta,
                    hit_ray_view_depth);
            }

            const float hit_thickness = computeAdaptiveThickness(hit_ray_view_depth, thickness);
            normalized_steps = saturate(hit_distance / max_distance);
            const float distance_weight = saturate(1.0f - normalized_steps * 0.75f);
            const float hit_edge_weight = computeEdgeFade(hit_uv);
            const float facing_weight = lerp(0.35f, 1.0f, saturate(abs(reflection_ray.z) * 2.0f));
            const float thickness_weight =
                1.0f - saturate(abs(hit_depth_delta) / max(max(hit_thickness, front_thickness), 0.0001f));
            hit_confidence =
                edge_weight *
                hit_edge_weight *
                distance_weight *
                thickness_weight *
                facing_weight *
                grazing_weight;
            break;
        }

        previous_distance = ray_distance;
        previous_delta = depth_delta;
        previous_valid = true;
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
