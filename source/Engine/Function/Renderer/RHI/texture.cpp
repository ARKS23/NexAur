#include "pch.h"
#include "texture.h"

namespace NexAur {
    std::shared_ptr<Texture2D> Texture2D::create(const TextureSpecification& specification) {
        return nullptr;
    }

    std::shared_ptr<Texture2D> Texture2D::create(const std::string& path) {
        return nullptr;
    }

    std::shared_ptr<TextureCubeMap> TextureCubeMap::create(const TextureSpecification& specification) {
        return nullptr;
    }

    std::shared_ptr<TextureCubeMap> TextureCubeMap::create(const std::string& path) {
        return nullptr;
    }
} // namespace NexAur