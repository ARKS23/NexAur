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

    struct RenderSettings {
        RenderPostProcessSettings post_process;
        RenderIblDebugSettings ibl_debug;
    };
} // namespace NexAur
