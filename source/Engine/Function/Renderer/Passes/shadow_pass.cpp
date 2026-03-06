#include "pch.h"
#include "shadow_pass.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Global/global_context.h"
#include "Function/Renderer/window_system.h"
#include "Function/File/file_system.h"
#include <glm/gtc/matrix_transform.hpp>

namespace NexAur {
    void ShadowPass::execute() {
        // 这个函数不使用，后续优化
    }

    ShadowPass::ShadowPass(const RenderPassSpecification& spec) : RenderPass(spec) {
        initResources();
    }

    void ShadowPass::initResources() {
        m_shader = RendererFactory::createShaderByPaths("shadow map shader", 
            NX_ASSET("assets/shaders/shadow/shadow_map.vs"), 
            NX_ASSET("assets/shaders/shadow/shadow_map.fs"));

        // 创建Depth Attachment的帧缓冲
        FramebufferSpecification framebuffer_spec;
        framebuffer_spec.width = SHADOW_WIDTH;
        framebuffer_spec.height = SHADOW_HEIGHT;
        FramebufferTextureSpecification depth_attachment_spec(FramebufferTextureFormat::Depth);
        framebuffer_spec.Attachments.push_back(depth_attachment_spec);
        m_framebuffer = RendererFactory::createFramebuffer(framebuffer_spec);

        uint32_t depth_texture_id = m_framebuffer->getDepthAttachmentRendererID();
        m_depth_texture = RendererFactory::createTexture2D(depth_texture_id, SHADOW_WIDTH, SHADOW_HEIGHT);
    }

    std::shared_ptr<Texture2D> ShadowPass::getDepthMapTexture() const {
        return m_depth_texture;
    }

    void ShadowPass::execute(std::shared_ptr<Scene> scene) {
        if (!scene) return;

        // 获取定向光
        DirectionalLight directional_light = scene->getDirectionalLight();

        // 计算光源空间矩阵,正交投影
        glm::mat4 light_projection = glm::ortho(-70.0f, 70.0f, -70.0f, 70.0f, 1.0f, 100.0f);
        glm::mat4 light_view = glm::lookAt(-directional_light.direction * 20.0f, // 光源位置
                                            glm::vec3(0.0f), // 目标点
                                            glm::vec3(0.0f, 1.0f, 0.0f)); // 上向量
        m_light_space_matrix = light_projection * light_view;

        // 绑定帧缓冲并设置视口
        m_framebuffer->bind();
        RendererCommand::setViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        RendererCommand::clear(ClearBufferFlag::Depth); // 阴影贴图只清空深度

        // shadow_map绘制场景物体阴影贴图
        m_shader->bind();
        m_shader->setMat4("u_LightSpaceMatrix", m_light_space_matrix);
        for (const RenderEntity& entity : scene->getEntities()) {
            m_shader->setMat4("u_Model", entity.transform);
            entity.mesh->bind();
            RendererCommand::drawIndexed(entity.mesh);
        }

        // 恢复默认的缓冲和视口
        m_framebuffer->unbind();
        auto [width, height] = g_runtime_global_context.m_window_system->getWindowSize();
        RendererCommand::setViewport(0, 0, width, height);
        RendererCommand::clear(ClearBufferFlag::Depth);
    }
} // namespace NexAur
