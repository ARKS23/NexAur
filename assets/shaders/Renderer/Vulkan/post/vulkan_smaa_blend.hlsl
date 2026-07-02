#include "../common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_edge_input;

[[vk::binding(1, 0)]]
SamplerState g_input_sampler;

struct SmaaBlendPushConstants {
    float edge_texel_width;
    float edge_texel_height;
    uint max_search_steps;
    float padding0;
};

[[vk::push_constant]]
SmaaBlendPushConstants g_smaa;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float2 sampleEdge(float2 uv) {
    return g_edge_input.SampleLevel(g_input_sampler, uv, 0.0f).rg;
}

float searchSpan(float2 uv, float2 direction, uint component_index, uint max_steps) {
    float span = 0.0f;
    [loop]
    for (uint step_index = 1u; step_index <= 16u; ++step_index) {
        if (step_index > max_steps) {
            break;
        }

        const float2 edge = sampleEdge(uv + direction * (float)step_index);
        const float value = component_index == 0u ? edge.r : edge.g;
        if (value <= 0.001f) {
            break;
        }
        span += 1.0f;
    }

    return span;
}

float blendWeight(float center_edge, float left_span, float right_span, float max_steps) {
    if (center_edge <= 0.001f) {
        return 0.0f;
    }

    const float span = left_span + right_span + 1.0f;
    const float local_weight = saturate(2.0f / span);
    const float search_weight = saturate(span / max(max_steps, 1.0f));
    return saturate(center_edge * local_weight * (0.5f + 0.5f * search_weight));
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    const float2 texel_size = float2(g_smaa.edge_texel_width, g_smaa.edge_texel_height);
    const uint max_steps = clamp(g_smaa.max_search_steps, 1u, 16u);
    const float2 center_edge = sampleEdge(input.uv);

    const float left_span = searchSpan(input.uv, float2(-texel_size.x, 0.0f), 0u, max_steps);
    const float right_span = searchSpan(input.uv, float2(texel_size.x, 0.0f), 0u, max_steps);
    const float up_span = searchSpan(input.uv, float2(0.0f, -texel_size.y), 1u, max_steps);
    const float down_span = searchSpan(input.uv, float2(0.0f, texel_size.y), 1u, max_steps);

    const float horizontal_weight = blendWeight(center_edge.r, left_span, right_span, (float)max_steps);
    const float vertical_weight = blendWeight(center_edge.g, up_span, down_span, (float)max_steps);
    return float4(horizontal_weight, vertical_weight, 0.0f, 1.0f);
}
