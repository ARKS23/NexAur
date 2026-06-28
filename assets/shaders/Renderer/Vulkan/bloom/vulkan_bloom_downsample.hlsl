#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_source_color;

[[vk::binding(1, 0)]]
SamplerState g_source_sampler;

struct BloomDownsamplePushConstants {
    float source_texel_width;
    float source_texel_height;
    uint prefilter;
    uint padding0;
};

[[vk::push_constant]]
BloomDownsamplePushConstants g_bloom;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float luminance(float3 color) {
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float sampleWeight(float3 color) {
    if (g_bloom.prefilter == 0u) {
        return 1.0f;
    }

    return rcp(1.0f + luminance(color));
}

float4 samplePrefiltered(float2 uv, float2 offset) {
    const float2 texel = float2(g_bloom.source_texel_width, g_bloom.source_texel_height);
    const float3 color = max(g_source_color.SampleLevel(g_source_sampler, uv + offset * texel, 0.0f).rgb, 0.0f);
    const float weight = sampleWeight(color);
    return float4(color * weight, weight);
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    float2 uv = input.uv;

    float4 color_weight = 0.0f;
    color_weight += samplePrefiltered(uv, float2(-1.0f, -1.0f));
    color_weight += samplePrefiltered(uv, float2( 0.0f, -1.0f)) * 2.0f;
    color_weight += samplePrefiltered(uv, float2( 1.0f, -1.0f));
    color_weight += samplePrefiltered(uv, float2(-1.0f,  0.0f)) * 2.0f;
    color_weight += samplePrefiltered(uv, float2( 0.0f,  0.0f)) * 4.0f;
    color_weight += samplePrefiltered(uv, float2( 1.0f,  0.0f)) * 2.0f;
    color_weight += samplePrefiltered(uv, float2(-1.0f,  1.0f));
    color_weight += samplePrefiltered(uv, float2( 0.0f,  1.0f)) * 2.0f;
    color_weight += samplePrefiltered(uv, float2( 1.0f,  1.0f));

    return float4(color_weight.rgb / max(color_weight.a, 0.0001f), 1.0f);
}
