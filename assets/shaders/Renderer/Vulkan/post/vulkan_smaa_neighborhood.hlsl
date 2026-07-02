#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_color_input;

[[vk::binding(1, 0)]]
Texture2D<float4> g_blend_input;

[[vk::binding(2, 0)]]
SamplerState g_input_sampler;

struct SmaaNeighborhoodPushConstants {
    float source_texel_width;
    float source_texel_height;
    float blend_strength;
    uint debug_mode;
};

[[vk::push_constant]]
SmaaNeighborhoodPushConstants g_smaa;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float4 center = g_color_input.SampleLevel(g_input_sampler, input.uv, 0.0f);
    if (g_smaa.debug_mode != 0u) {
        return center;
    }

    const float2 texel_size = float2(g_smaa.source_texel_width, g_smaa.source_texel_height);
    const float2 blend_weight = saturate(g_blend_input.SampleLevel(g_input_sampler, input.uv, 0.0f).rg) *
        saturate(g_smaa.blend_strength);

    const float3 left = g_color_input.SampleLevel(g_input_sampler, input.uv - float2(texel_size.x, 0.0f), 0.0f).rgb;
    const float3 right = g_color_input.SampleLevel(g_input_sampler, input.uv + float2(texel_size.x, 0.0f), 0.0f).rgb;
    const float3 up = g_color_input.SampleLevel(g_input_sampler, input.uv - float2(0.0f, texel_size.y), 0.0f).rgb;
    const float3 down = g_color_input.SampleLevel(g_input_sampler, input.uv + float2(0.0f, texel_size.y), 0.0f).rgb;

    const float3 horizontal = (left + right) * 0.5f;
    const float3 vertical = (up + down) * 0.5f;
    float3 color = lerp(center.rgb, horizontal, blend_weight.x);
    color = lerp(color, vertical, blend_weight.y * (1.0f - 0.5f * blend_weight.x));
    return float4(saturate(color), center.a);
}
