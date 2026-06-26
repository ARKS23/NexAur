#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/Base.h"
#include "Function/Resource/texture_types.h"

namespace NexAur {
    class NEXAUR_API TextureAsset {
    public:
        TextureAsset() = default;
        TextureAsset(
            uint32_t width,
            uint32_t height,
            TexturePixelFormat format,
            TextureColorSpace color_space,
            std::vector<uint8_t> pixels,
            std::string source_path);

        bool isLoaded() const;
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }
        uint32_t getChannelCount() const { return 4; }
        TexturePixelFormat getFormat() const { return m_format; }
        TextureColorSpace getColorSpace() const { return m_color_space; }
        const std::vector<uint8_t>& getPixels() const { return m_pixels; }
        const std::string& getSourcePath() const { return m_source_path; }
        size_t getByteSize() const { return m_pixels.size(); }

    private:
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        TexturePixelFormat m_format = TexturePixelFormat::RGBA8;
        TextureColorSpace m_color_space = TextureColorSpace::SRGB;
        std::vector<uint8_t> m_pixels;
        std::string m_source_path;
    };
} // namespace NexAur
