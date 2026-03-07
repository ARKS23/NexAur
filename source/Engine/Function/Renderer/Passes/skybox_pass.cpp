#include "pch.h"
#include "skybox_pass.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Global/global_context.h"
#include "Function/File/file_system.h"
#include "Function/Renderer/camera.h"

namespace NexAur {
    void SkyboxPass::execute() {
        glm::mat4 view = glm::mat4(glm::mat3(m_camera->getView())); // 去除平移部分
        glm::mat4 projection = m_camera->getProjection();
        glm::mat4 vp = projection * view;

        RendererCommand::setDepthFunc(DepthFunc::Lequal); // 设置深度函数为小于等于，确保天空盒绘制在最远处
        m_shader->bind();
        m_shader->setInt("u_Skybox", 0);
        m_shader->setMat4("u_ViewProjection", vp);
        m_skybox_texture->bind(0);
        m_vertex_array->bind();
        RendererCommand::drawTriangles(m_vertex_array, 36);
        RendererCommand::setDepthFunc(DepthFunc::Less); // 恢复默认深度函数
    }

    void SkyboxPass::initResources() {
        m_shader = RendererFactory::createShaderByPaths("skybox shader", 
            NX_ASSET("assets/shaders/skybox/skybox.vs"), NX_ASSET("assets/shaders/skybox/skybox.fs"));

        float skyboxVertices[] = {
            // positions          
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

