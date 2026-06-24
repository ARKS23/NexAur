#include "pch.h"
#include "renderer_system.h"

#include "Function/Renderer/Platform/OpenGL/opengl_render_device.h"
#include "Function/Renderer/RHI/render_forward_pipeline.h"
#include "Function/Renderer/RHI/renderer.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Renderer/Resources/render_ibl_builder.h"
#include "Function/Renderer/Resources/render_resource_cache.h"
#include "Function/Resource/asset_manager.h"

#include <algorithm>

namespace NexAur {
    void RendererSystem::init() {
        // 当前后端仍然是 OpenGL；RenderDevice 把创建资源的入口先收口，给后续 Vulkan 留替换点。
        m_render_device = std::make_shared<OpenGLRenderDevice>();
        RendererFactory::setDevice(m_render_device);

        Renderer::init();
        RenderResourceCache::getInstance().init();

        RendererCommand::setClearColor(glm::vec4{ 0.1f, 0.1f, 0.1f, 1.0f });

        m_forward_pipeline = std::make_shared<RenderForwardPipeline>();
        m_forward_pipeline->init();
        NX_CORE_ASSERT(m_forward_pipeline, "Failed to create RenderForwardPipeline in RendererSystem!");

        // Viewport Framebuffer 用于离屏渲染，再交给编辑器 viewport 显示。
        FramebufferSpecification fb_spec;
        fb_spec.width = m_viewport_width;
        fb_spec.height = m_viewport_height;
        fb_spec.Attachments = {
            FramebufferTextureFormat::RGBA8,
            FramebufferTextureFormat::RED_INTEGER,
            FramebufferTextureFormat::DEPTH24STENCIL8
        };

        m_viewport_framebuffer = RendererFactory::createFramebuffer(fb_spec);
        NX_CORE_ASSERT(m_viewport_framebuffer, "Failed to create viewport framebuffer in RendererSystem!");
    }

    void RendererSystem::shutdown() {
        m_viewport_framebuffer.reset();
        m_forward_pipeline.reset();
        RenderResourceCache::getInstance().shutdown();

        Renderer::shutdown();
        RendererFactory::setDevice(nullptr);
        m_render_device.reset();
    }

    void RendererSystem::tick(TimeStep ts, const RenderDataPacket& render_data) {
        (void)ts;

        // 先把跨模块的轻量 RenderDataPacket 解析成 Renderer 内部可直接绘制的资源。
        ResolvedRenderDataPacket resolved_render_data = resolveRenderData(render_data);
        RenderPassContext pass_context = createRenderPassContext();

        if (m_viewport_framebuffer) {
            m_viewport_framebuffer->bind();
            RendererCommand::setViewport(0, 0, pass_context.viewport_width, pass_context.viewport_height);
            RendererCommand::setClearColor(glm::vec4{ 0.1f, 0.1f, 0.1f, 1.0f });
            RendererCommand::clear(ClearBufferFlag::ColorDepth);
            m_viewport_framebuffer->clearAttachment(1, -1);
        }

        m_forward_pipeline->render(pass_context, resolved_render_data);

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

    RenderPassContext RendererSystem::createRenderPassContext() const {
        RenderPassContext context;
        context.viewport_framebuffer = m_viewport_framebuffer;
        context.viewport_width = std::max(1u, m_viewport_width);
        context.viewport_height = std::max(1u, m_viewport_height);

        if (m_viewport_framebuffer) {
            const FramebufferSpecification& spec = m_viewport_framebuffer->getSpecification();
            context.viewport_width = std::max(1u, spec.width);
            context.viewport_height = std::max(1u, spec.height);
        }

        return context;
    }

    void RendererSystem::onEvent(Event& e) {
        EventDispatcher dispatcher(e);
        dispatcher.dispatch<WindowResizeEvent>(NX_BIND_EVENT_FN(RendererSystem::onWindowResize));
    }

    void RendererSystem::setViewportSize(uint32_t width, uint32_t height) {
        width = std::max(1u, width);
        height = std::max(1u, height);

        if (width == m_viewport_width && height == m_viewport_height) {
            return;
        }

        m_viewport_width = width;
        m_viewport_height = height;

        if (m_viewport_framebuffer) {
            m_viewport_framebuffer->resize(width, height);
        }
    }

    ViewportOutput RendererSystem::getViewportOutput() const {
        ViewportOutput output;
        output.backend = getBackendType();
        output.width = m_viewport_width;
        output.height = m_viewport_height;

        if (!m_viewport_framebuffer) {
            return output;
        }

        output.kind = ViewportOutputKind::OpenGLTexture;
        output.coordinate_origin = ViewportCoordinateOrigin::BottomLeft;
        output.numeric_handle = m_viewport_framebuffer->getColorAttachmentRendererID(0);
        return output;
    }

    ViewportPickResult RendererSystem::pickViewport(const ViewportPickRequest& request) {
        ViewportPickResult result;
        result.supported = true;

        if (!m_viewport_framebuffer) {
            return result;
        }

        m_viewport_framebuffer->bind();
        result.entity_id = m_viewport_framebuffer->readPixel(1, request.x, request.y);
        m_viewport_framebuffer->unbind();
        result.ready = true;
        return result;
    }

    bool RendererSystem::onWindowResize(WindowResizeEvent& e) {
        Renderer::onWindowResize(e.getWidth(), e.getHeight());
        return false;
    }
} // namespace NexAur
