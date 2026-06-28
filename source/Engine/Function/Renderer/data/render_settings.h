#pragma once

#include <cstdint>

namespace NexAur {
    enum class RenderToneMappingMode : uint32_t {
        None = 0,
        ACES = 1
    };

    struct RenderPostProcessSettings {
        RenderToneMappingMode tone_mapping_mode = RenderToneMappingMode::ACES;
        float exposure = 1.0f;
        bool bloom_enabled = true;
        float bloom_intensity = 0.08f;
        float bloom_scatter = 0.7f;
        float bloom_radius = 1.0f;
    };

    struct RenderSettings {
        RenderPostProcessSettings post_process;
    };
} // namespace NexAur
