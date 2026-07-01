#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float> g_source_ao;

[[vk::binding(1, 0)]]
SamplerState g_source_sampler;

struct AoBlurPushConstants {
    float source_texel_width;
    float source_texel_height;
    uint blur_enabled;
    uint padding0;
};

[[vk::push_constant]]
AoBlurPushConstants g_blur;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float sampleAo(float2 uv, float2 offset) {
    const float2 texel = float2(g_blur.source_texel_width, g_blur.source_texel_height);
    return g_source_ao.SampleLevel(g_source_sampler, uv + offset * texel, 0.0f);
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    if (g_blur.blur_enabled == 0u) {
        const float ao = sampleAo(input.uv, float2(0.0f, 0.0f));
        return float4(ao, ao, ao, 1.0f);
    }

    float ao = 0.0f;
    ao += sampleAo(input.uv, float2(-1.0f, -1.0f));
    ao += sampleAo(input.uv, float2( 0.0f, -1.0f)) * 2.0f;
    ao += sampleAo(input.uv, float2( 1.0f, -1.0f));
    ao += sampleAo(input.uv, float2(-1.0f,  0.0f)) * 2.0f;
    ao += sampleAo(input.uv, float2( 0.0f,  0.0f)) * 4.0f;
    ao += sampleAo(input.uv, float2( 1.0f,  0.0f)) * 2.0f;
    ao += sampleAo(input.uv, float2(-1.0f,  1.0f));
    ao += sampleAo(input.uv, float2( 0.0f,  1.0f)) * 2.0f;
    ao += sampleAo(input.uv, float2( 1.0f,  1.0f));
    ao *= 1.0f / 16.0f;

    return float4(ao, ao, ao, 1.0f);
}
