#include "pch.h"
#include "triangle_pass.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Renderer/RHI/renderer_system.h"

namespace NexAur {
    void TrianglePass::execute() {
        if (m_shader && m_vertex_array) {
            m_shader->bind();
            m_vertex_array->bind();
            RendererCommand::drawIndexed(m_vertex_array);
        }
    }

    void TrianglePass::initResources() {
        m_vertex_array = RendererFactory::createVertexArray();
        float vertices[] = {
                -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f, // 左下 (红)
                 0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, // 右下 (绿)
                 0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f  // 顶 (蓝)
        };
        auto vbo = RendererFactory::createVertexBuffer(vertices, sizeof(vertices));
        vbo->setLayout({ { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float3, "a_Color" } });
        m_vertex_array->addVertexBuffer(vbo);

        uint32_t indices[] = { 0, 1, 2 };
        auto ebo = RendererFactory::createIndexBuffer(indices, 3);
        m_vertex_array->setIndexBuffer(ebo);

        // 2. 创建 Shader
        std::string vertSrc = R"(
            #version 450 core
            layout(location = 0) in vec3 a_Pos;
            layout(location = 1) in vec3 a_Color;
            out vec3 v_Color;
            void main() { v_Color = a_Color; gl_Position = vec4(a_Pos, 1.0); }
        )";
        std::string fragSrc = R"(
            #version 450 core
            in vec3 v_Color;
            out vec4 color;
            void main() { color = vec4(v_Color, 1.0); }
        )";
        m_shader = RendererFactory::createShader("GradientTriangle", vertSrc, fragSrc);
    }
} // namespace NexAur