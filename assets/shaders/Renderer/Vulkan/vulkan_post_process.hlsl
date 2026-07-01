#include "common/vulkan_fullscreen_triangle.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> g_hdr_scene_color;

[[vk::binding(1, 0)]]
SamplerState g_scene_sampler;

[[vk::binding(2, 0)]]
Texture2DArray<float> g_shadow_debug_map;

[[vk::binding(3, 0)]]
SamplerState g_shadow_debug_sampler;

[[vk::binding(4, 0)]]
Texture2D<float> g_scene_depth_debug;

[[vk::binding(5, 0)]]
Texture2D<float> g_ao_raw_debug;

[[vk::binding(6, 0)]]
Texture2D<float> g_ao_blurred_debug;

[[vk::binding(7, 0)]]
SamplerState g_ao_debug_sampler;

[[vk::binding(8, 0)]]
Texture2DArray<float> g_point_shadow_debug_map;

[[vk::binding(9, 0)]]
SamplerState g_point_shadow_debug_sampler;

static const uint TONE_MAPPING_NONE = 0u;
static const uint TONE_MAPPING_ACES = 1u;
static const uint POST_PROCESS_FLAG_MANUAL_GAMMA = 1u << 0u;
static const uint EFFECT_DEBUG_FINAL_LIT = 0u;
static const uint EFFECT_DEBUG_HDR_SCENE_COLOR = 1u;
static const uint EFFECT_DEBUG_BLOOM_COMPOSITE = 2u;
static const uint EFFECT_DEBUG_BLOOM_DOWNSAMPLE_MIP = 3u;
static const uint EFFECT_DEBUG_BLOOM_UPSAMPLE_MIP = 4u;
static const uint EFFECT_DEBUG_SHADOW_MAP = 5u;
static const uint EFFECT_DEBUG_SHADOW_CASCADES = 6u;
static const uint EFFECT_DEBUG_SCENE_DEPTH = 7u;
static const uint EFFECT_DEBUG_AO_RAW = 8u;
static const uint EFFECT_DEBUG_AO_BLURRED = 9u;
static const uint EFFECT_DEBUG_POINT_SHADOW_MAP = 10u;

struct PostProcessPushConstants {
    float exposure;
    float output_gamma;
    uint tone_mapping_mode;
    uint flags;
    uint effect_debug_view;
    uint effect_debug_index;
    uint shadow_layer_count;
    uint _padding0;
    float ao_intensity;
    float ao_power;
    uint ao_enabled;
    uint _padding1;
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

float3 encodeOutputColor(float3 color) {
    if ((g_post_process.flags & POST_PROCESS_FLAG_MANUAL_GAMMA) != 0u) {
        color = linearToSrgb(color);
    }

    return saturate(color);
}

float4 sampleShadowMapDebug(FullscreenVSOutput input) {
    const uint layer_count = max(g_post_process.shadow_layer_count, 1u);
    const uint layer = min(g_post_process.effect_debug_index, layer_count - 1u);
    const float depth = g_shadow_debug_map.SampleLevel(
        g_shadow_debug_sampler,
        float3(input.uv, (float)layer),
        0.0f);
    const float visual_depth = saturate(depth);
    return float4(encodeOutputColor(float3(visual_depth, visual_depth, visual_depth)), 1.0f);
}

float4 samplePointShadowMapDebug(FullscreenVSOutput input) {
    const uint layer_count = max(g_post_process.shadow_layer_count, 1u);
    const uint layer = min(g_post_process.effect_debug_index, layer_count - 1u);
    const float depth = g_point_shadow_debug_map.SampleLevel(
        g_point_shadow_debug_sampler,
        float3(input.uv, (float)layer),
        0.0f);
    const float visual_depth = saturate(depth);
    return float4(encodeOutputColor(float3(visual_depth, visual_depth, visual_depth)), 1.0f);
}

float4 sampleSceneDepthDebug(FullscreenVSOutput input) {
    const float depth = g_scene_depth_debug.SampleLevel(g_ao_debug_sampler, input.uv, 0.0f);
    const float visual_depth = saturate(depth);
    return float4(encodeOutputColor(float3(visual_depth, visual_depth, visual_depth)), 1.0f);
}

float4 sampleAoDebugValue(float ao) {
    ao = saturate(ao);
    return float4(encodeOutputColor(float3(ao, ao, ao)), 1.0f);
}

float3 mapHdrDebugColor(float3 color) {
    color = applyExposure(max(color, 0.0f));
    return color / (color + float3(1.0f, 1.0f, 1.0f));
}

float applyScreenSpaceAo(float2 uv) {
    if (g_post_process.ao_enabled == 0u || g_post_process.ao_intensity <= 0.0f) {
        return 1.0f;
    }

    const float depth = g_scene_depth_debug.SampleLevel(g_ao_debug_sampler, uv, 0.0f);
    if (depth >= 0.99999f) {
        return 1.0f;
    }

    const float ao = saturate(g_ao_blurred_debug.SampleLevel(g_ao_debug_sampler, uv, 0.0f));
    const float powered_ao = pow(ao, max(g_post_process.ao_power, 0.01f));
    return lerp(1.0f, powered_ao, saturate(g_post_process.ao_intensity));
}

float4 PSMain(FullscreenVSOutput input) : SV_Target0 {
    if (g_post_process.effect_debug_view == EFFECT_DEBUG_SHADOW_MAP) {
        return sampleShadowMapDebug(input);
    }
    if (g_post_process.effect_debug_view == EFFECT_DEBUG_POINT_SHADOW_MAP) {
        return samplePointShadowMapDebug(input);
    }
    if (g_post_process.effect_debug_view == EFFECT_DEBUG_SCENE_DEPTH) {
        return sampleSceneDepthDebug(input);
    }
    if (g_post_process.effect_debug_view == EFFECT_DEBUG_AO_RAW) {
        return sampleAoDebugValue(g_ao_raw_debug.SampleLevel(g_ao_debug_sampler, input.uv, 0.0f));
    }
    if (g_post_process.effect_debug_view == EFFECT_DEBUG_AO_BLURRED) {
        return sampleAoDebugValue(g_ao_blurred_debug.SampleLevel(g_ao_debug_sampler, input.uv, 0.0f));
    }

    float4 hdr_color = g_hdr_scene_color.SampleLevel(g_scene_sampler, input.uv, 0.0f);
    float3 color = max(hdr_color.rgb, 0.0f);

    if (g_post_process.effect_debug_view != EFFECT_DEBUG_FINAL_LIT &&
        g_post_process.effect_debug_view != EFFECT_DEBUG_SHADOW_CASCADES) {
        return float4(encodeOutputColor(mapHdrDebugColor(color)), saturate(hdr_color.a));
    }

    color *= applyScreenSpaceAo(input.uv);
    color = applyExposure(color);
    if (g_post_process.tone_mapping_mode == TONE_MAPPING_ACES) {
        color = acesFitted(color);
    } else {
        color = saturate(color);
    }

    return float4(encodeOutputColor(color), saturate(hdr_color.a));
}
