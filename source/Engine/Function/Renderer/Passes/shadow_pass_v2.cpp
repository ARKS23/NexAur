#include "pch.h"
#include "shadow_pass_v2.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Global/global_context.h"
#include "Function/Renderer/window_system.h"
#include "Function/File/file_system.h"
#include <glm/gtc/matrix_transform.hpp>

namespace NexAur {
    ShadowPassV2::ShadowPassV2(const RenderPassSpecificationV2& spec) : IRenderPass(spec) {
        initResources();
    }

    void ShadowPassV2::initResources() {
        m_shader = RendererFactory::createShaderByPaths("shadow map shader v2", 
            NX_ASSET("assets/shaders/shadow/shadow_map.vs"), 
            NX_ASSET("assets/shaders/shadow/shadow_map.fs"));

        // 创建Depth Attachment的帧缓冲
        FramebufferSpecification framebuffer_spec;
        framebuffer_spec.width = SHADOW_WIDTH;
        framebuffer_spec.height = SHADOW_HEIGHT;
        FramebufferTextureSpecification depth_attachment_spec(FramebufferTextureFormat::Depth);
        depth_attachment_spec.texture_wrap = TextureWrap::ClampToBorder; // 超出部分使用纯色
        framebuffer_spec.Attachments.push_back(depth_attachment_spec);
        m_framebuffer = RendererFactory::createFramebuffer(framebuffer_spec);

        uint32_t depth_texture_id = m_framebuffer->getDepthAttachmentRendererID();
        m_depth_texture = RendererFactory::createTexture2D(depth_texture_id, SHADOW_WIDTH, SHADOW_HEIGHT);
    }

    void ShadowPassV2::run_without_begin_end(const RenderDataPacket& render_data) {
        glm::vec3 dir_light_direction = render_data.directional_light_data.direction;
        // if (glm::length(dir_light_direction) < 0.001f) 
        //     dir_light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
        dir_light_direction = glm::normalize(dir_light_direction);

        // 计算光源空间矩阵: 正交投影
        glm::mat4 light_projection = glm::ortho(ORTHO_LEFT, ORTHO_RIGHT, ORTHO_BOTTOM, ORTHO_TOP, ORTHO_NEAR, ORTHO_FAR);
        glm::mat4 light_view = glm::lookAt(-dir_light_direction * 20.0f, // 光源位置
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
        for (const RenderObjectData &object : render_data.opaque_objects) {
            if (!object.model_data) continue;   // 安全性检查
            m_shader->setMat4("u_Model", object.transform);
            for (const RenderMeshData &mesh : object.model_data->meshes) {
                if (!mesh.vertex_array) continue; // 安全性检查
                mesh.vertex_array->bind();
                RendererCommand::drawIndexed(mesh.vertex_array, mesh.index_count);
            }
        }
        
        // 恢复默认的缓冲和视口
        m_framebuffer->unbind();
        auto [width, height] = g_runtime_global_context.m_window_system->getWindowSize();
        RendererCommand::setViewport(0, 0, width, height);
        RendererCommand::clear(ClearBufferFlag::Depth);
    }
} // namespace NexAur
