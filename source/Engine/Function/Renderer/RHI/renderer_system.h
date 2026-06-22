#pragma once
#include "Core/Base.h"
#include "Core/Time/TimeStep.h"
#include "Core/Events/window_event.h"
#include "vertex_array.h"
#include "uniform_buffer.h"
#include "framebuffer.h"
#include "texture.h"
#include "material.h"
#include "buffer.h"
#include "renderer_service.h"
#include "Function/Renderer/data/render_data.h"

namespace NexAur {
    class RenderForwardPipeline;
    class EditorCamera;
} // namespace NexAur


namespace NexAur {
    // 渲染模块资源工厂
    class NEXAUR_API RendererFactory {
    public:
        // Framebuffer
        static std::shared_ptr<Framebuffer> createFramebuffer(const FramebufferSpecification& spec);

        // Material
        static std::shared_ptr<Material> createMaterial(const std::shared_ptr<Shader>& shader);

        // Shader
        static std::shared_ptr<Shader> createShader(const std::string& filepath);
        static std::shared_ptr<Shader> createShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
        static std::shared_ptr<Shader> createShaderByPaths(const std::string& name, const std::string& vertexSrcPath, const std::string& fragmentSrcPath);

        // TexutreCubeMap
        static std::shared_ptr<TextureCubeMap> createTextureCube(const TextureSpecification& specification);
		static std::shared_ptr<TextureCubeMap> createTextureCube(const std::string& path);

        // Texutre2D
        static std::shared_ptr<Texture2D> createTexture2D(const TextureSpecification& specification); 
		static std::shared_ptr<Texture2D> createTexture2D(const std::string& path);
        static std::shared_ptr<Texture2D> createTexture2D(uint32_t renderer_id, uint32_t width, uint32_t height);
        
        // VAO
        static std::shared_ptr<VertexArray> createVertexArray();

        // VBO
        static std::shared_ptr<VertexBuffer> createVertexBuffer(uint32_t size);
        static std::shared_ptr<VertexBuffer> createVertexBuffer(float* vertices, uint32_t size);

        // EBO
        static std::shared_ptr<IndexBuffer> createIndexBuffer(uint32_t* indices, uint32_t count);

        // Unifrom Buffer
        static std::shared_ptr<UniformBuffer> createUniformBuffer(uint32_t size, uint32_t binding);
    };

    
    // 渲染模块总线
    class NEXAUR_API RendererSystem : public RendererService {
    public:
        RendererSystem() = default;
        ~RendererSystem() = default;

        void init();
        void shutdown();

        void tick(TimeStep ts, const RenderDataPacket& render_data); // 每帧调用，传入渲染数据包
        void render(TimeStep ts, const RenderDataPacket& render_data) override { tick(ts, render_data); }
        void onEvent(Event& e);

        void setViewportSize(uint32_t width, uint32_t height) override;
        std::pair<uint32_t, uint32_t> getViewportSize() const override { return { m_viewport_width, m_viewport_height }; }
        uint32_t getViewportColorAttachment() const override { return m_viewport_framebuffer ? m_viewport_framebuffer->getColorAttachmentRendererID(0) : 0; }
        int readViewportEntityID(int x, int y) override;
        std::shared_ptr<Framebuffer> getViewportFramebuffer() const override { return m_viewport_framebuffer; }

    private:
        ResolvedRenderDataPacket resolveRenderData(const RenderDataPacket& render_data);
        bool onWindowResize(WindowResizeEvent& e);

    private:
        std::shared_ptr<RenderForwardPipeline> m_forward_pipeline;  // 前向渲染管线

        std::shared_ptr<Framebuffer> m_viewport_framebuffer;
        uint32_t m_viewport_width = 1280, m_viewport_height = 720;
    };
} // namespace NexAur
