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
        PCF5x5 = 2
    };

    struct RenderPostProcessSettings {
        RenderToneMappingMode tone_mapping_mode = RenderToneMappingMode::ACES;
        float exposure = 1.0f;
        bool bloom_enabled = true;
        float bloom_intensity = 0.08f;
        float bloom_scatter = 0.7f;
        float bloom_radius = 1.0f;
    };

    struct RenderIblDebugSettings {
        RenderIblDebugMode mode = RenderIblDebugMode::FinalLit;
        float prefilter_mip = 0.0f;
    };

    struct RenderShadowSettings {
        bool enabled = true;
        RenderShadowFilterMode filter_mode = RenderShadowFilterMode::PCF3x3;
        float strength = 0.7f;
        float constant_bias = 0.003f;
        float normal_bias = 0.0f;
        float slope_bias = 0.001f;
        float filter_radius = 1.0f;
        float distance = 35.0f;
        uint32_t map_resolution = 2048;
        bool stabilize = true;
    };

    struct RenderSettings {
        RenderPostProcessSettings post_process;
        RenderIblDebugSettings ibl_debug;
        RenderShadowSettings shadow;
    };
} // namespace NexAur
