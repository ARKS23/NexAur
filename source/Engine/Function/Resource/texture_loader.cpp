#include "pch.h"
#include "texture_loader.h"

#include <stb_image.h>

#include <cstring>
#include <utility>
#include <vector>

namespace NexAur {
    std::shared_ptr<TextureAsset> TextureLoader::load2D(const std::string& path, TextureColorSpace color_space) {
        if (path.empty()) {
            NX_CORE_WARN("Attempted to load texture from an empty path.");
            return nullptr;
        }

        int width = 0;
        int height = 0;
        int source_channels = 0;
        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &source_channels, STBI_rgb_alpha);
        if (!pixels) {
            NX_CORE_ERROR("Failed to load texture: {} ({})", path, stbi_failure_reason());
            return nullptr;
        }

        if (width <= 0 || height <= 0) {
            NX_CORE_ERROR("Texture has invalid dimensions: {}", path);
            stbi_image_free(pixels);
            return nullptr;
        }

        const size_t byte_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        std::vector<uint8_t> texture_pixels(byte_size);
        std::memcpy(texture_pixels.data(), pixels, byte_size);
        stbi_image_free(pixels);

        return std::make_shared<TextureAsset>(
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            TexturePixelFormat::RGBA8,
            color_space,
            std::move(texture_pixels),
            path);
    }

    std::shared_ptr<TextureAsset> TextureLoader::createSolidColorRGBA8(
        uint8_t r,
        uint8_t g,
        uint8_t b,
        uint8_t a,
        TextureColorSpace color_space,
        const std::string& debug_name) {
        std::vector<uint8_t> pixels{ r, g, b, a };
        return std::make_shared<TextureAsset>(
            1,
            1,
            TexturePixelFormat::RGBA8,
            color_space,
            std::move(pixels),
            debug_name);
    }
} // namespace NexAur
