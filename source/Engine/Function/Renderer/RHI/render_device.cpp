#include "pch.h"
#include "render_device.h"

#include "Core/Log/log_system.h"

namespace NexAur {
    namespace {
        std::shared_ptr<RenderDevice>& activeRenderDevice() {
            static std::shared_ptr<RenderDevice> device;
            return device;
        }

        RenderDevice& requireRenderDevice() {
            NX_CORE_ASSERT(activeRenderDevice(), "RendererFactory used before RenderDevice was set.");
            return *activeRenderDevice();
        }
    } // namespace

    void RendererFactory::setDevice(std::shared_ptr<RenderDevice> device) {
        activeRenderDevice() = std::move(device);
    }

    std::shared_ptr<RenderDevice> RendererFactory::getDevice() {
        return activeRenderDevice();
    }

    std::shared_ptr<Framebuffer> RendererFactory::createFramebuffer(const FramebufferSpecification& spec) {
        return requireRenderDevice().createFramebuffer(spec);
    }

    std::shared_ptr<Material> RendererFactory::createMaterial(const std::shared_ptr<Shader>& shader) {
        return requireRenderDevice().createMaterial(shader);
    }

    std::shared_ptr<Shader> RendererFactory::createShader(const std::string& filepath) {
        return requireRenderDevice().createShader(filepath);
    }

    std::shared_ptr<Shader> RendererFactory::createShader(
        const std::string& name,
        const std::string& vertexSrc,
        const std::string& fragmentSrc) {
        return requireRenderDevice().createShader(name, vertexSrc, fragmentSrc);
    }

    std::shared_ptr<Shader> RendererFactory::createShaderByPaths(
        const std::string& name,
        const std::string& vertexSrcPath,
        const std::string& fragmentSrcPath) {
        return requireRenderDevice().createShaderByPaths(name, vertexSrcPath, fragmentSrcPath);
    }

    std::shared_ptr<TextureCubeMap> RendererFactory::createTextureCube(const TextureSpecification& specification) {
        return requireRenderDevice().createTextureCube(specification);
    }

    std::shared_ptr<TextureCubeMap> RendererFactory::createTextureCube(const std::string& path) {
        return requireRenderDevice().createTextureCube(path);
    }

    std::shared_ptr<Texture2D> RendererFactory::createTexture2D(const TextureSpecification& specification) {
        return requireRenderDevice().createTexture2D(specification);
    }

    std::shared_ptr<Texture2D> RendererFactory::createTexture2D(const std::string& path) {
        return requireRenderDevice().createTexture2D(path);
    }

    std::shared_ptr<Texture2D> RendererFactory::createTexture2D(uint32_t renderer_id, uint32_t width, uint32_t height) {
        return requireRenderDevice().createTexture2D(renderer_id, width, height);
    }

    std::shared_ptr<VertexArray> RendererFactory::createVertexArray() {
        return requireRenderDevice().createVertexArray();
    }

    std::shared_ptr<VertexBuffer> RendererFactory::createVertexBuffer(uint32_t size) {
        return requireRenderDevice().createVertexBuffer(size);
    }

    std::shared_ptr<VertexBuffer> RendererFactory::createVertexBuffer(float* vertices, uint32_t size) {
        return requireRenderDevice().createVertexBuffer(vertices, size);
    }

    std::shared_ptr<IndexBuffer> RendererFactory::createIndexBuffer(uint32_t* indices, uint32_t count) {
        return requireRenderDevice().createIndexBuffer(indices, count);
    }

    std::shared_ptr<UniformBuffer> RendererFactory::createUniformBuffer(uint32_t size, uint32_t binding) {
        return requireRenderDevice().createUniformBuffer(size, binding);
    }
} // namespace NexAur
