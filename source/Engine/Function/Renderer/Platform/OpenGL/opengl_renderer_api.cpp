#include "pch.h"
#include "opengl_renderer_api.h"
#include "Function/Renderer/RHI/definitions.h"

namespace NexAur {
    void OpenGLRendererAPI::init() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRendererAPI::setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        glViewport(x, y, width, height);
    }

    void OpenGLRendererAPI::setClearColor(const glm::vec4& color) {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRendererAPI::clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::drawIndex(const std::shared_ptr<VertexArray>& vertex_array, uint32_t index_count) {
        vertex_array->bind();
        uint32_t count = index_count ? index_count : vertex_array->getIndexBuffer()->getCount();
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLRendererAPI::drawLines(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count) {
        vertex_array->bind();
        glDrawArrays(GL_LINES, 0, vertex_count);
    }

    void OpenGLRendererAPI::drawTriangles(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count) {
        vertex_array->bind();
        glDrawArrays(GL_TRIANGLES, 0, vertex_count);
    }

    void OpenGLRendererAPI::setLineWidth(float width) {
        glLineWidth(width);
    }

    void OpenGLRendererAPI::setDepthFunc(DepthFunc depth_func) {
        switch (depth_func) {
            case DepthFunc::Less:
                glDepthFunc(GL_LESS);
                break;
            case DepthFunc::Lequal:
                glDepthFunc(GL_LEQUAL);
                break;
            case DepthFunc::Equal:
                glDepthFunc(GL_EQUAL);
                break;
            case DepthFunc::Gequal:
                glDepthFunc(GL_GEQUAL);
                break;
            case DepthFunc::Greater:
                glDepthFunc(GL_GREATER);
                break;
            case DepthFunc::NotEqual:
                glDepthFunc(GL_NOTEQUAL);
                break;
            case DepthFunc::Always:
                glDepthFunc(GL_ALWAYS);
                break;
            case DepthFunc::Never:
                glDepthFunc(GL_NEVER);
                break;
            default:
                NX_CORE_WARN("OpenGLRendererAPI::setDepthFunc: Unknown DepthFunc!");
                break;
        }
    }

    void OpenGLRendererAPI::setDepthMask(bool writable) {
        glDepthMask(writable ? GL_TRUE : GL_FALSE);
    }
} // namespace NexAur