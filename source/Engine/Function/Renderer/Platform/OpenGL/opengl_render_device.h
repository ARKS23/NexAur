#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/render_device.h"

namespace NexAur {
    class NEXAUR_API OpenGLRenderDevice final : public RenderDevice {
    public:
        std::shared_ptr<Framebuffer> createFramebuffer(const FramebufferSpecification& spec) override;
        std::shared_ptr<Material> createMaterial(const std::shared_ptr<Shader>& shader) override;

        std::shared_ptr<Shader> createShader(const std::string& filepath) override;
        std::shared_ptr<Shader> createShader(
            const std::string& name,
            const std::string& vertexSrc,
            const std::string& fragmentSrc) override;
        std::shared_ptr<Shader> createShaderByPaths(
            const std::string& name,
            const std::string& vertexSrcPath,
            const std::string& fragmentSrcPath) override;

        std::shared_ptr<TextureCubeMap> createTextureCube(const TextureSpecification& specification) override;
        std::shared_ptr<TextureCubeMap> createTextureCube(const std::string& path) override;
        std::shared_ptr<Texture2D> createTexture2D(const TextureSpecification& specification) override;
        std::shared_ptr<Texture2D> createTexture2D(const std::string& path) override;
        std::shared_ptr<Texture2D> createTexture2D(uint32_t renderer_id, uint32_t width, uint32_t height) override;

        std::shared_ptr<VertexArray> createVertexArray() override;
        std::shared_ptr<VertexBuffer> createVertexBuffer(uint32_t size) override;
        std::shared_ptr<VertexBuffer> createVertexBuffer(float* vertices, uint32_t size) override;
        std::shared_ptr<IndexBuffer> createIndexBuffer(uint32_t* indices, uint32_t count) override;
        std::shared_ptr<UniformBuffer> createUniformBuffer(uint32_t size, uint32_t binding) override;
    };
} // namespace NexAur
