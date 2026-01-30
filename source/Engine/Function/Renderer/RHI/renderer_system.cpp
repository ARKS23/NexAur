#include "pch.h"
#include "renderer_system.h"
#include "renderer.h"
#include "renderer_command.h"
#include "render_forward_pipeline.h"
#include "Function/Renderer/editor_camera.h"

// 工厂函数实现
namespace NexAur {
    std::shared_ptr<Framebuffer> RendererFactory::createFramebuffer(const FramebufferSpecification& spec) {
        return Framebuffer::create(spec);
    }

    std::shared_ptr<Material> RendererFactory::createMaterial(const std::shared_ptr<Shader>& shader) {
        return std::make_shared<Material>(shader);
    }

    std::shared_ptr<Shader> RendererFactory::createShader(const std::string& filepath) {
        return Shader::create(filepath);
    }

    std::shared_ptr<Shader> RendererFactory::createShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) {
        return Shader::create(name, vertexSrc, fragmentSrc);
    }

    std::shared_ptr<TextureCubeMap> RendererFactory::createTextureCube(const TextureSpecification& specification) {
        return TextureCubeMap::create(specification);
    }

    std::shared_ptr<TextureCubeMap> RendererFactory::createTextureCube(const std::string& path) {
        return TextureCubeMap::create(path);
    }

    std::shared_ptr<Texture2D> RendererFactory::createTexture2D(const TextureSpecification& specification) {
        return Texture2D::create(specification);
    }

    std::shared_ptr<Texture2D> RendererFactory::createTexture2D(const std::string& path) {
        return Texture2D::create(path);
    }

    std::shared_ptr<VertexArray> RendererFactory::createVertexArray() {
        return VertexArray::create();
    }

    std::shared_ptr<VertexBuffer> RendererFactory::createVertexBuffer(uint32_t size) {
        return VertexBuffer::create(size);
    }

    std::shared_ptr<VertexBuffer> RendererFactory::createVertexBuffer(float* vertices, uint32_t size) {
        return VertexBuffer::create(vertices, size);
    }

    std::shared_ptr<IndexBuffer> RendererFactory::createIndexBuffer(uint32_t* indices, uint32_t count) {
        return IndexBuffer::create(indices, count);
    }

    std::shared_ptr<UniformBuffer> RendererFactory::createUniformBuffer(uint32_t size, uint32_t binding) {
        return UniformBuffer::create(size, binding);
    }
} // namespace NexAur


namespace NexAur {
    void RendererSystem::init() {
        // 初始化底层渲染器
        Renderer::init();
        
        RendererCommand::setClearColor(glm::vec4{ 0.1f, 0.1f, 0.1f, 1.0f });

        // 初始化摄像机
        m_editor_camera = std::make_shared<EditorCamera>(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);

        // 初始化RenderPipline
        m_forward_pipeline = std::make_shared<RenderForwardPipeline>();
        m_forward_pipeline->init();
        NX_CORE_ASSERT(m_forward_pipeline, "Failed to create RenderForwardPipeline in RendererSystem!");
    }

    void RendererSystem::shutdown() {
        // 关闭底层渲染器
        Renderer::shutdown();
    }

    void RendererSystem::tick(TimeStep ts) {
        // 更新摄像机
        m_editor_camera->onUpdate(ts);

        // 每帧更新渲染逻辑
        m_forward_pipeline->render(m_editor_camera);
    }

    void RendererSystem::onEvent(Event& e) {
        m_editor_camera->onEvent(e);

        // 事件处理补充
    }

    void RendererSystem::onWindowResize(WindowResizeEvent& e) {
        Renderer::onWindowResize(e.getWidth(), e.getHeight());
    }
} // namespace NexAur
