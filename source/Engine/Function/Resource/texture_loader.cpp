#include "pch.h"
#include "texture_loader.h"

#include "Function/Resource/environment_map_asset.h"

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
        std::vector<float> resampleRGBA32F(
            const float* source_pixels,
            uint32_t source_width,
            uint32_t source_height,
            uint32_t target_width,
            uint32_t target_height) {
            std::vector<float> target_pixels(
                static_cast<size_t>(target_width) *
                static_cast<size_t>(target_height) *
                4u);

            for (uint32_t y = 0; y < target_height; ++y) {
                const float source_y = (static_cast<float>(y) + 0.5f) *
                    static_cast<float>(source_height) /
                    static_cast<float>(target_height) -
                    0.5f;
                const uint32_t y0 = static_cast<uint32_t>(std::clamp(
                    static_cast<int>(std::floor(source_y)),
                    0,
                    static_cast<int>(source_height - 1)));
                const uint32_t y1 = std::min(y0 + 1, source_height - 1);
                const float fy = std::clamp(source_y - static_cast<float>(y0), 0.0f, 1.0f);

                for (uint32_t x = 0; x < target_width; ++x) {
                    const float source_x = (static_cast<float>(x) + 0.5f) *
                        static_cast<float>(source_width) /
                        static_cast<float>(target_width) -
                        0.5f;
                    const uint32_t x0 = static_cast<uint32_t>(std::clamp(
                        static_cast<int>(std::floor(source_x)),
                        0,
                        static_cast<int>(source_width - 1)));
                    const uint32_t x1 = std::min(x0 + 1, source_width - 1);
                    const float fx = std::clamp(source_x - static_cast<float>(x0), 0.0f, 1.0f);

                    const size_t target_index =
                        (static_cast<size_t>(y) * target_width + x) * 4u;
                    const size_t source00 =
                        (static_cast<size_t>(y0) * source_width + x0) * 4u;
                    const size_t source10 =
                        (static_cast<size_t>(y0) * source_width + x1) * 4u;
                    const size_t source01 =
                        (static_cast<size_t>(y1) * source_width + x0) * 4u;
                    const size_t source11 =
                        (static_cast<size_t>(y1) * source_width + x1) * 4u;

                    for (uint32_t channel = 0; channel < 4; ++channel) {
                        const float top = source_pixels[source00 + channel] * (1.0f - fx) +
                            source_pixels[source10 + channel] * fx;
                        const float bottom = source_pixels[source01 + channel] * (1.0f - fx) +
                            source_pixels[source11 + channel] * fx;
                        target_pixels[target_index + channel] = top * (1.0f - fy) + bottom * fy;
                    }
                }
            }

            return target_pixels;
        }
    } // namespace

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

    std::shared_ptr<EnvironmentMapAsset> TextureLoader::loadHDREnvironment(
        const std::string& path,
        uint32_t max_width) {
        if (path.empty()) {
            NX_CORE_WARN("Attempted to load HDR environment from an empty path.");
            return nullptr;
        }

        int width = 0;
        int height = 0;
        int source_channels = 0;
        float* pixels = stbi_loadf(path.c_str(), &width, &height, &source_channels, STBI_rgb_alpha);
        if (!pixels) {
            NX_CORE_ERROR("Failed to load HDR environment: {} ({})", path, stbi_failure_reason());
            return nullptr;
        }

        if (width <= 0 || height <= 0) {
            NX_CORE_ERROR("HDR environment has invalid dimensions: {}", path);
            stbi_image_free(pixels);
            return nullptr;
        }

        const uint32_t source_width = static_cast<uint32_t>(width);
        const uint32_t source_height = static_cast<uint32_t>(height);
        uint32_t target_width = source_width;
        uint32_t target_height = source_height;
        if (max_width > 0 && source_width > max_width) {
            target_width = max_width;
            target_height = std::max(
                1u,
                static_cast<uint32_t>(
                    std::round(static_cast<float>(source_height) *
                               static_cast<float>(target_width) /
                               static_cast<float>(source_width))));
        }

        std::vector<float> environment_pixels = resampleRGBA32F(
            pixels,
            source_width,
            source_height,
            target_width,
            target_height);
        stbi_image_free(pixels);

        return std::make_shared<EnvironmentMapAsset>(
            target_width,
            target_height,
            std::move(environment_pixels),
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
