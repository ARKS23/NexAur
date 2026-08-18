#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float> g_scene_depth;

[[vk::binding(1, 0)]]
SamplerState g_scene_depth_sampler;

[[vk::binding(0, 1)]]
RaytracingAccelerationStructure g_ray_query_scene;

struct RtaoPushConstants {
    float4x4 inverse_view_projection;
    float4 camera_position_max_distance; // xyz: camera, w: ray distance
    float4 ao_params; // x: intensity, y: normal bias, z: power, w: ray count
    float4 depth_texture_params; // xy: depth texel size, zw: unused
};

[[vk::push_constant]]
RtaoPushConstants g_rtao;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float3 reconstructWorldPosition(float2 uv, float depth) {
    const float2 ndc = uv * 2.0f - 1.0f;
    float4 world_position = mul(
        g_rtao.inverse_view_projection,
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
    const float2 texel = g_rtao.depth_texture_params.xy;
    const float2 left_uv = saturate(uv - float2(texel.x, 0.0f));
    const float2 right_uv = saturate(uv + float2(texel.x, 0.0f));
    const float2 up_uv = saturate(uv - float2(0.0f, texel.y));
    const float2 down_uv = saturate(uv + float2(0.0f, texel.y));

    const float left_depth = g_scene_depth.SampleLevel(g_scene_depth_sampler, left_uv, 0.0f);
    const float right_depth = g_scene_depth.SampleLevel(g_scene_depth_sampler, right_uv, 0.0f);
    const float up_depth = g_scene_depth.SampleLevel(g_scene_depth_sampler, up_uv, 0.0f);
    const float down_depth = g_scene_depth.SampleLevel(g_scene_depth_sampler, down_uv, 0.0f);

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
        g_rtao.camera_position_max_distance.xyz - center_position);
    float3 normal = cross(horizontal, vertical);
    if (dot(normal, normal) <= 0.0000001f) {
        return view_direction;
    }
    normal = normalize(normal);
    return dot(normal, view_direction) < 0.0f ? -normal : normal;
}

float hashPixel(float2 pixel) {
    float3 value = frac(float3(pixel.xyx) * 0.1031f);
    value += dot(value, value.yzx + 33.33f);
    return frac((value.x + value.y) * value.z);
}

float3 cosineHemisphereDirection(
    float3 normal,
    float2 pixel,
    uint sample_index,
    uint sample_count) {
    const float sequence = ((float)sample_index + 0.5f) / (float)sample_count;
    const float u1 = frac(sequence + hashPixel(pixel));
    const float u2 = frac(
        hashPixel(pixel + float2((float)sample_index * 17.0f, 41.0f)) +
        hashPixel(pixel.yx + 7.0f));
    const float radius = sqrt(u1);
    const float angle = 6.2831853f * u2;
    const float3 local_direction = float3(
        radius * cos(angle),
        radius * sin(angle),
        sqrt(max(1.0f - u1, 0.0f)));

    const float3 reference_axis = abs(normal.z) < 0.999f ?
        float3(0.0f, 0.0f, 1.0f) :
        float3(1.0f, 0.0f, 0.0f);
    const float3 tangent = normalize(cross(reference_axis, normal));
    const float3 bitangent = cross(normal, tangent);
    return normalize(
        tangent * local_direction.x +
        bitangent * local_direction.y +
        normal * local_direction.z);
}

bool traceOcclusion(float3 origin, float3 direction, float max_distance) {
    RayQuery<
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
        RAY_FLAG_FORCE_OPAQUE |
        RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
    RayDesc ray;
    ray.Origin = origin;
    ray.TMin = 0.001f;
    ray.Direction = direction;
    ray.TMax = max_distance;
    query.TraceRayInline(g_ray_query_scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed()) {
    }
    return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float depth = g_scene_depth.SampleLevel(
        g_scene_depth_sampler,
        input.uv,
        0.0f);
    if (depth >= 0.99999f) {
        return 1.0f;
    }

    const float3 world_position = reconstructWorldPosition(input.uv, depth);
    const float3 world_normal = reconstructWorldNormal(input.uv, world_position);
    const float normal_bias = max(g_rtao.ao_params.y, 0.0f);
    const float max_distance = max(g_rtao.camera_position_max_distance.w, 0.01f);
    const uint ray_count = clamp((uint)g_rtao.ao_params.w, 1u, 8u);
    const float3 ray_origin = world_position + world_normal * normal_bias;

    float occlusion = 0.0f;
    [loop]
    for (uint ray_index = 0u; ray_index < ray_count; ++ray_index) {
        const float3 ray_direction = cosineHemisphereDirection(
            world_normal,
            input.position.xy,
            ray_index,
            ray_count);
        occlusion += traceOcclusion(ray_origin, ray_direction, max_distance) ? 1.0f : 0.0f;
    }

    occlusion /= (float)ray_count;
    float ao = 1.0f - occlusion * saturate(g_rtao.ao_params.x);
    ao = pow(saturate(ao), max(g_rtao.ao_params.z, 0.01f));
    return float4(ao, ao, ao, 1.0f);
}
