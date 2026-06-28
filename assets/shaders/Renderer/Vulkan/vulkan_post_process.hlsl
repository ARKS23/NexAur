#include "common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_hdr_scene_color;

[[vk::binding(1, 0)]]
SamplerState g_scene_sampler;

static const uint TONE_MAPPING_NONE = 0u;
static const uint TONE_MAPPING_ACES = 1u;
static const uint POST_PROCESS_FLAG_MANUAL_GAMMA = 1u << 0u;

struct PostProcessPushConstants {
    float exposure;
    float output_gamma;
    uint tone_mapping_mode;
    uint flags;
};

[[vk::push_constant]]
PostProcessPushConstants g_post_process;

FullscreenVSOutput VSMain(uint vertex_id : SV_VertexID) {
    return FullscreenTriangleVS(vertex_id);
}

float3 applyExposure(float3 color) {
    return color * max(g_post_process.exposure, 0.0f);
}

float3 acesFitted(float3 color) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float3 linearToSrgb(float3 color) {
    const float gamma = max(g_post_process.output_gamma, 0.0001f);
    return pow(saturate(color), 1.0f / gamma);
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    float4 hdr_color = g_hdr_scene_color.SampleLevel(g_scene_sampler, input.uv, 0.0f);
    float3 color = max(hdr_color.rgb, 0.0f);

    color = applyExposure(color);
    if (g_post_process.tone_mapping_mode == TONE_MAPPING_ACES) {
        color = acesFitted(color);
    } else {
        color = saturate(color);
    }

    if ((g_post_process.flags & POST_PROCESS_FLAG_MANUAL_GAMMA) != 0u) {
        color = linearToSrgb(color);
    }

    return float4(saturate(color), saturate(hdr_color.a));
}
