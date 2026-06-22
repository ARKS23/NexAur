#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "Core/Base.h"
#include "Function/Renderer/RHI/buffer.h"
#include "Function/Renderer/RHI/framebuffer.h"
#include "Function/Renderer/RHI/material.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/texture.h"
#include "Function/Renderer/RHI/uniform_buffer.h"
#include "Function/Renderer/RHI/vertex_array.h"

namespace NexAur {
    // 后端设备抽象的最小版本。
    // 现阶段它先覆盖资源创建入口，后续 Vulkan 接入时可以从这里扩展命令队列、swapchain、descriptor 等能力。
    class NEXAUR_API RenderDevice {
    public:
        virtual ~RenderDevice() = default;

        virtual std::shared_ptr<Framebuffer> createFramebuffer(const FramebufferSpecification& spec) = 0;
        virtual std::shared_ptr<Material> createMaterial(const std::shared_ptr<Shader>& shader) = 0;

        virtual std::shared_ptr<Shader> createShader(const std::string& filepath) = 0;
        virtual std::shared_ptr<Shader> createShader(
            const std::string& name,
            const std::string& vertexSrc,
            const std::string& fragmentSrc) = 0;
        virtual std::shared_ptr<Shader> createShaderByPaths(
            const std::string& name,
            const std::string& vertexSrcPath,
            const std::string& fragmentSrcPath) = 0;

        virtual std::shared_ptr<TextureCubeMap> createTextureCube(const TextureSpecification& specification) = 0;
        virtual std::shared_ptr<TextureCubeMap> createTextureCube(const std::string& path) = 0;
        virtual std::shared_ptr<Texture2D> createTexture2D(const TextureSpecification& specification) = 0;
        virtual std::shared_ptr<Texture2D> createTexture2D(const std::string& path) = 0;
        virtual std::shared_ptr<Texture2D> createTexture2D(uint32_t renderer_id, uint32_t width, uint32_t height) = 0;

        virtual std::shared_ptr<VertexArray> createVertexArray() = 0;
        virtual std::shared_ptr<VertexBuffer> createVertexBuffer(uint32_t size) = 0;
        virtual std::shared_ptr<VertexBuffer> createVertexBuffer(float* vertices, uint32_t size) = 0;
        virtual std::shared_ptr<IndexBuffer> createIndexBuffer(uint32_t* indices, uint32_t count) = 0;
        virtual std::shared_ptr<UniformBuffer> createUniformBuffer(uint32_t size, uint32_t binding) = 0;
    };

    // 兼容旧代码的静态工厂门面。
    // 新实现会转发到当前 RendererModule 持有的 RenderDevice；旧调用点可以逐步迁移，不必一次性大改。
    class NEXAUR_API RendererFactory {
    public:
        static void setDevice(std::shared_ptr<RenderDevice> device);
        static std::shared_ptr<RenderDevice> getDevice();

        static std::shared_ptr<Framebuffer> createFramebuffer(const FramebufferSpecification& spec);
        static std::shared_ptr<Material> createMaterial(const std::shared_ptr<Shader>& shader);

        static std::shared_ptr<Shader> createShader(const std::string& filepath);
        static std::shared_ptr<Shader> createShader(
            const std::string& name,
            const std::string& vertexSrc,
            const std::string& fragmentSrc);
        static std::shared_ptr<Shader> createShaderByPaths(
            const std::string& name,
            const std::string& vertexSrcPath,
            const std::string& fragmentSrcPath);

        static std::shared_ptr<TextureCubeMap> createTextureCube(const TextureSpecification& specification);
        static std::shared_ptr<TextureCubeMap> createTextureCube(const std::string& path);
        static std::shared_ptr<Texture2D> createTexture2D(const TextureSpecification& specification);
        static std::shared_ptr<Texture2D> createTexture2D(const std::string& path);
        static std::shared_ptr<Texture2D> createTexture2D(uint32_t renderer_id, uint32_t width, uint32_t height);

        static std::shared_ptr<VertexArray> createVertexArray();
        static std::shared_ptr<VertexBuffer> createVertexBuffer(uint32_t size);
        static std::shared_ptr<VertexBuffer> createVertexBuffer(float* vertices, uint32_t size);
        static std::shared_ptr<IndexBuffer> createIndexBuffer(uint32_t* indices, uint32_t count);
        static std::shared_ptr<UniformBuffer> createUniformBuffer(uint32_t size, uint32_t binding);
    };
} // namespace NexAur
