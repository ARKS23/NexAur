#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "Core/Base.h"
#include "Function/Resource/texture_asset.h"

namespace NexAur {
    class NEXAUR_API TextureLoader {
    public:
        static std::shared_ptr<TextureAsset> load2D(
            const std::string& path,
            TextureColorSpace color_space = TextureColorSpace::SRGB);

        static std::shared_ptr<TextureAsset> createSolidColorRGBA8(
            uint8_t r,
            uint8_t g,
            uint8_t b,
            uint8_t a,
            TextureColorSpace color_space = TextureColorSpace::SRGB,
            const std::string& debug_name = "RuntimeTexture");
    };
} // namespace NexAur
