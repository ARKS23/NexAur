#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_color_input;

[[vk::binding(1, 0)]]
SamplerState g_input_sampler;

struct SmaaEdgePushConstants {
    float source_texel_width;
    float source_texel_height;
    float edge_threshold;
    float contrast_factor;
};

[[vk::push_constant]]
SmaaEdgePushConstants g_smaa;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float luminance(float3 color) {
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float sampleLuma(float2 uv) {
    return luminance(g_color_input.SampleLevel(g_input_sampler, uv, 0.0f).rgb);
}

float edgeStrength(float contrast, float threshold, float contrast_factor) {
    const float edge = contrast > threshold ? contrast : 0.0f;
    return saturate(edge * contrast_factor);
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float2 texel_size = float2(g_smaa.source_texel_width, g_smaa.source_texel_height);
    const float center = sampleLuma(input.uv);
    const float left = sampleLuma(input.uv - float2(texel_size.x, 0.0f));
    const float right = sampleLuma(input.uv + float2(texel_size.x, 0.0f));
    const float up = sampleLuma(input.uv - float2(0.0f, texel_size.y));
    const float down = sampleLuma(input.uv + float2(0.0f, texel_size.y));

    const float threshold = max(g_smaa.edge_threshold, 0.001f);
    const float contrast_factor = max(g_smaa.contrast_factor, 0.0f);
    const float horizontal_contrast = max(abs(center - left), abs(center - right));
    const float vertical_contrast = max(abs(center - up), abs(center - down));

    const float horizontal_edge = edgeStrength(horizontal_contrast, threshold, contrast_factor);
    const float vertical_edge = edgeStrength(vertical_contrast, threshold, contrast_factor);
    return float4(horizontal_edge, vertical_edge, 0.0f, 1.0f);
}
