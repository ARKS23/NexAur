#include "pch.h"
#include "triangle_test.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Renderer/RHI/renderer_system.h" // 为了用 RendererFactory

namespace NexAur {

    void TriangleTest::init() {
        // 1. VAO
        m_VertexArray = RendererFactory::createVertexArray();

        // 2. VBO
        float vertices[3 * 3] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f
        };
        auto vertexBuffer = RendererFactory::createVertexBuffer(vertices, sizeof(vertices));
        NX_CORE_INFO("Vertex Buffer created.");
        
        BufferLayout layout = {
            { ShaderDataType::Float3, "a_Position" }
        };
        vertexBuffer->setLayout(layout);
        m_VertexArray->addVertexBuffer(vertexBuffer);

        // 3. EBO
        uint32_t indices[3] = { 0, 1, 2 };
        auto indexBuffer = RendererFactory::createIndexBuffer(indices, 3);
        m_VertexArray->setIndexBuffer(indexBuffer);
        NX_CORE_INFO("Index Buffer created.");

        // 4. Shader
        std::string vertexSrc = R"(
            #version 450 core
            layout(location = 0) in vec3 a_Position;
            void main() {
                gl_Position = vec4(a_Position, 1.0);
            }
        )";

        std::string fragmentSrc = R"(
            #version 450 core
            layout(location = 0) out vec4 color;
            void main() {
                color = vec4(0.8, 0.3, 0.2, 1.0);
            }
        )";
        m_Shader = RendererFactory::createShader("Triangle", vertexSrc, fragmentSrc);

        if (!m_Shader) {
            NX_CORE_ERROR("Shader creation failed!");
        }
    }

    void TriangleTest::onUpdate(TimeStep ts) {
        if (!m_Shader || !m_VertexArray) return;

        // 这里不需要 clear，因为 RendererSystem 的 tick 里可能已经 clear 了
        // 如果没有，可以手动调 RenderingCommand::clear()
        
        m_Shader->bind();
        m_VertexArray->bind();
        RendererCommand::drawIndexed(m_VertexArray);
    }

}