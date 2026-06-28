#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_scene_color;

[[vk::binding(1, 0)]]
Texture2D<float4> g_bloom_color;

[[vk::binding(2, 0)]]
SamplerState g_bloom_sampler;

struct BloomCompositePushConstants {
    float intensity;
    float padding0;
    float padding1;
    float padding2;
};

[[vk::push_constant]]
BloomCompositePushConstants g_bloom;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float4 scene_color = g_scene_color.SampleLevel(g_bloom_sampler, input.uv, 0.0f);
    const float3 bloom_color = max(g_bloom_color.SampleLevel(g_bloom_sampler, input.uv, 0.0f).rgb, 0.0f);
    return float4(max(scene_color.rgb, 0.0f) + bloom_color * max(g_bloom.intensity, 0.0f), scene_color.a);
}
