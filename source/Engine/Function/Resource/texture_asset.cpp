#include "pch.h"
#include "texture_asset.h"

#include <utility>

namespace NexAur {
    TextureAsset::TextureAsset(
        uint32_t width,
        uint32_t height,
        TexturePixelFormat format,
        TextureColorSpace color_space,
        std::vector<uint8_t> pixels,
        std::string source_path)
        : m_width(width)
        , m_height(height)
        , m_format(format)
        , m_color_space(color_space)
        , m_pixels(std::move(pixels))
        , m_source_path(std::move(source_path)) {}

    bool TextureAsset::isLoaded() const {
        return m_width > 0 && m_height > 0 && !m_pixels.empty();
    }
} // namespace NexAur
