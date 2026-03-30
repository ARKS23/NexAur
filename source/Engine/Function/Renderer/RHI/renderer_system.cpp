#include "pch.h"
#include "renderer_system.h"
#include "renderer.h"
#include "renderer_command.h"
#include "render_forward_pipeline.h"
#include "Function/Renderer/editor_camera.h"
#include "Function/Scene/scene.h"

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

    std::shared_ptr<Shader> RendererFactory::createShaderByPaths(const std::string& name, const std::string& vertexSrcPath, const std::string& fragmentSrcPath) {
        return Shader::createByPaths(name, vertexSrcPath, fragmentSrcPath);
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

    std::shared_ptr<Texture2D> RendererFactory::createTexture2D(uint32_t renderer_id, uint32_t width, uint32_t height) {
        return Texture2D::create(renderer_id, width, height);
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

        // 初始化RenderPipline
        m_forward_pipeline = std::make_shared<RenderForwardPipeline>();
        m_forward_pipeline->init();
        NX_CORE_ASSERT(m_forward_pipeline, "Failed to create RenderForwardPipeline in RendererSystem!");

        // Viewport Framebuffer 用于离屏渲染给编辑器窗口
        FramebufferSpecification fb_spec;
        fb_spec.width = m_viewport_width;
        fb_spec.height = m_viewport_height;
        fb_spec.Attachments = {
            FramebufferTextureFormat::RGBA8, 
            FramebufferTextureFormat::RED_INTEGER,     // 额外的ID附件，用于编辑器物体选中
            FramebufferTextureFormat::DEPTH24STENCIL8
        };
        
        m_viewport_framebuffer = RendererFactory::createFramebuffer(fb_spec);
        NX_CORE_ASSERT(m_viewport_framebuffer, "Failed to create viewport framebuffer in RendererSystem!");
    }

    void RendererSystem::shutdown() {
        // 关闭底层渲染器
        Renderer::shutdown();
    }

    void RendererSystem::tick(TimeStep ts, const RenderDataPacket& render_data) {
        if (m_viewport_framebuffer) {
            m_viewport_framebuffer->bind();
            RendererCommand::setClearColor(glm::vec4{ 0.1f, 0.1f, 0.1f, 1.0f });
            RendererCommand::clear(ClearBufferFlag::ColorDepth);
            m_viewport_framebuffer->clearAttachment(1, -1); // ID附件清除为-1，表示没有选中任何物体
        }

        // 场景渲染v2
        m_forward_pipeline->render(render_data);

        if (m_viewport_framebuffer) {
            m_viewport_framebuffer->unbind();
        }
    }

    void RendererSystem::onEvent(Event& e) {
        // m_editor_camera->onEvent(e);
        
        // 事件处理补充
    }

    void RendererSystem::setViewportSize(uint32_t width, uint32_t height) {
        width = std::max(1u, width);
        height = std::max(1u, height);

        if (width == m_viewport_width && height == m_viewport_height) {
            return; // 尺寸未改变，无需处理
        }

        m_viewport_width = width;
        m_viewport_height = height;

        if (m_viewport_framebuffer) {
            m_viewport_framebuffer->resize(width, height);
        }
    }

    void RendererSystem::onWindowResize(WindowResizeEvent& e) {
        Renderer::onWindowResize(e.getWidth(), e.getHeight());
    }
} // namespace NexAur
