#include "pch.h"
#include "skybox_pass_v2.h"

#include "Function/File/file_system.h"
#include "Function/Renderer/RHI/render_device.h"
#include "Function/Renderer/RHI/renderer_command.h"

namespace NexAur {
    void SkyboxPassV2::execute(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) {
        (void)pass_context;

        glm::mat4 view = glm::mat4(glm::mat3(render_data.camera_data.view_matrix));
        glm::mat4 projection = render_data.camera_data.projection_matrix;
        glm::mat4 vp = projection * view;

        setSkyboxTexture(render_data.environment_data.skybox_texture);
        if (!m_skybox_texture) {
            return;
        }

        RendererCommand::setDepthFunc(DepthFunc::Lequal);
        m_shader->bind();
        m_shader->setInt("u_Skybox", 0);
        m_shader->setMat4("u_ViewProjection", vp);
        m_skybox_texture->bind(0);
        m_vertex_array->bind();
        RendererCommand::drawTriangles(m_vertex_array, 36);
        RendererCommand::setDepthFunc(DepthFunc::Less);
    }

    void SkyboxPassV2::initResources() {
        m_shader = RendererFactory::createShaderByPaths(
            "skybox shader",
            NX_ASSET("assets/shaders/skybox/skybox.vs"),
            NX_ASSET("assets/shaders/skybox/skybox.fs"));

        float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        auto vbo = RendererFactory::createVertexBuffer(skyboxVertices, sizeof(skyboxVertices));
        BufferLayout layout = {
            { ShaderDataType::Float3, "a_Position" }
        };
        vbo->setLayout(layout);

        m_vertex_array = RendererFactory::createVertexArray();
        m_vertex_array->addVertexBuffer(vbo);
    }
} // namespace NexAur
