#pragma once

#include <cstdint>

namespace NexAur {
    enum class RenderToneMappingMode : uint32_t {
        None = 0,
        ACES = 1
    };

    enum class RenderIblDebugMode : uint32_t {
        FinalLit = 0,
        DiffuseIbl = 1,
        SpecularIbl = 2,
        CombinedIbl = 3,
        Normal = 4,
        Metallic = 5,
        Roughness = 6,
        AmbientOcclusion = 7,
        Emissive = 8,
        Irradiance = 9,
        PrefilteredEnvironment = 10,
        BrdfLut = 11
    };

    enum class RenderShadowFilterMode : uint32_t {
        Hard = 0,
        PCF3x3 = 1,
        PCF5x5 = 2,
        PoissonPCF = 3,
        PCSS = 4
    };

    enum class RenderEffectDebugView : uint32_t {
        FinalLit = 0,
        HdrSceneColor = 1,
        BloomComposite = 2,
        BloomDownsampleMip = 3,
        BloomUpsampleMip = 4,
        ShadowMap = 5,
        ShadowCascades = 6,
        SceneDepth = 7,
        AoRaw = 8,
        AoBlurred = 9
    };

    struct RenderAoSettings {
        bool enabled = true;
        float radius = 1.2f;
        float intensity = 0.6f;
        float bias = 0.025f;
        float power = 1.2f;
        bool blur_enabled = true;
        bool half_resolution = true;
    };

    struct RenderPostProcessSettings {
        RenderToneMappingMode tone_mapping_mode = RenderToneMappingMode::ACES;
        float exposure = 0.85f;
        bool bloom_enabled = true;
        float bloom_intensity = 0.05f;
        float bloom_scatter = 0.7f;
        float bloom_radius = 1.0f;
    };

    struct RenderIblDebugSettings {
        RenderIblDebugMode mode = RenderIblDebugMode::FinalLit;
        float prefilter_mip = 0.0f;
    };

    struct RenderEffectDebugSettings {
        RenderEffectDebugView view = RenderEffectDebugView::FinalLit;
        uint32_t bloom_mip = 0;
        uint32_t shadow_cascade = 0;
    };

    struct RenderShadowSettings {
        bool enabled = true;
        RenderShadowFilterMode filter_mode = RenderShadowFilterMode::PCF3x3;
        float strength = 0.7f;
        float constant_bias = 0.003f;
        float normal_bias = 0.0f;
        float slope_bias = 0.001f;
        float filter_radius = 1.0f;
        float pcss_light_radius = 0.5f;
        float pcss_search_radius = 3.0f;
        float pcss_min_filter_radius = 0.75f;
        float pcss_max_filter_radius = 6.0f;
        float distance = 35.0f;
        uint32_t map_resolution = 2048;
        bool stabilize = true;
        bool cascades_enabled = true;
        uint32_t cascade_count = 4;
        float cascade_split_lambda = 0.65f;
        bool cascade_debug_overlay = false;
    };

    struct RenderSettings {
        RenderPostProcessSettings post_process;
        RenderAoSettings ao;
        RenderIblDebugSettings ibl_debug;
        RenderEffectDebugSettings effects_debug;
        RenderShadowSettings shadow;
    };
} // namespace NexAur
