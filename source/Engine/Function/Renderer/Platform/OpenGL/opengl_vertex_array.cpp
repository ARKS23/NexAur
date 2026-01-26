#include "pch.h"
#include "opengl_vertex_array.h"

namespace NexAur {
    static GLenum GLTypeFromShaderDataType(ShaderDataType type) {
        switch (type) {
            case ShaderDataType::Float:    return GL_FLOAT;
            case ShaderDataType::Float2:   return GL_FLOAT;
            case ShaderDataType::Float3:   return GL_FLOAT;
            case ShaderDataType::Float4:   return GL_FLOAT;
            case ShaderDataType::Mat3:     return GL_FLOAT;
            case ShaderDataType::Mat4:     return GL_FLOAT;
            case ShaderDataType::Int:      return GL_INT;
            case ShaderDataType::Int2:     return GL_INT;
            case ShaderDataType::Int3:     return GL_INT;
            case ShaderDataType::Int4:     return GL_INT;
            case ShaderDataType::Bool:     return GL_BOOL;
            default:
                NX_CORE_ERROR("Unsupported ShaderDataType!");
                return 0;
        }
    }

    static bool isMatrixType(ShaderDataType type) {
        return type == ShaderDataType::Mat3 || type == ShaderDataType::Mat4;
    }

    OpenGLVertexArray::OpenGLVertexArray() {
        glCreateVertexArrays(1, &m_ID);
    }

    OpenGLVertexArray::~OpenGLVertexArray() {
        glDeleteVertexArrays(1, &m_ID);
    }

    void OpenGLVertexArray::bind() const {
        glBindVertexArray(m_ID);
    }

    void OpenGLVertexArray::unbind() const {
        glBindVertexArray(0);
    }

    const std::vector<std::shared_ptr<VertexBuffer>>& OpenGLVertexArray::getVertexBuffers() const {
        return m_vertex_buffers;
    }

    const std::shared_ptr<IndexBuffer>& OpenGLVertexArray::getIndexBuffer() const {
        return m_index_buffer;
    }

    void OpenGLVertexArray::setIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) {
        this->bind();
        indexBuffer->bind();
        m_index_buffer = indexBuffer;
    }

    void OpenGLVertexArray::addVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) {
        NX_CORE_ASSERT(vertexBuffer->getLayout().getElements().size(), "Vertex Buffer has no layout!");
        
        this->bind();
        vertexBuffer->bind();

        // 解析layout: 逐个element设置vertexArrayPointer，内部的element的偏移量已经在layout中构造时预处理过
        const BufferLayout& layout = vertexBuffer->getLayout();
        for (const BufferElement& element : layout) {
            GLenum gl_type = GLTypeFromShaderDataType(element.type);
            NX_CORE_ASSERT(gl_type != 0, "Unsupported ShaderDataType in VertexBuffer!");

            if (gl_type == GL_FLOAT && !isMatrixType(element.type)) {
                glEnableVertexAttribArray(m_vertexbuffer_index);
                glVertexAttribPointer(
                    m_vertexbuffer_index,
                    element.getComponentCount(),
                    gl_type,
                    element.need_normalized ? GL_TRUE : GL_FALSE,
                    layout.getStride(),
                    (const void*)(uintptr_t)(element.Offset)
                );
            }
            else if (gl_type == GL_INT || gl_type == GL_BOOL) {
                glEnableVertexAttribArray(m_vertexbuffer_index);
                glVertexAttribIPointer( // 整数类型用I版本
                    m_vertexbuffer_index,
                    element.getComponentCount(),
                    gl_type,
                    layout.getStride(),
                    (const void*)(uintptr_t)(element.Offset)
                );
            }
            else if (isMatrixType(element.type)) {
                uint8_t count = element.getComponentCount(); // mat3 = 3, mat4 = 4
                for (uint8_t i = 0; i < count; ++i) {
                    glEnableVertexAttribArray(m_vertexbuffer_index);
                    glVertexAttribPointer(
                        m_vertexbuffer_index,
                        count,
                        gl_type,
                        element.need_normalized ? GL_TRUE : GL_FALSE,
                        layout.getStride(),
                        (const void*)(uintptr_t)(element.Offset + sizeof(float) * count * i)
                    );
                    glVertexAttribDivisor(m_vertexbuffer_index, 1); // 告诉OpenGL这是一个实例化的属性
                    m_vertexbuffer_index++;
                }
                continue; // 矩阵类型已经在循环内自增过了
            }

            m_vertexbuffer_index++;
        }

        m_vertex_buffers.push_back(vertexBuffer);
    }
} // namespace NexAur
