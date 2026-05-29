#include "pch.h"
#include "renderer_system.h"
#include "renderer.h"
#include "renderer_command.h"
#include "render_forward_pipeline.h"
#include "Function/Renderer/editor_camera.h"
#include "Function/Renderer/Resources/render_ibl_builder.h"
#include "Function/Renderer/Resources/render_resource_cache.h"
#include "Function/Resource/asset_manager.h"

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
        RenderResourceCache::getInstance().init();
        
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
        m_viewport_framebuffer.reset();
        m_forward_pipeline.reset();
        RenderResourceCache::getInstance().shutdown();

        // 关闭底层渲染器
        Renderer::shutdown();
    }

    void RendererSystem::tick(TimeStep ts, const RenderDataPacket& render_data) {
        // 先解析资源，再绑定本帧目标；环境光 IBL 首次烘焙会使用临时 FBO。
        ResolvedRenderDataPacket resolved_render_data = resolveRenderData(render_data);

        if (m_viewport_framebuffer) {
            m_viewport_framebuffer->bind();
            RendererCommand::setClearColor(glm::vec4{ 0.1f, 0.1f, 0.1f, 1.0f });
            RendererCommand::clear(ClearBufferFlag::ColorDepth);
            m_viewport_framebuffer->clearAttachment(1, -1); // ID附件清除为-1，表示没有选中任何物体
        }

        m_forward_pipeline->render(resolved_render_data);

        if (m_viewport_framebuffer) {
            m_viewport_framebuffer->unbind();
        }
    }

    ResolvedRenderDataPacket RendererSystem::resolveRenderData(const RenderDataPacket& render_data) {
        ResolvedRenderDataPacket resolved_data;
        resolved_data.camera_data = render_data.camera_data;
        resolved_data.directional_light_data = render_data.directional_light_data;
        resolved_data.point_lights_data = render_data.point_lights_data;
        resolved_data.environment_data.intensity = render_data.environment_data.intensity;

        AssetManager& asset_manager = AssetManager::getInstance();
        RenderResourceCache& resource_cache = RenderResourceCache::getInstance();

        if (render_data.environment_data.environment_asset) {
            std::shared_ptr<RenderEnvironmentMap> environment_map =
                resource_cache.getOrCreateEnvironmentMap(render_data.environment_data.environment_asset, asset_manager);
            if (environment_map) {
                resolved_data.environment_data.skybox_texture = environment_map->skybox_texture;
                resolved_data.environment_data.irradiance_map = environment_map->irradiance_map;
                resolved_data.environment_data.prefilter_map = environment_map->prefilter_map;
                resolved_data.environment_data.brdf_lut_map = environment_map->brdf_lut_map;
            }
        }

        auto resolve_objects = [&](const std::vector<RenderObjectData>& source_objects, std::vector<ResolvedRenderObjectData>& target_objects) {
            target_objects.reserve(source_objects.size());
            for (const RenderObjectData& object : source_objects) {
                if (!object.model_asset) {
                    continue;
                }

                std::shared_ptr<RenderModelData> gpu_model = resource_cache.getOrCreateModel(object.model_asset, asset_manager);
                if (!gpu_model) {
                    continue;
                }

                ResolvedRenderObjectData resolved_object;
                resolved_object.model_data = gpu_model;
                resolved_object.transform = object.transform;
                resolved_object.entity_id = object.entity_id;
                target_objects.push_back(resolved_object);
            }
        };

        resolve_objects(render_data.opaque_objects, resolved_data.opaque_objects);
        resolve_objects(render_data.transparent_objects, resolved_data.transparent_objects);

        return resolved_data;
    }

    void RendererSystem::onEvent(Event& e) {
        (void)e;
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
