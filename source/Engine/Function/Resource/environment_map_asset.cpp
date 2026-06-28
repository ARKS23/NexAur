#include "pch.h"
#include "environment_map_asset.h"

#include <utility>

namespace NexAur {
    EnvironmentMapAsset::EnvironmentMapAsset(
        uint32_t width,
        uint32_t height,
        std::vector<float> pixels,
        std::string source_path)
        : m_width(width)
        , m_height(height)
        , m_pixels(std::move(pixels))
        , m_source_path(std::move(source_path)) {}

    bool EnvironmentMapAsset::isLoaded() const {
        return m_width > 0 &&
               m_height > 0 &&
               m_pixels.size() == static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4u;
    }
} // namespace NexAur
