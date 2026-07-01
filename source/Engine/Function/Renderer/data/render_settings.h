#pragma once

#include <cstdint>

namespace NexAur {
    inline constexpr uint32_t kMaxRenderPointShadowLights = 4;
    inline constexpr uint32_t kRenderPointShadowCubeFaceCount = 6;

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
        AoBlurred = 9,
        PointShadowMap = 10
    };

    enum class RenderLightingPreset : uint32_t {
        Outdoor = 0,
        Studio = 1,
        Cornell = 2,
        AssetPreview = 3,
        Custom = 4
    };

    inline const char* renderLightingPresetName(RenderLightingPreset preset) {
        switch (preset) {
        case RenderLightingPreset::Outdoor:
            return "Outdoor";
        case RenderLightingPreset::Studio:
            return "Studio";
        case RenderLightingPreset::Cornell:
            return "Cornell";
        case RenderLightingPreset::AssetPreview:
            return "Asset Preview";
        case RenderLightingPreset::Custom:
        default:
            return "Custom";
        }
    }

    struct RenderLightingCalibrationSettings {
        RenderLightingPreset preset = RenderLightingPreset::Outdoor;
        float directional_light_intensity_scale = 1.0f;
        float point_light_intensity_scale = 1.0f;
        float skybox_intensity_scale = 1.0f;
        float ibl_intensity_scale = 1.0f;
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
        uint32_t point_shadow_layer = 0;
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

    struct RenderPointShadowSettings {
        bool enabled = true;
        uint32_t max_shadowed_lights = 1;
        uint32_t map_resolution = 512;
        float strength = 0.85f;
        float constant_bias = 0.015f;
        float normal_bias = 0.02f;
        float filter_radius = 1.0f;
    };

    struct RenderContactShadowSettings {
        bool enabled = false;
        float intensity = 0.35f;
        float max_distance = 0.45f;
        float thickness = 0.08f;
    };

    struct RenderSettings {
        RenderLightingCalibrationSettings lighting;
        RenderPostProcessSettings post_process;
        RenderAoSettings ao;
        RenderIblDebugSettings ibl_debug;
        RenderEffectDebugSettings effects_debug;
        RenderShadowSettings shadow;
        RenderPointShadowSettings point_shadow;
        RenderContactShadowSettings contact_shadow;
    };

    inline void applyRenderLightingPreset(RenderSettings& settings, RenderLightingPreset preset) {
        settings.lighting.preset = preset;

        if (preset == RenderLightingPreset::Custom) {
            return;
        }

        settings.post_process.tone_mapping_mode = RenderToneMappingMode::ACES;
        settings.post_process.bloom_enabled = true;
        settings.post_process.bloom_scatter = 0.7f;
        settings.post_process.bloom_radius = 1.0f;

        switch (preset) {
        case RenderLightingPreset::Outdoor:
            settings.lighting.directional_light_intensity_scale = 1.0f;
            settings.lighting.point_light_intensity_scale = 1.0f;
            settings.lighting.skybox_intensity_scale = 1.0f;
            settings.lighting.ibl_intensity_scale = 1.0f;
            settings.post_process.exposure = 0.85f;
            settings.post_process.bloom_intensity = 0.05f;
            break;
        case RenderLightingPreset::Studio:
            settings.lighting.directional_light_intensity_scale = 0.35f;
            settings.lighting.point_light_intensity_scale = 1.0f;
            settings.lighting.skybox_intensity_scale = 0.45f;
            settings.lighting.ibl_intensity_scale = 0.85f;
            settings.post_process.exposure = 0.80f;
            settings.post_process.bloom_intensity = 0.035f;
            break;
        case RenderLightingPreset::Cornell:
            settings.lighting.directional_light_intensity_scale = 0.0f;
            settings.lighting.point_light_intensity_scale = 1.0f;
            settings.lighting.skybox_intensity_scale = 0.0f;
            settings.lighting.ibl_intensity_scale = 0.03f;
            settings.post_process.exposure = 1.0f;
            settings.post_process.bloom_intensity = 0.02f;
            break;
        case RenderLightingPreset::AssetPreview:
            settings.lighting.directional_light_intensity_scale = 0.25f;
            settings.lighting.point_light_intensity_scale = 1.0f;
            settings.lighting.skybox_intensity_scale = 0.65f;
            settings.lighting.ibl_intensity_scale = 1.0f;
            settings.post_process.exposure = 0.85f;
            settings.post_process.bloom_intensity = 0.03f;
            break;
        case RenderLightingPreset::Custom:
        default:
            break;
        }
    }
} // namespace NexAur
