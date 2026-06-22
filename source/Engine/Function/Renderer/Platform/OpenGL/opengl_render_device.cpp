#include "pch.h"
#include "opengl_render_device.h"

namespace NexAur {
    std::shared_ptr<Framebuffer> OpenGLRenderDevice::createFramebuffer(const FramebufferSpecification& spec) {
        return Framebuffer::create(spec);
    }

    std::shared_ptr<Material> OpenGLRenderDevice::createMaterial(const std::shared_ptr<Shader>& shader) {
        return std::make_shared<Material>(shader);
    }

    std::shared_ptr<Shader> OpenGLRenderDevice::createShader(const std::string& filepath) {
        return Shader::create(filepath);
    }

    std::shared_ptr<Shader> OpenGLRenderDevice::createShader(
        const std::string& name,
        const std::string& vertexSrc,
        const std::string& fragmentSrc) {
        return Shader::create(name, vertexSrc, fragmentSrc);
    }

    std::shared_ptr<Shader> OpenGLRenderDevice::createShaderByPaths(
        const std::string& name,
        const std::string& vertexSrcPath,
        const std::string& fragmentSrcPath) {
        return Shader::createByPaths(name, vertexSrcPath, fragmentSrcPath);
    }

    std::shared_ptr<TextureCubeMap> OpenGLRenderDevice::createTextureCube(const TextureSpecification& specification) {
        return TextureCubeMap::create(specification);
    }

    std::shared_ptr<TextureCubeMap> OpenGLRenderDevice::createTextureCube(const std::string& path) {
        return TextureCubeMap::create(path);
    }

    std::shared_ptr<Texture2D> OpenGLRenderDevice::createTexture2D(const TextureSpecification& specification) {
        return Texture2D::create(specification);
    }

    std::shared_ptr<Texture2D> OpenGLRenderDevice::createTexture2D(const std::string& path) {
        return Texture2D::create(path);
    }

    std::shared_ptr<Texture2D> OpenGLRenderDevice::createTexture2D(uint32_t renderer_id, uint32_t width, uint32_t height) {
        return Texture2D::create(renderer_id, width, height);
    }

    std::shared_ptr<VertexArray> OpenGLRenderDevice::createVertexArray() {
        return VertexArray::create();
    }

    std::shared_ptr<VertexBuffer> OpenGLRenderDevice::createVertexBuffer(uint32_t size) {
        return VertexBuffer::create(size);
    }

    std::shared_ptr<VertexBuffer> OpenGLRenderDevice::createVertexBuffer(float* vertices, uint32_t size) {
        return VertexBuffer::create(vertices, size);
    }

    std::shared_ptr<IndexBuffer> OpenGLRenderDevice::createIndexBuffer(uint32_t* indices, uint32_t count) {
        return IndexBuffer::create(indices, count);
    }

    std::shared_ptr<UniformBuffer> OpenGLRenderDevice::createUniformBuffer(uint32_t size, uint32_t binding) {
        return UniformBuffer::create(size, binding);
    }
} // namespace NexAur
