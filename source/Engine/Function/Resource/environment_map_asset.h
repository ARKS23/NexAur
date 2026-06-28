#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/Base.h"

namespace NexAur {
    class NEXAUR_API EnvironmentMapAsset {
    public:
        EnvironmentMapAsset() = default;
        EnvironmentMapAsset(
            uint32_t width,
            uint32_t height,
            std::vector<float> pixels,
            std::string source_path);

        bool isLoaded() const;
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }
        uint32_t getChannelCount() const { return 4; }
        const std::vector<float>& getPixels() const { return m_pixels; }
        const std::string& getSourcePath() const { return m_source_path; }
        size_t getByteSize() const { return m_pixels.size() * sizeof(float); }

    private:
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        std::vector<float> m_pixels;
        std::string m_source_path;
    };
} // namespace NexAur
