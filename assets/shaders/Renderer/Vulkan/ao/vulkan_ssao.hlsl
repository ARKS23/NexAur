#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float> g_scene_depth;

[[vk::binding(1, 0)]]
SamplerState g_scene_depth_sampler;

struct SsaoPushConstants {
    float4x4 inverse_projection;
    float4 ao_params; // x: radius, y: intensity, z: bias, w: power
    float4 texture_params; // x/y: depth texel size, z: aspect, w: unused
};

[[vk::push_constant]]
SsaoPushConstants g_ssao;

static const uint SSAO_SAMPLE_COUNT = 16u;
static const float2 SSAO_DISK[16] = {
    float2( 0.5381f,  0.1856f),
    float2( 0.1379f,  0.2486f),
    float2( 0.3371f,  0.5679f),
    float2(-0.6999f, -0.0451f),
    float2(-0.0689f, -0.1598f),
    float2( 0.0560f,  0.0069f),
    float2(-0.0146f,  0.1402f),
    float2( 0.0100f, -0.1924f),
    float2(-0.3577f, -0.5301f),
    float2(-0.3169f,  0.1063f),
    float2( 0.0103f, -0.5869f),
    float2(-0.0897f, -0.4940f),
    float2( 0.7119f, -0.0154f),
    float2(-0.0533f,  0.0596f),
    float2( 0.0352f, -0.0631f),
    float2(-0.4776f,  0.2847f)
};

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float3 reconstructViewPosition(float2 uv, float depth) {
    const float2 ndc = uv * 2.0f - 1.0f;
    float4 view_position = mul(g_ssao.inverse_projection, float4(ndc, depth, 1.0f));
    view_position.xyz /= max(abs(view_position.w), 0.00001f);
    return view_position.xyz;
}

float interleavedGradientNoise(float2 pixel) {
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float2 rotateOffset(float2 offset, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return float2(offset.x * c - offset.y * s, offset.x * s + offset.y * c);
}

float sampleOcclusion(float2 uv, float center_view_depth, float2 offset) {
    const float2 sample_uv = saturate(uv + offset);
    const float sample_depth = g_scene_depth.SampleLevel(g_scene_depth_sampler, sample_uv, 0.0f);
    if (sample_depth >= 0.99999f) {
        return 0.0f;
    }

    const float sample_view_depth = -reconstructViewPosition(sample_uv, sample_depth).z;
    const float depth_delta = center_view_depth - sample_view_depth;
    const float range_weight = smoothstep(
        0.0f,
        1.0f,
        g_ssao.ao_params.x / max(abs(depth_delta), 0.0001f));
    return depth_delta > g_ssao.ao_params.z ? range_weight : 0.0f;
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float depth = g_scene_depth.SampleLevel(g_scene_depth_sampler, input.uv, 0.0f);
    if (depth >= 0.99999f) {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    const float center_view_depth = -reconstructViewPosition(input.uv, depth).z;
    const float radius = max(g_ssao.ao_params.x, 0.001f);
    const float screen_radius = radius / max(center_view_depth, 0.25f);
    const float2 radius_uv = float2(screen_radius / max(g_ssao.texture_params.z, 0.001f), screen_radius);
    const float angle = interleavedGradientNoise(input.position.xy) * 6.2831853f;

    float occlusion = 0.0f;
    [unroll]
    for (uint index = 0u; index < SSAO_SAMPLE_COUNT; ++index) {
        occlusion += sampleOcclusion(input.uv, center_view_depth, rotateOffset(SSAO_DISK[index], angle) * radius_uv);
    }

    occlusion /= (float)SSAO_SAMPLE_COUNT;
    float ao = 1.0f - occlusion * saturate(g_ssao.ao_params.y);
    ao = pow(saturate(ao), max(g_ssao.ao_params.w, 0.01f));
    return float4(ao, ao, ao, 1.0f);
}
