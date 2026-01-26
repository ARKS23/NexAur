#include "pch.h"
#include "texture.h"
#include "Function/Renderer/Platform/OpenGL/opengl_texture.h"

namespace NexAur {
    std::shared_ptr<Texture2D> Texture2D::create(const TextureSpecification& specification) {
        return std::make_shared<OpenGLTexture2D>(specification);
    }

    std::shared_ptr<Texture2D> Texture2D::create(const std::string& path) {
        return std::make_shared<OpenGLTexture2D>(path);
    }

    std::shared_ptr<TextureCubeMap> TextureCubeMap::create(const TextureSpecification& specification) {
        return std::make_shared<OpenGLTextureCubeMap>(specification);
    }

    std::shared_ptr<TextureCubeMap> TextureCubeMap::create(const std::string& path) {
        return std::make_shared<OpenGLTextureCubeMap>(path);
    }
} // namespace NexAur