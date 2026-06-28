#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_high_color;

[[vk::binding(1, 0)]]
Texture2D<float4> g_low_color;

[[vk::binding(2, 0)]]
SamplerState g_bloom_sampler;

struct BloomUpsamplePushConstants {
    float low_texel_width;
    float low_texel_height;
    float radius;
    float scatter;
};

[[vk::push_constant]]
BloomUpsamplePushConstants g_bloom;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float3 sampleLowTent(float2 uv) {
    const float2 texel = float2(g_bloom.low_texel_width, g_bloom.low_texel_height) * g_bloom.radius;

    float3 color = 0.0f;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv + texel * float2(-1.0f, -1.0f), 0.0f).rgb;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv + texel * float2( 0.0f, -1.0f), 0.0f).rgb * 2.0f;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv + texel * float2( 1.0f, -1.0f), 0.0f).rgb;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv + texel * float2(-1.0f,  0.0f), 0.0f).rgb * 2.0f;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv, 0.0f).rgb * 4.0f;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv + texel * float2( 1.0f,  0.0f), 0.0f).rgb * 2.0f;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv + texel * float2(-1.0f,  1.0f), 0.0f).rgb;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv + texel * float2( 0.0f,  1.0f), 0.0f).rgb * 2.0f;
    color += g_low_color.SampleLevel(g_bloom_sampler, uv + texel * float2( 1.0f,  1.0f), 0.0f).rgb;

    return max(color * (1.0f / 16.0f), 0.0f);
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float3 high_color = max(g_high_color.SampleLevel(g_bloom_sampler, input.uv, 0.0f).rgb, 0.0f);
    const float3 low_color = sampleLowTent(input.uv);
    return float4(high_color + low_color * saturate(g_bloom.scatter), 1.0f);
}
